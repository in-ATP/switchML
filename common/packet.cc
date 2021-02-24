#include "packet.h"
std::mutex _print_mutex;

void make_packet_and_copy_to(void* payload, uint64_t* key, uint32_t* len_tensor, uint32_t* workerID, uint8_t* num_worker, uint16_t* appID, uint16_t* agtr, uint16_t* seq_num, int32_t* data, bool isTerminated, bool isResend, int thread_id) {
    char* eth_ip_header = (char*) payload;
    memcpy(payload, IP_ETH_UDP_HEADER, sizeof(IP_ETH_UDP_HEADER));
    eth_ip_header[5] = thread_id;
    make_p4ml_layer_and_copy_to((char*)payload + sizeof(IP_ETH_UDP_HEADER), key, len_tensor, workerID, num_worker, appID, agtr, seq_num, data, isTerminated, isResend);
}

void make_p4ml_layer_and_copy_to(void* payload, uint64_t* key, uint32_t* len_tensor, uint32_t* workerID, uint8_t* num_worker, uint16_t* appID, uint16_t* agtr, uint16_t* seq_num, int32_t* data, bool isTerminated, bool isResend) {
    agghdr* agg_header = (agghdr*) payload;
    agghdr* p4ml_header = agg_header;
    agg_header->key = *key;
    agg_header->len_tensor = htonl(*len_tensor);
    agg_header->bitmap = htonl(1 << (*workerID));
    agg_header->num_worker = *num_worker;
    agg_header->appID = htons(*appID);
    agg_header->flag = 0;
    agg_header->agtr = htons(*agtr);
    agg_header->seq_num = htons(*seq_num);
    agg_header->versionIndex = htonl(*agtr * *num_worker + *workerID);
    // agg_header->last_ack = 0;

    // version = 1
    if (((*seq_num) % 200) > 100)
        agg_header->flag |= 32;
    if ((*seq_num) % 200 == 0)
        agg_header->flag |= 32;
    if (isTerminated)
        agg_header->flag |= 2;
    if (isResend)
        agg_header->flag |= 4;
    memcpy(agg_header->vector, data, sizeof(uint32_t) * (MAX_ENTRIES_PER_PACKET));
    // p4ml_header_print_h(agg_header, "Make");
}

void p4ml_header_ntoh(agghdr* p_p4ml) {
    p_p4ml->len_tensor = ntohl(p_p4ml->len_tensor);
    p_p4ml->bitmap = ntohl(p_p4ml->bitmap);
    p_p4ml->seq_num = ntohs(p_p4ml->seq_num);
    p_p4ml->agtr = ntohs(p_p4ml->agtr);
    p_p4ml->appID = ntohs(p_p4ml->appID);
    p_p4ml->versionIndex = ntohl(p_p4ml->versionIndex);
    // // p_p4ml->last_ack = ntohl(p_p4ml->last_ack);
    int32_t* p_model = p_p4ml->vector;
    for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
        p_model[i] = ntohl(p_model[i]);
}

void p4ml_header_ntoh_without_data(agghdr* p_p4ml) {
    p_p4ml->len_tensor = ntohl(p_p4ml->len_tensor);
    p_p4ml->bitmap = ntohl(p_p4ml->bitmap);
    p_p4ml->seq_num = ntohs(p_p4ml->seq_num);
    p_p4ml->agtr = ntohs(p_p4ml->agtr);
    p_p4ml->appID = ntohs(p_p4ml->appID);
    p_p4ml->versionIndex = ntohl(p_p4ml->versionIndex);
    // // p_p4ml->last_ack = ntohl(p_p4ml->last_ack);
    int32_t* p_model = p_p4ml->vector;
}

void p4ml_header_hton_without_data(agghdr* p_p4ml) {
    p_p4ml->len_tensor = htonl(p_p4ml->len_tensor);
    p_p4ml->bitmap = htonl(p_p4ml->bitmap);
    p_p4ml->seq_num = htons(p_p4ml->seq_num);
    p_p4ml->agtr = htons(p_p4ml->agtr);
    p_p4ml->appID = htons(p_p4ml->appID);
    p_p4ml->versionIndex = htonl(p_p4ml->versionIndex);
    // // p_p4ml->last_ack = htonl(p_p4ml->last_ack);
}

void p4ml_header_setACK(agghdr* p4ml_header) {
    p4ml_header->flag |= 1;
}

void p4ml_header_setTerminated(agghdr* p4ml_header) {
    p4ml_header->flag |= 2;
}

void p4ml_header_resetIndex(agghdr* p4ml_header) {
    p4ml_header->flag &= ~(16);	
}

void p4ml_header_print(agghdr* p4ml_header, char* caption) {
    std::lock_guard<std::mutex> lock(_print_mutex);
    printf("[%s] \n key: %" PRIu64 ", len_tensor: %u, "
        "bitmap: " BYTE_TO_BINARY_PATTERN ", num_worker: %u, appID: %u, "
        "agtr: %u seq_num: %u, versionIndex: %u, isACK: %d, version: %d,"
        "isResend: %d, data: ", caption, p4ml_header->key, p4ml_header->len_tensor, \
    BYTE_TO_BINARY(p4ml_header->bitmap), p4ml_header->num_worker, p4ml_header->appID,
    p4ml_header->agtr, p4ml_header->seq_num, p4ml_header->versionIndex, \
    p4ml_header->flag & 1?1:0, p4ml_header->flag & 32?1:0, p4ml_header->flag & 4?1:0);
    for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++) 
        printf("%d ", p4ml_header->vector[i]);
    printf("\n");
}

void p4ml_header_print_h(agghdr* p4ml_header, char* caption) {
    std::lock_guard<std::mutex> lock(_print_mutex);
    printf("[%s] \n key: %" PRIu64 ", len_tensor: %u, "
        "bitmap: " BYTE_TO_BINARY_PATTERN ", num_worker: %u, appID: %u, "
        "agtr: %u seq_num: %u, versionIndex: %u, isACK: %d, version: %d,"
        "isResend: %d, data: ", caption, p4ml_header->key, ntohl(p4ml_header->len_tensor), \
    BYTE_TO_BINARY(ntohl(p4ml_header->bitmap)), p4ml_header->num_worker, ntohs(p4ml_header->appID), \
    ntohs(p4ml_header->agtr), ntohs(p4ml_header->seq_num), ntohl(p4ml_header->versionIndex), \
    p4ml_header->flag & 1?1:0, p4ml_header->flag & 32?1:0, p4ml_header->flag & 4?1:0);
    for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++) 
        printf("%d ", ntohl(p4ml_header->vector[i]));
    printf("\n");
}
