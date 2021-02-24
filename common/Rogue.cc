#include "Rogue.h"
#define INIT_SS_THRESH 100 * P4ML_PACKET_SIZE
#define BASE_RTT (6)

#define MAX_BYTES 100 * P4ML_PACKET_SIZE

static int alpha = 2;
static int beta = 4;
static int gamma_vegas = 1;

Rogue::Rogue(int init_window)
{
    baseRTT = 0x7fffffff;
    minRTT = 0x7fffffff;
    ss_thresh = INIT_SS_THRESH;
    cwnd_bytes = init_window;
    update_state = 0;
}

void Rogue::packets_acked(int sample_rtt)
{
    if (sample_rtt <= BASE_RTT)
        sample_rtt = BASE_RTT;

	/* Filter to find propagation delay: */
    baseRTT = GET_MIN(sample_rtt, baseRTT);
        
    minRTT = sample_rtt; //GET_MIN(minRTT, sample_rtt);
}

int Rogue::tcp_in_slow_start()
{
    if (cwnd_bytes < ss_thresh)
        return 1;
    else
        return 0;
}

int Rogue::tcp_vegas_ssthresh()
{
    return GET_MIN(ss_thresh, cwnd_bytes);
}

int Rogue::cc_avoid()
{
    uint32_t rtt;
    uint64_t target_cwnd;
    rtt = minRTT;
    target_cwnd = (uint64_t)cwnd_bytes * baseRTT / rtt;
    diff = (cwnd_bytes) / P4ML_PACKET_SIZE * (rtt - baseRTT) / baseRTT;

    if (diff > gamma_vegas && tcp_in_slow_start()) {
        cwnd_bytes = GET_MIN(cwnd_bytes, (uint64_t)(target_cwnd + P4ML_PACKET_SIZE));
        ss_thresh = tcp_vegas_ssthresh();
        update_state = 0;

    } else if (rtt > 5000) {
        cwnd_bytes = P4ML_PACKET_SIZE;
        update_state = 9;

    } else if (tcp_in_slow_start()) {
        cwnd_bytes += cwnd_bytes;
        update_state = 1;

    } else if (rtt < 30) {
        cwnd_bytes += GET_MAX(P4ML_PACKET_SIZE, cwnd_bytes);
        update_state = 8;

    } else if (diff > beta) {
        cwnd_bytes = cwnd_bytes / 2;
        ss_thresh = tcp_vegas_ssthresh();
        update_state = 2;

    } else if (diff < alpha) {
        cwnd_bytes += P4ML_PACKET_SIZE;
        update_state = 4;

    } else {
        update_state = 5;
    }

    if (cwnd_bytes < P4ML_PACKET_SIZE)
        cwnd_bytes = P4ML_PACKET_SIZE;
    if (cwnd_bytes > 100 * P4ML_PACKET_SIZE)
        cwnd_bytes = 100 * P4ML_PACKET_SIZE;
    if (cwnd_bytes > P4ML_PACKET_SIZE)
        cwnd_bytes = (cwnd_bytes / P4ML_PACKET_SIZE) * P4ML_PACKET_SIZE;
    return cwnd_bytes / P4ML_PACKET_SIZE;
}

/* Delete these function will lead to 6Gbps decrease... */
int Rogue:: tcp_current_ssthresh()
{
		return GET_MAX(ss_thresh,
			   ((cwnd_bytes >> 1) +
			    (cwnd_bytes >> 2)));
}
void Rogue::PrintStats()
{
	cout<<" cwnd_bytes=" << cwnd_bytes <<" minRTT="<<minRTT <<" ss_thresh="<<ss_thresh<<" update_state="<< update_state <<" diff="<<diff<<endl;
}
/*      */

int Rogue::adjustWindow(bool isECN) {
    if (isECN) {
        cwnd_bytes /= 2;
    } else {
        cwnd_bytes += 1500;
    }

    if (cwnd_bytes < P4ML_PACKET_SIZE)
        cwnd_bytes = P4ML_PACKET_SIZE;
    if (cwnd_bytes > MAX_BYTES)
        cwnd_bytes = MAX_BYTES;
    if (cwnd_bytes > P4ML_PACKET_SIZE)
        cwnd_bytes = (cwnd_bytes / P4ML_PACKET_SIZE) * P4ML_PACKET_SIZE;
    return cwnd_bytes / P4ML_PACKET_SIZE;
}