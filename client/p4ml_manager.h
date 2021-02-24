
#ifndef P4ML_MANAGER_H
#define P4ML_MANAGER_H

#include <stdlib.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <random>
#include <ctime>
#include <queue>
#include <assert.h>
#include <limits.h>
#include <bits/stdc++.h>
#include <netinet/ip.h> 
#include "../common/utils.h"
#include "../common/dma_common.h"
#include "../common/packet.h"
#include "../common/window_manager.h"

#define P4ML_KEY_TOTAL 500000
#define MAX_TENSOR_SIZE 1024000

struct ThreadInfo {
    int thread_id;
    int agtr_start_pos;
};

struct Job {
    uint64_t key;
    int32_t* data;
    uint32_t len;
    int cmd;
};

struct PacketObject {
    agghdr* p4ml_header;
};

class P4mlManager {
    public:

        P4mlManager(uint32_t host, int num_worker, int appID);
        // ~P4mlManager();
        
        void init_threadPool(int num_thread);
        void PushPull(uint64_t key, char* data, int len, int cmd);
        static void Start(int thread_id);
        
        uint64_t GetNewKey();
        int64_t GetFinishKey();
        void SetForceForward(float forward_rate);
        void SetMaxAgtrSizePerThread(int max_agtr_size_per_thread);

    private:
        static uint32_t host;
        static uint8_t num_worker;
        static uint16_t appID;
        static uint64_t p4mlKey;
        static int max_agtr_size_per_thread;
        static int _num_thread;
        static std::chrono::time_point<std::chrono::system_clock> start;
        static ThreadInfo** threadInfoQueue;
        static DMAcontext** dmaContextQueue;
        static std::thread** threadQueue;
        static std::queue<Job*> *jobQueue;
        static WindowManager* window_manager;
        static std::queue<uint64_t> finishQueue;
        static std::queue<PacketObject> *pendingQueue;
        static uint64_t* weightQueue;

        static uint16_t* hash_map;
        
        static bool isForceForward; 
        static int forwardFrequency;
        static float forwardRate;

        static std::mutex Resource_mutex;
        static std::mutex _P4MLKey_mutex;
        static std::mutex _print_mutex;
        static std::mutex _queuePush_mutex;

        static void main_receive_packet_loop(DMAcontext* dma_context, int32_t* data, int my_id);
        static void updateModel(agghdr* p4ml_header, int32_t* data);
};

#endif //P4ML_MANAGER_H