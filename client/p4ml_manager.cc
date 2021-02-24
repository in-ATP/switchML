#include "p4ml_manager.h"

#define CC_ENABLE false
#define LOSS_RECOVERY_ENABLE false
#define LOSS_RECOVERY_LOG true

uint32_t P4mlManager::host;
uint8_t P4mlManager::num_worker;
uint16_t P4mlManager::appID;
uint64_t P4mlManager::p4mlKey;
int P4mlManager::max_agtr_size_per_thread = 0;
int P4mlManager::_num_thread;

std::chrono::time_point<std::chrono::system_clock> P4mlManager::start;

ThreadInfo** P4mlManager::threadInfoQueue;
DMAcontext** P4mlManager::dmaContextQueue;
std::thread** P4mlManager::threadQueue;
std::queue<Job*> *P4mlManager::jobQueue;
std::queue<PacketObject> *P4mlManager::pendingQueue;
uint64_t* P4mlManager::weightQueue;
std::queue<uint64_t> P4mlManager::finishQueue;
WindowManager* P4mlManager::window_manager;

uint16_t* P4mlManager::hash_map;

int finish_thread = 0;
int loop_times[56] = {0};

int random_array[20][1024000] = {0};
bool isLoss[20][1024001] = {0};
bool isSeen[20][1024001] = {0};
int forwardCounter[20] = {0};

float mean[1000];
float median[1000];

bool P4mlManager::isForceForward = false;
int P4mlManager::forwardFrequency;
float P4mlManager::forwardRate;

std::mutex P4mlManager::_P4MLKey_mutex;
std::mutex P4mlManager::_print_mutex;
std::mutex P4mlManager::_queuePush_mutex;

std::chrono::high_resolution_clock::time_point recv_time;

P4mlManager::P4mlManager(uint32_t host, int num_worker, int appID) {
    srand(time(NULL));
    this->host = host;
    this->p4mlKey = 0;
    this->appID = (uint16_t) appID;
    this->num_worker = (uint8_t) num_worker;
}

void P4mlManager::updateModel(agghdr *p4ml_header, int32_t *data) {
    uint16_t *p_seq = &p4ml_header->seq_num;
    uint32_t *tensor_len = &p4ml_header->len_tensor;
    int32_t *p_model = p4ml_header->vector;
    uint32_t offset = (*p_seq - 1) * MAX_ENTRIES_PER_PACKET;
    if (offset < *tensor_len) {
        if (offset + MAX_ENTRIES_PER_PACKET > *tensor_len)
            memcpy(data + offset, p_model, sizeof(int32_t) * (*tensor_len % MAX_ENTRIES_PER_PACKET));
        else
            memcpy(data + offset, p_model, sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
    }
}

uint64_t P4mlManager::GetNewKey() {
    std::lock_guard<std::mutex> lock(_P4MLKey_mutex);
    return p4mlKey++;
}

int64_t P4mlManager::GetFinishKey() {
    if (!finishQueue.empty()) {
        std::lock_guard<std::mutex> lock(_queuePush_mutex);
        uint64_t tmp_key = finishQueue.front();
        finishQueue.pop();
        // printf("return completed task %d to outside\n", tmp_key);
        return tmp_key;
    } else {
        return -1;
    }
}

void P4mlManager::SetForceForward(float forward_rate) {
    isForceForward = true;
    forwardRate = forward_rate;
    if (forward_rate == 0.75) {
        forwardFrequency = 4;
        printf("\n No 0.75 supported, exit.\n");
        exit(1);
    } else {
        forwardFrequency = 1 / forward_rate;
        printf("\nSet force forward, frequency = %d\n\n", forwardFrequency);
    }
}

void P4mlManager::SetMaxAgtrSizePerThread(int agtr) {
    max_agtr_size_per_thread = agtr;
    printf("\nSet max_agtr_size_per_thread to %d...\n\n", agtr);
}

void P4mlManager::main_receive_packet_loop(DMAcontext *dma_context,
                                           int32_t *data,
                                           int my_id) {
    int msgs_completed;
    int total_resent = 0;
    std::queue<int> resent_queue;
    std::queue<int> trigger_queue;
    std::queue<int> loss_queue;
    int total_loss = 0;
    int total_dup_packet = 0;
    int total_last_tensor_packet = 0;
    int this_pos_to_send = max_agtr_size_per_thread;
    int resend_pos_to_send = dma_context->my_send_queue_length / 2;
    int total_packet = window_manager[my_id].total_ACK;
    int rand_index = 0;
    
    int window = max_agtr_size_per_thread;
    /* Loss simulation */
    int loss = 0;
    int resend_loss = 0;
    int timeout_loss = 0;

    int send_pointer = max_agtr_size_per_thread;

    int last_pending_front = 0;
    int pending_front_stuck_time = 0;

    bool resend_waiting = false;

    // int packet_processing_time_count = 0;
    // int packet_processing_time[35000];
    // float total_time = 0.0;

    memset(&isLoss[my_id], 0, sizeof(bool) * 32001);

    loop_times[my_id]++;

    int finish_window_seq = max_agtr_size_per_thread;
    Rogue rogue(max_agtr_size_per_thread);

    if(loop_times[my_id] % 1000 == 0)
	    fprintf(stderr, "loop_times[ %d ]  %d finished\n", my_id, loop_times[my_id]);
    
    // if(loop_times[my_id] %100 == 0) {
    //     for (int i = 0; i < _num_thread; i++)
    //         while (loop_times[i] < loop_times[my_id] - 50) {
    //             usleep(1);
    //         }
    // }

    char *send_region = (char *)dma_context->send_region;

    while (window_manager[my_id].last_ACK < window_manager[my_id].total_ACK) {
        cqe_snapshot_t cur_snapshot;
        // printf("window_manager[my_id].last_ACK: %d \n", window_manager[my_id].last_ACK);
        // struct ibv_wc wc_recv[POLLING_SIZE * 2];
        msgs_completed = 0;
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        while(1) {
            snapshot_cqe(&dma_context->mp_cqe_arr[dma_context->cqe_idx], cur_snapshot);
            const size_t delta = get_cycle_delta(dma_context->prev_snapshot, cur_snapshot);

            // msgs_completed = ibv_poll_cq(dma_context->receive_cq, POLLING_SIZE * 2, wc_recv);
            std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            if (!(delta == 0 || delta >= kAppNumRingEntries)) {
                msgs_completed = delta;
                // printf("[%d] msgs_completed:%d, break\n", my_id, msgs_completed);
                recv_time = std::chrono::high_resolution_clock::now();
                break;
            }
            // printf("msgs_completed: %d, %f\n", msgs_completed, time_span.count());
                // printf("[%d] loop, msgs_completed:%d, dma_context->total_received:%d, time_span:%f\n", my_id, msgs_completed, dma_context->total_received, time_span);
            if (LOSS_RECOVERY_ENABLE) {
                if(time_span.count() > 0.05 && msgs_completed == 0) {
                    uint16_t timeout_seq = window_manager[my_id].last_ACK + 1;
                    if (LOSS_RECOVERY_LOG)
                        printf("[thread %d] Timeout, send %d\n", my_id, timeout_seq);
                    // exit(1);
                    int offset = (timeout_seq - 1) * MAX_ENTRIES_PER_PACKET;
                    uint16_t switch_agtr_pos = threadInfoQueue[my_id]->agtr_start_pos  + ((timeout_seq - 1) % max_agtr_size_per_thread);
                    if (timeout_seq <= total_packet) {
                        // if(my_id == 0)
                        // printf("Seq %d trigger %d Resend!\n", p4ml_header->seq_num, timeout_seq);
                        // set Terminated if last packet
                        if (timeout_seq != total_packet) {
                            // for packet which integer < 32

                            /* Packet Loss simulation */
                            rand_index++;
                            if (random_array[my_id][rand_index] < timeout_loss) {
                                total_loss++;
                                rand_index++;
                                // printf("[Timeout] %d loss.\n", timeout_seq);
                                isLoss[my_id][timeout_seq] = true;
                                loss_queue.push(timeout_seq);
                            } else {
                                if (offset + MAX_ENTRIES_PER_PACKET > jobQueue[my_id].front()->len) {
                                    int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                    memcpy(tmp, data + offset, sizeof(int32_t) * (jobQueue[my_id].front()->len % MAX_ENTRIES_PER_PACKET));
                                    make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &timeout_seq, tmp, false, true);
                                } else {
                                    make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &timeout_seq, data + offset, false, true);
                                }
                            }
                        } else {
                            // for packet which integer < 32
                            if (offset + MAX_ENTRIES_PER_PACKET > jobQueue[my_id].front()->len) {
                                int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                memcpy(tmp, data + offset, sizeof(int32_t) * (jobQueue[my_id].front()->len % MAX_ENTRIES_PER_PACKET));
                                make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &timeout_seq, tmp, true, true);
                            } else {
                                make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &timeout_seq, data + offset, true, true);
                            }
                        }

                        // p4ml_header_print(p4ml_header, "RESEND TRIGGER");

                        send_packet(dma_context, P4ML_LAYER_SIZE, resend_pos_to_send);
                        resend_pos_to_send++;
                        total_resent++;
                        if (resend_pos_to_send == dma_context->my_send_queue_length - 1)
                            resend_pos_to_send = dma_context->my_send_queue_length / 2 + 1;
                        resent_queue.push(timeout_seq);
                        trigger_queue.push(timeout_seq);
                    }
                    t1 = std::chrono::high_resolution_clock::now();
                }
            }

            if(time_span.count() > 20.0  && msgs_completed == 0 && dma_context->total_received > 0) {
                fprintf(stderr, "Timeout happened this thread thread_id=%d, total_received=%d, total_sent=%d, total_loss=%d, total_resent=%d, last_ACK=%d, total_dup_recv=%d, total_last_tensor_packet_recv=%d, loop_time=%d\n", \
                    my_id, dma_context->total_received, dma_context->total_sent, total_loss, total_resent, window_manager[my_id].last_ACK, total_dup_packet, total_last_tensor_packet, loop_times[my_id]);
                fprintf(stderr, "%d is simulate loss packet [%d]\n", window_manager[my_id].last_ACK + 1, isLoss[my_id][window_manager[my_id].last_ACK+1]);

                printf("\nLoss\n");
                for (int i = 0 ; i < 32001; i++)
                    if (isLoss[my_id][i])
                        printf("%d ", i);
                printf("\nResend\n");
                
                while (!resent_queue.empty()) {
                    printf("%d[%d] ", resent_queue.front(), trigger_queue.front());
                    resent_queue.pop();
                    trigger_queue.pop();
                }
                printf("\nPendingQueue\n");
                while (!pendingQueue[my_id].empty()) {
                    printf("%d ", pendingQueue[my_id].front().p4ml_header->seq_num);
                    pendingQueue[my_id].pop();
                }
                printf("\nlast_ACK: %d \n", window_manager[my_id].last_ACK);
                
                
                for(int i=0; i< _num_thread; i++) 
                    fprintf(stderr, "Timeout happened  thread_id=%d, total_received=%d, total_sent=%d, loop_time=%d\n", i, dmaContextQueue[i]->total_received, dmaContextQueue[i]->total_sent, loop_times[i]);
                exit(-1);
            }
        }
        
       /* circle alignment */
        if (this_pos_to_send + max_agtr_size_per_thread + max_agtr_size_per_thread > dma_context->my_send_queue_length / 2)
            this_pos_to_send = 0;

 		int to_be_sent = 0;

        dma_context->total_received += msgs_completed;

        // printf("msgs_completed: %d, dma_context->total_received: %d\n", msgs_completed, dma_context->total_received);

        for (int msg = 0; msg < msgs_completed; msg++) {
        // std::chrono::high_resolution_clock::time_point packet_start = std::chrono::high_resolution_clock::now();
            uint8_t* buf = &dma_context->mp_recv_ring[dma_context->ring_head * kAppRingMbufSize];
                
            agghdr *p4ml_header = reinterpret_cast<agghdr*>(buf + IP_ETH_UDP_HEADER_SIZE);
            // p4ml_header_print_h(p4ml_header, "RECEIVE");
            p4ml_header_ntoh_without_data(p4ml_header);
            bool is_resend_packet = p4ml_header->flag & 0x04;
            bool is_ecn_mark_packet = p4ml_header->flag & 0x08;
            
            // If that is resend packet from last tensor, ignore it
            if (p4ml_header->key != jobQueue[my_id].front()->key) {
                total_last_tensor_packet++;
                dma_context->total_received--;

                dma_context->ring_head = (dma_context->ring_head + 1) % kAppNumRingEntries;
                dma_context->nb_rx_rolling++;
                if (dma_context->nb_rx_rolling == kAppStridesPerWQE) {
                    dma_context->nb_rx_rolling = 0;
                    int ret = dma_context->mp_wq_family->recv_burst(dma_context->mp_wq, &dma_context->mp_sge[dma_context->sge_idx], 1);
                    rt_assert(ret == 0);
                    dma_context->sge_idx = (dma_context->sge_idx + 1) % kAppRQDepth;
                }
                continue;
            }
            // If that is duplicate resend packet, ignore it
            if (window_manager[my_id].isACKed[p4ml_header->seq_num] && is_resend_packet) {
                total_dup_packet++;
                dma_context->total_received--;

                dma_context->ring_head = (dma_context->ring_head + 1) % kAppNumRingEntries;
                dma_context->nb_rx_rolling++;
                if (dma_context->nb_rx_rolling == kAppStridesPerWQE) {
                    dma_context->nb_rx_rolling = 0;
                    int ret = dma_context->mp_wq_family->recv_burst(dma_context->mp_wq, &dma_context->mp_sge[dma_context->sge_idx], 1);
                    rt_assert(ret == 0);
                    dma_context->sge_idx = (dma_context->sge_idx + 1) % kAppRQDepth;
                }
                continue;
            }

            // printf("packet %d receive\n", p4ml_header->seq_num);
            
            /* Receive Normal Packet */
            if (!window_manager[my_id].isACKed[p4ml_header->seq_num]) {
                dma_context->receive_time[p4ml_header->seq_num] = recv_time;
                /* Update Model */
                for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                    p4ml_header->vector[i] = ntohl(p4ml_header->vector[i]);
                updateModel(p4ml_header, data);

                /* Update Window */
                if (window_manager[my_id].UpdateWindow(&p4ml_header->seq_num)) {
                    resend_waiting = false;
                }

                /* If not Ready for next Seq sending, Enqueue */
                bool isEnqueue = false;
                uint16_t next_seq_num = p4ml_header->seq_num + window;
                int next_offset = (next_seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                // printf("next_seq_num: %d\n", next_seq_num);
    
                if (next_seq_num > window_manager[my_id].last_ACK + window) {
                        // printf("%d: next seq_num: %d enqueue! [%d ~ %d] \n", p4ml_header->seq_num, next_seq_num, window_manager[my_id].last_ACK, window_manager[my_id].last_ACK + window);
                        agghdr* enqueue_header = (agghdr*) malloc(sizeof(agghdr));
                        memcpy(enqueue_header, p4ml_header, sizeof(agghdr));
                        PacketObject pending_obj = {
                            .p4ml_header = enqueue_header,
                            // .wc_recv_id = wc_recv_id
                        };
                        isEnqueue = true;
                        pendingQueue[my_id].push(pending_obj);
                    // }
                }

                /* Send Next Packet */
                if (next_seq_num <= total_packet && next_seq_num <= window_manager[my_id].last_ACK + window && next_seq_num > send_pointer) {
                    // printf("next_seq_num: %d, send_pointer: %d\n", next_seq_num, send_pointer);
                    
                    bool ForceForward = false;
                    if (isForceForward) {
                        if (forwardCounter[my_id] == forwardFrequency) {
                            ForceForward = true;
                            forwardCounter[my_id] = 0;
                        } else {
                            forwardCounter[my_id]++;
                        }
                    }
                    
                    int packet_to_process = abs(next_seq_num - send_pointer);
                    // printf("packet to process: %d\n", packet_to_process);
                    // send more packet if window change
                    for (int i = packet_to_process - 1; i >= 0; i--) {
                        uint16_t process_next_seq_num = next_seq_num - i;

                        // printf("[%d] next_seq_num: %d, send_pointer: %d\n", p4ml_header->seq_num, process_next_seq_num, send_pointer);

                        uint16_t switch_agtr_pos = threadInfoQueue[my_id]->agtr_start_pos  + ((process_next_seq_num - 1) % max_agtr_size_per_thread);
                        // set Terminated if last packet
                        /* We don't loss Terminate packet here */
                        if (process_next_seq_num != total_packet) {
                            /* Packet Loss simulation */
                            rand_index++;
                            if (random_array[my_id][rand_index] < loss) {
                            // if (process_next_seq_num == 477  || process_next_seq_num == 478) {
                                total_loss++;
                                rand_index++;
                                // printf("%d loss.\n", process_next_seq_num);
                                isLoss[my_id][process_next_seq_num] = true;
                                to_be_sent--;
                                loss_queue.push(process_next_seq_num);
                            } else {
                                // for packet which integer < 32
                                if (next_offset + MAX_ENTRIES_PER_PACKET > p4ml_header->len_tensor) {
                                    int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                    memcpy(tmp, data + next_offset, sizeof(int32_t) * (p4ml_header->len_tensor % MAX_ENTRIES_PER_PACKET));
                                    make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &p4ml_header->key, &p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, tmp, false, ForceForward);
                                } else {
                                    make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &p4ml_header->key, &p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, data + next_offset, false, ForceForward);
                                }
                            }

                        } else {
                            // for packet which integer < 32
                            if (next_offset + MAX_ENTRIES_PER_PACKET > p4ml_header->len_tensor) {
                                int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                memcpy(tmp, data + next_offset, sizeof(int32_t) * (p4ml_header->len_tensor % MAX_ENTRIES_PER_PACKET));
                                make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &p4ml_header->key, &p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, tmp, true, ForceForward);
                            } else {
                                make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &p4ml_header->key, &p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, data + next_offset, true, ForceForward);
                            }
                        }

                        to_be_sent++;
                    }
                    send_pointer = next_seq_num;
                    // printf("send_pointer: %d\n", send_pointer);
                }

                int i = 0;
                /* Check If packet in Pending Queue is Ready to send */
                while (!pendingQueue[my_id].empty()) {
                    // printf("p4ml_header->seq_num: %d\n", p4ml_header->seq_num);
                    PacketObject pendingObj = pendingQueue[my_id].front();
                
                    i++;
                    agghdr* pending_p4ml_header = pendingObj.p4ml_header;
           
                    if (window_manager[my_id].last_ACK < pending_p4ml_header->seq_num) {
                        // printf("%d Get %d from pending queue and window_manager[my_id].last_ACK = %d, quit\n", p4ml_header->seq_num, pending_p4ml_header->seq_num, window_manager[my_id].last_ACK);
                        break;
                    }

                    uint16_t next_seq_num = pending_p4ml_header->seq_num + window;
                    int next_offset = (next_seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                    // printf("[pending] org: %d, next_number: %d\n", pending_p4ml_header->seq_num, next_seq_num);
                    if (next_seq_num <= window_manager[my_id].last_ACK + window && next_seq_num > send_pointer) {
                        
                        bool ForceForward = false;
                        if (isForceForward) {
                            if (forwardCounter[my_id] == forwardFrequency) {
                                ForceForward = true;
                                forwardCounter[my_id] = 0;
                            } else {
                                forwardCounter[my_id]++;
                            }
                        }

                        /* Copy to Send Region */
                        if (next_seq_num <= total_packet) {

                            int packet_to_process = abs(next_seq_num - send_pointer);
                            // printf("[pending] packet to process: %d\n", packet_to_process);
                            
                            for (int i = packet_to_process - 1; i >= 0; i--) {
                                uint16_t process_next_seq_num = next_seq_num - i;
                                // printf("[Pending] seq_num trigger %d send next seq_num: %d\n", p4ml_header->seq_num, next_seq_num);
                                uint16_t switch_agtr_pos = threadInfoQueue[my_id]->agtr_start_pos  + ((process_next_seq_num - 1) % max_agtr_size_per_thread);
                                // set Terminated if last packet
                                if (process_next_seq_num != total_packet) {
                                    /* Packet Loss simulation */
                                    rand_index++;
                                    if (random_array[my_id][rand_index] < loss) {
                                        total_loss++;
                                        rand_index++;
                                        // printf("[Pending] %d loss.\n", process_next_seq_num);
                                        isLoss[my_id][process_next_seq_num] = true;
                                        to_be_sent--;
                                        loss_queue.push(process_next_seq_num);
                                    } else {
                                        // for packet which integer < 32
                                        if (next_offset + MAX_ENTRIES_PER_PACKET > pending_p4ml_header->len_tensor) {
                                            int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                            memcpy(tmp, data + next_offset, sizeof(int32_t) * (pending_p4ml_header->len_tensor % MAX_ENTRIES_PER_PACKET));
                                            make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &pending_p4ml_header->key, &pending_p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, tmp, false, ForceForward);
                                        } else {
                                            make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &pending_p4ml_header->key, &pending_p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, data + next_offset, false, ForceForward);
                                        }
                                    }
                                } else {
                                    // for packet which integer < 32
                                    if (next_offset + MAX_ENTRIES_PER_PACKET > pending_p4ml_header->len_tensor) {
                                        int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                        memcpy(tmp, data + next_offset, sizeof(int32_t) * (pending_p4ml_header->len_tensor % MAX_ENTRIES_PER_PACKET));
                                        make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &pending_p4ml_header->key, &pending_p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, tmp, true, ForceForward);
                                    } else {
                                        make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, &pending_p4ml_header->key, &pending_p4ml_header->len_tensor, &host, &num_worker, &appID, &switch_agtr_pos, &process_next_seq_num, data + next_offset, true, ForceForward);
                                    }
                                }

                                to_be_sent++;
                            }
                            send_pointer = next_seq_num;
                            // printf("[pending] send_pointer: %d\n", send_pointer);
                        }
                        // printf("%d: next seq_num: %d dequeue! [%d ~ %d] \n", pending_p4ml_header->seq_num, next_seq_num, window_manager[my_id].last_ACK, window_manager[my_id].last_ACK + window);
                        free(pending_p4ml_header);
                        pendingQueue[my_id].pop();
                    } else {
                        // printf("%d: next seq_num: %d dequeue! [%d ~ %d] \n", pending_p4ml_header->seq_num, next_seq_num, window_manager[my_id].last_ACK, window_manager[my_id].last_ACK + window);
                        free(pending_p4ml_header);
                        pendingQueue[my_id].pop();
                    }
                }
            

                /* If force forward is configurated, expect will not packet loss */
                if (!isForceForward && LOSS_RECOVERY_ENABLE) {
                    if (!resend_waiting) {
                        // printf("resend_processed\n");
                        uint16_t resend_seq = 0;

                        if (!pendingQueue[my_id].empty()) {
                            PacketObject pendingObj = pendingQueue[my_id].front();
                            agghdr* pending_p4ml_header = pendingObj.p4ml_header;
                            // printf("%d, %d\n", pending_p4ml_header->seq_num, window_manager[my_id].last_ACK + 1);
                            if (window_manager[my_id].last_ACK < pending_p4ml_header->seq_num) {
                                resend_seq = window_manager[my_id].last_ACK + 1;
                            }
                        }

                        bool isResendLoss = false;
                        if (resend_seq) {
                            resend_waiting = true;
                            int offset = (resend_seq - 1) * MAX_ENTRIES_PER_PACKET;
                            uint16_t switch_agtr_pos = hash_map[threadInfoQueue[my_id]->agtr_start_pos  + ((resend_seq - 1) % max_agtr_size_per_thread)];
                            if (resend_seq <= total_packet) {
                                if (LOSS_RECOVERY_LOG)
                                    printf("[Thread %d] Seq %d trigger %d Resend!\n", my_id, p4ml_header->seq_num, resend_seq);
                                // exit(1);
                                // set Terminated if last packet
                                if (resend_seq != total_packet) {
                                    // for packet which integer < 32

                                    /* Packet Loss simulation */
                                    rand_index++;
                                    if (random_array[my_id][rand_index] < resend_loss) {
                                        total_loss++;
                                        rand_index++;
                                        // printf("[Resend] %d loss.\n", resend_seq);
                                        isLoss[my_id][resend_seq] = true;
                                        loss_queue.push(resend_seq);
                                        isResendLoss = true;
                                    } else {
                                        if (offset + MAX_ENTRIES_PER_PACKET > jobQueue[my_id].front()->len) {
                                            int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                            memcpy(tmp, data + offset, sizeof(int32_t) * (jobQueue[my_id].front()->len % MAX_ENTRIES_PER_PACKET));
                                            make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &resend_seq, tmp, false, true);
                                        } else {
                                            make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &resend_seq, data + offset, false, true);
                                        }
                                    }
                                } else {
                                    // for packet which integer < 32
                                    if (offset + MAX_ENTRIES_PER_PACKET > jobQueue[my_id].front()->len) {
                                        int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                                        memcpy(tmp, data + offset, sizeof(int32_t) * (jobQueue[my_id].front()->len % MAX_ENTRIES_PER_PACKET));
                                        make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &resend_seq, tmp, true, true);
                                    } else {
                                        make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send * P4ML_LAYER_SIZE), &jobQueue[my_id].front()->key, &jobQueue[my_id].front()->len, &host, &num_worker, &appID, &switch_agtr_pos, &resend_seq, data + offset, true, true);
                                    }
                                }
                                // p4ml_header_print(p4ml_header, "RESEND TRIGGER");

                                if (!isResendLoss) {
                                    // int stop;
                                    // scanf("%d", &stop);
                                    send_packet(dma_context, P4ML_LAYER_SIZE, resend_pos_to_send);
                                    resend_pos_to_send++;
                                    total_resent++;
                                    if (resend_pos_to_send == dma_context->my_send_queue_length - 1)
                                        resend_pos_to_send = dma_context->my_send_queue_length / 2 + 1;
                                }
                                resent_queue.push(resend_seq);
                                trigger_queue.push(p4ml_header->seq_num);

                                // if (!isLoss[my_id][resend_seq]) {
                                //     printf("%d Not suppose to be lost\n", resend_seq);
                                //     printf("\nLoss\n");
                                //     for (int i = 0 ; i < 32000; i++)
                                //         if (isLoss[my_id][i])
                                //             printf("%d ", i);
                                //     printf("\nResend\n");
                                
                                //     while (!resent_queue.empty()) {
                                //         printf("%d[%d] ", resent_queue.front(), trigger_queue.front());
                                //         resent_queue.pop();
                                //         trigger_queue.pop();
                                //     }
                                //     printf("\nPendingQueue\n");
                                //     while (!pendingQueue[my_id].empty()) {
                                //         printf("%d ", pendingQueue[my_id].front().p4ml_header->seq_num);
                                //         pendingQueue[my_id].pop();
                                //     }
                                //     printf("\n");

                                //     printf("last_ACK: %d\n", window_manager[my_id].last_ACK);
                                //     exit(1);
                                // }

                            }
                        }
                    }
                }

                if (CC_ENABLE) {
                    if (p4ml_header->seq_num == finish_window_seq) {
                        // if (is_ecn_mark_packet)
                            // printf("ECN mark found, adjust window.\n");
                        
                        // rogue.packets_acked(time_in_us);
                        // int new_window = rogue.cc_avoid();
                        int new_window = rogue.adjustWindow(is_ecn_mark_packet);
                        // if (my_id == 0 && new_window != 100)
                        // printf("new_window: %d\n", new_window);

                        if (send_pointer + new_window > window_manager[my_id].total_ACK) 
                            window = window_manager[my_id].total_ACK - send_pointer;
                        else 
                            window = new_window;
                        //TODO: ADJUST WINDOW
                        finish_window_seq += window;
                    }
                }
            }

            dma_context->ring_head = (dma_context->ring_head + 1) % kAppNumRingEntries;
            dma_context->nb_rx_rolling++;
            if (dma_context->nb_rx_rolling == kAppStridesPerWQE) {
                dma_context->nb_rx_rolling = 0;
                int ret = dma_context->mp_wq_family->recv_burst(dma_context->mp_wq, &dma_context->mp_sge[dma_context->sge_idx], 1);
                rt_assert(ret == 0);
                dma_context->sge_idx = (dma_context->sge_idx + 1) % kAppRQDepth;
            }
            // std::chrono::high_resolution_clock::time_point packet_end = std::chrono::high_resolution_clock::now();
            // std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(packet_end - packet_start);
            // double time = time_span.count();
            // packet_processing_time[packet_processing_time_count] = time;
            // packet_processing_time_count++;
            // total_time += time;
            // if (packet_processing_time_count == 32000) {
            //     std::sort(packet_processing_time, packet_processing_time+32000);
            //     float mean = total_time/32000.0;
            //     float median = packet_processing_time[16000];
            //     printf("mean:%.10f, madian: %.10f.\n", mean, median);
            // }
        }

        dma_context->prev_snapshot = cur_snapshot;
        dma_context->cqe_idx = (dma_context->cqe_idx + 1) % kAppRecvCQDepth;

        if (msgs_completed < 0) {
				printf("Polling error\n");
				exit(1);
			}

		if (msgs_completed > 0 && to_be_sent) {
            send_packet(dma_context, P4ML_LAYER_SIZE * to_be_sent, this_pos_to_send);
			this_pos_to_send += to_be_sent;
		}

    }

    // if (my_id == 0) {
    //     printf("packet_processing_time_count: %d\n", packet_processing_time_count);
    //     for (int i = 0; i < packet_processing_time_count; i++)
    //         printf("%d\n", packet_processing_time[i]);
    //     // if (packet_processing_time_count == 32000) {
    //     //     std::sort(packet_processing_time, packet_processing_time+32000);
    //     //     float mean = total_time/32000.0;
    //     //     float median = packet_processing_time[16000];
    //     //     printf("mean:%.10f, madian: %.10f.\n", mean, median);
    //     // }
    // }
    dma_context->total_sent = 0;
    dma_context->total_received = 0;

    if (!pendingQueue[my_id].empty()){
        printf("PENDING QUEUE NOT EMPTY AFTER DONE.\n");
        while (!pendingQueue[my_id].empty()) {
            printf("%d ", pendingQueue[my_id].front().p4ml_header->seq_num);
            pendingQueue[my_id].pop();
        }
        printf("pendingQueue[my_id].size: %zu\n", pendingQueue[my_id].size());
        exit(1);
    }

    
    // printf("\nLoss\n");
    // for (int i = 0 ; i < 32000; i++)
    //     if (isLoss[my_id][i])
    //         printf("%d ", i);
    // printf("\nResend\n");
    
    // while (!resent_queue.empty()) {
    //     printf("%d[%d] ", resent_queue.front(), trigger_queue.front());
    //     resent_queue.pop();
    //     trigger_queue.pop();
    // }
    // printf("\nPendingQueue\n");
    // while (!pendingQueue[my_id].empty()) {
    //     printf("%d ", pendingQueue[my_id].front().p4ml_header->seq_num);
    //     pendingQueue[my_id].pop();
    // }
    // printf("\n");

                        
    // if (my_id == 0)
    // fprintf(stderr, "[Finish log] thread_id=%d, total_received=%d, total_sent=%d, total_loss=%d, total_resent=%d, last_ACK=%d, total_dup_recv=%d, total_last_tensor_packet_recv=%d\n", \
        my_id, dma_context->total_received, dma_context->total_sent, total_loss, total_resent, window_manager[my_id].last_ACK, total_dup_packet, total_last_tensor_packet);
}

void P4mlManager::PushPull(uint64_t key, char *data, int len, int cmd) {
    Job* job = new Job {
        .key = key,
        .data = (int32_t*) data,
        .len = (uint32_t) len,
        .cmd = cmd
    };

    /* Load Balance */
    uint64_t smallestWeight = weightQueue[0];
    int queueToGo = 0;
    for (int i = 1; i < _num_thread; i++) {
        if (weightQueue[i] < smallestWeight) {
            smallestWeight = weightQueue[i];
            queueToGo = i;
        }
    }

    /* If someone overflow, all minus the smallest one */
    if (weightQueue[queueToGo] > UINT64_MAX - len) 
        for (int i = 0; i < _num_thread; i++) 
            weightQueue[i] = weightQueue[i] - weightQueue[queueToGo];
    


    weightQueue[queueToGo] += len;
    // printf("% "PRIu64" \n", weightQueue[queueToGo]);
    // printf("Job %d Get, Send to Queue %d.\n", key, queueToGo);
    jobQueue[queueToGo].push(job);
}


void P4mlManager::init_threadPool(int num_thread) {
    _num_thread = num_thread;
    // max_agtr_size_per_thread = MAX_AGTR_COUNT / num_thread;
    /* Let fix each thread use 800 agtr */
    if (!max_agtr_size_per_thread)
        max_agtr_size_per_thread = 100;
    printf("max_agtr_size_per_thread: %d\n", max_agtr_size_per_thread);
    threadInfoQueue = new ThreadInfo*[num_thread];
    dmaContextQueue = new DMAcontext*[num_thread];
    weightQueue = new uint64_t[num_thread]();
    threadQueue = new std::thread*[num_thread];
    jobQueue = new std::queue<Job*>[num_thread];
    pendingQueue = new std::queue<PacketObject>[num_thread];
    window_manager = new WindowManager[num_thread];
    hash_map = new uint16_t[num_thread * max_agtr_size_per_thread];

    printf("Genereating random numbers...\n");
    for (int i = 0; i < num_thread; i++)
        for (int j = 0; j < 1024000; j++)
            random_array[i][j] = rand() % 10000;

    // Start from zero
    for (int i = 0; i < num_thread * max_agtr_size_per_thread; i++)
        hash_map[i] = hash_by(i, appID);

    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get devices list");
        exit(1);
    }

    ib_dev = dev_list[1];
    if (!ib_dev) {
        fprintf(stderr, "IB device not found\n");
        exit(1);
    }

    for (int i = 0; i < num_thread; i++) {
        threadInfoQueue[i] = new ThreadInfo {
            .thread_id = i,
            .agtr_start_pos = max_agtr_size_per_thread * i,
        };
        dmaContextQueue[i] = DMA_create(ib_dev, max_agtr_size_per_thread, i);
        threadQueue[i] = new std::thread(Start, i);
        window_manager[i].isACKed = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->isMarkTimeStamp = false;
        dmaContextQueue[i]->isSent = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->send_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->receive_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
    }

    printf("using: %s\n", ibv_get_device_name(ib_dev)); 

}

void P4mlManager::Start(int thread_id) {
    bindingCPU(thread_id + 16);
    ThreadInfo *thread_info = threadInfoQueue[thread_id];
    DMAcontext *dma_context = dmaContextQueue[thread_id];
    int agtr_size = max_agtr_size_per_thread;
    int my_id = thread_id;
    int agtr_start_pos = thread_info->agtr_start_pos;
    char *send_region = (char *)dma_context->send_region;

    while (1) {
        if (!jobQueue[thread_id].empty()) {
            Job* job = jobQueue[thread_id].front();
            uint64_t key = job->key;
            int32_t* data = job->data;
            uint32_t tensor_len = job->len;
          	// fprintf(stderr, "%lld: thread_id=%d, tensor_len=%d, agg_size=%d\n", key, thread_id, tensor_len, agtr_size); 

            int total_packet = ceil((float)tensor_len / MAX_ENTRIES_PER_PACKET);
            window_manager[thread_id].Reset(total_packet);
            memset(dma_context->isSent, 0, sizeof(bool) * window_manager[my_id].total_ACK + 1);

            for (int i = 0; i < tensor_len; i++)
                data[i] = htonl(data[i]);
            
            // SEQ number start from 1
            uint16_t seq_num = 0;

            int num_first_time_sending;
            if (max_agtr_size_per_thread * MAX_ENTRIES_PER_PACKET > tensor_len)
                num_first_time_sending = ceil((float)tensor_len / MAX_ENTRIES_PER_PACKET);
            else 
                num_first_time_sending = max_agtr_size_per_thread;

            // the first round sending
            for (int i = 0; i < num_first_time_sending; i++) {
                // usleep(rand() % 10 + 1);
                seq_num++;
                int offset = (seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                uint16_t switch_agtr_pos = hash_map[agtr_start_pos + i];
                // This thread have first time sending
                if (seq_num <= total_packet) {

                    bool ForceForward = false;
                    if (isForceForward) {
                        if (forwardRate == 0.75) {
                            if (!(seq_num % forwardFrequency == 0)) {
                                ForceForward = true;
                            }
                        } else {
                            if (seq_num % forwardFrequency == 0) {
                                ForceForward = true;
                            }
                        }
                    }

                    // set Terminated if last packet
                    if (seq_num != total_packet) {
                        // for packet which integer < 32
                        if (offset + MAX_ENTRIES_PER_PACKET > tensor_len) {
                            int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                            memcpy(tmp, data + offset, sizeof(int32_t) * (tensor_len % MAX_ENTRIES_PER_PACKET));
                            make_p4ml_layer_and_copy_to(send_region + P4ML_LAYER_SIZE * i, &key, &tensor_len, &host, &num_worker, &appID, &switch_agtr_pos, &seq_num, tmp, false, ForceForward);
                        } else {
                            make_p4ml_layer_and_copy_to(send_region + P4ML_LAYER_SIZE * i, &key, &tensor_len, &host, &num_worker, &appID, &switch_agtr_pos, &seq_num, data + offset, false, ForceForward);
                        }
                    } else {
                        // for packet which integer < 32
                        if (offset + MAX_ENTRIES_PER_PACKET > tensor_len) {
                            int32_t *tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
                            memcpy(tmp, data + offset, sizeof(int32_t) * (tensor_len % MAX_ENTRIES_PER_PACKET));
                            make_p4ml_layer_and_copy_to(send_region + P4ML_LAYER_SIZE * i, &key, &tensor_len, &host, &num_worker, &appID, &switch_agtr_pos, &seq_num, tmp, true, ForceForward);
                        } else {
                            make_p4ml_layer_and_copy_to(send_region + P4ML_LAYER_SIZE * i, &key, &tensor_len, &host, &num_worker, &appID, &switch_agtr_pos, &seq_num, data + offset, true, ForceForward);
                        }
                    }
                } else {
                }
            }

            for(int j = 0;j < num_first_time_sending; j++){
                send_packet(dma_context, P4ML_LAYER_SIZE, j);
            }
            
            // send_packet(dma_context, P4ML_LAYER_SIZE * num_first_time_sending, 0);
            
            main_receive_packet_loop(dma_context, data, my_id);

            // for (int i = 0; i < tensor_len / 32; i++) {
            //     printf("%d: ", i);
            //     for (int j = 0; j < 32; j++) {
            //             printf("%d ", data[i*32 + j]);
            //     }
            //     printf("\n");
            // }

            /* For Per Packet */
            // if (thread_id == 0) {
            // // printf("######################## %d ##########################\n", loop_times[my_id]);
            // // // printf("Last 32 int: \n");
            // // // // for (int i = tensor_len - 32; i < tensor_len; i++)
            // for (int i = 0; i < tensor_len; i++)
            //     printf("%d ", data[i]);
            // // // printf("\n");
            // double total_time = 0.0;
            // double time[32001] = {0.0};
            // double min_value = 100000.0;
            // if (thread_id == 0) {
            //     for (int i = 1; i <= total_packet; i++) {
            //         std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(dma_context->receive_time[i] - dma_context->send_time[i]);
            //         time[i] = time_span.count();
            //         printf("%d: %lf\n", i, time[i]);
            //         total_time += time[i];
            //         if (time[i] < min_value)
            //             min_value = time[i];
            //         // if (isLoss[my_id][i])
            //         //     printf(" [Loss Packet]\n");
            //         // else
            //         //     printf("\n");
            //         // // printf("totaltime: %lf\n", total_time);
            //     }
            //     std::sort(time+1, time+32001);
            //     // // printf("\n###################################################\n");
            //     // // for (int i = 1; i <= total_packet; i++) {
            //     // //     printf("%lf ", time[i]);
            //     // // }
            //     // // printf("\n");
            //     mean[loop_times[my_id]] = total_time/32000.0;
            //     median[loop_times[my_id]] = time[16000];
            //     printf("mean: %lf, median: %lf, min_value: %lf\n", total_time/32000.0, time[16000], min_value);
            // }
            // if (loop_times[my_id] == 1000)
            //     printf("median: %lf\n", median[500]);
            // }

            /* For Throughput */
            // if (thread_id == 0) {
            // printf("######################## %d ##########################\n", loop_times[my_id]);
            // // // printf("Last 32 int: \n");
            // // // // for (int i = tensor_len - 32; i < tensor_len; i++)
            // // // for (int i = 0; i < tensor_len; i++)
            // // //     printf("%d ", data[i]);
            // // // printf("\n");
            // double total_time = 0.0;
            // double time[32001] = {0.0};
            // for (int i = 1; i <= total_packet; i++) {
            //     std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(window_manager[my_id].receive_time[i] - start_time);
            //     time[i] = time_span.count();
            //     printf("%d: %lf, Throughput: %lf", i, time[i], ((float)P4ML_PACKET_SIZE * i / 1024 / 1024 / 1024 * 8) / time[i]);
            //     total_time += time[i];
            //     if (isLoss[my_id][i])
            //         printf(" [Loss Packet]\n");
            //     else
            //         printf("\n");
            //     // printf("totaltime: %lf\n", total_time);
            // }
            // std::sort(time+1, time+32001);
            // printf("\n###################################################\n");
            // // for (int i = 1; i <= total_packet; i++) {
            // //     printf("%lf ", time[i]);
            // // }
            // printf("\n");
            // printf("mean: %lf, median: %lf\n", total_time/32000.0, time[16000]);
            // }

            jobQueue[thread_id].pop();
            {
                std::lock_guard<std::mutex> lock(_queuePush_mutex);
                // printf("%d to Finish Queue\n", key);
                finishQueue.push(key);
            }
        }
        usleep(1);
    }
    
}
