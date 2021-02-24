#ifndef SLIDING_W_H
#define SLIDING_W_H

#include "packet.h"
#include "Rogue.h"
#define RESEND_TRIGGER 1

class WindowManager {
    public:
        bool* isACKed;
        /* This three variable is completely useless, but
        when deleting it, the performance will drop from 46Gbps to 40Gbps.. */
        bool* isSent;
        std::chrono::high_resolution_clock::time_point* send_time;
        std::chrono::high_resolution_clock::time_point* receive_time;
        /* */
        int total_ACK;
        int last_ACK;

        WindowManager();
        bool UpdateWindow(uint16_t *num);
        int Reset(int packet_total);

};

#endif