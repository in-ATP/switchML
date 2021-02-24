#ifndef ROUGE_H
#define ROUGE_H

#include<stdio.h>
#include<stdint.h>
#include<iostream>
#include"packet.h"

using namespace std;
#define do_div(n,base) ({                                      \
                uint32_t __base = (base);                               \
                uint32_t __rem;                                         \
                __rem = ((uint64_t)(n)) % __base;                       \
                (n) = ((uint64_t)(n)) / __base;                         \
                __rem;                                                  \
                })
#define GET_MIN(a,b) 		(a<b?a:b)
#define GET_MAX(a,b) 		(a>b?a:b)

class Rogue{
	
	public:
		Rogue(int init_window);	
		int cc_avoid();
		void packets_acked(int sample_rtt);
		int tcp_in_slow_start(); 
		uint64_t  GetCwndBytes(){ return cwnd_bytes;}
		void PrintStats();
		int tcp_current_ssthresh();
		int tcp_vegas_ssthresh();
		int no_packet_drop_cnt;
		int adjustWindow(bool isECN);
	private:
		uint16_t     cntRTT;         /* # of RTTs measured within last RTT */
		uint32_t     minRTT;         /* min of RTTs measured within last RTT (in usec) */
		uint32_t     baseRTT;        /* the min of all Vegas RTT measurements seen (in usec) */
		uint32_t     baseRTT_update_pacing;	
		uint64_t 	cwnd_bytes; 
		uint32_t 	ss_thresh;
		int 		diff;
		int 		update_state;
		int 	uid;
};

#endif