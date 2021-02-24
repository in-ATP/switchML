#ifndef PACKET_P4ML_H
#define PACKET_P4ML_H
#include <stdint.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <mutex>
#include <inttypes.h>
#include <iostream>
#include <bitset>
#include "utils.h"

#define DST_MAC 0x00, 0x01, 0x02, 0x03, 0x04, 0x05
// #define SRC_MAC 0xb8, 0x59, 0x9f, 0x1d, 0x04, 0xf2 
#define SRC_MAC 0xe4, 0x1d, 0x2d, 0xf3, 0xdd, 0xcc
// #define DST_MAC 0xb8, 0x59, 0x9f, 0x0b, 0x30, 0x72

#define ETH_TYPE 0x07, 0x00

#define IP_HDRS 0x45, 0x00, 0x00, 0x54, 0x00, 0x00, 0x40, 0x00, 0x40, 0x01, 0xaf, 0xb6

#define SRC_IP 0x0d, 0x07, 0x38, 0x66

#define DST_IP 0x0d, 0x07, 0x38, 0x7f

#define SRC_PORT 0x67, 0x67

#define DST_PORT 0x78, 0x78

#define UDP_HDRS 0x00, 0x00, 0x00, 0x00

// Only a template, DST_IP will be modified soon
const unsigned char IP_ETH_UDP_HEADER[] = { DST_MAC, SRC_MAC, ETH_TYPE, IP_HDRS, SRC_IP, DST_IP };

// P4ML_PACKET_SIZE = IP_ETH_HEADER_SIZE + P4ML_HEADER_SIZE + P4ML_DATA_SIZE
#define P4ML_PACKET_SIZE 190
#define P4ML_DATA_SIZE 128
#define P4ML_HEADER_SIZE 28
#define P4ML_LAYER_SIZE 156
#define IP_ETH_UDP_HEADER_SIZE 34

#define MAX_ENTRIES_PER_PACKET 32

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

#pragma pack(push, 1)
    struct agghdr {
        uint64_t key;
        uint32_t len_tensor;
        uint32_t bitmap;
        uint8_t num_worker;
        uint8_t flag;
        uint16_t agtr;
        uint16_t appID;
        uint16_t seq_num;
        uint32_t versionIndex;
        int32_t vector[MAX_ENTRIES_PER_PACKET];
};
#pragma pack(pop)

void make_packet_and_copy_to(void* payload, uint64_t* key, uint32_t* len_tensor, uint32_t* workerID, uint8_t* agtr_time, uint16_t* appID, uint16_t* agtr, uint16_t* seq_num, int32_t* data, bool isTerminated, bool isResend, int thread_id);
void make_p4ml_layer_and_copy_to(void* payload, uint64_t* key, uint32_t* len_tensor, uint32_t* workerID, uint8_t* agtr_time, uint16_t* appID, uint16_t* agtr, uint16_t* seq_num, int32_t* data, bool isTerminated, bool isResend);
void p4ml_header_ntoh(agghdr* p_p4ml);
void p4ml_header_ntoh_without_data(agghdr* p_p4ml);
void p4ml_header_hton_without_data(agghdr* p_p4ml);
void p4ml_header_setACK(agghdr* p_p4ml);
void p4ml_header_setTerminated(agghdr* p4ml_header);
void p4ml_header_setLastDstMAC(int filter_id);
void p4ml_header_print(agghdr* p4ml_header,char* caption);
void p4ml_header_print_h(agghdr* p4ml_header, char* caption);


#endif
