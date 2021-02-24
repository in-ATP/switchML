#include "window_manager.h"

WindowManager::WindowManager() {
    last_ACK = 0;
}

bool WindowManager::UpdateWindow(uint16_t *seq_num){
    bool isLastAckUpdated = false; 
    isACKed[*seq_num] = true;
    while (isACKed[last_ACK + 1]) {
        last_ACK++;
        isLastAckUpdated = true;
    }
    return isLastAckUpdated;
}

int WindowManager::Reset(int packet_total) {
    last_ACK = 0;
    total_ACK = packet_total;
    memset(isACKed, 0, sizeof(bool) * packet_total + 1);
}