#ifndef _CONGESTION_EVENTS_H_
#define _CONGETIONS_EVENTS_H_


#include <cinttypes>
#include <iostream>
#include <vector>
#include "pcc_sender.h"

typedef std::vector<CongestionEvent> CongestionVector;

void OnCongestionEvent(PccSender& pcc_sender, uint64_t time, uint64_t rtt, CongestionVector& acked_packets, CongestionVector& lost_packets);
void OnPacketSent(PccSender& pcc_sender, uint64_t time, int32_t seq_no, int32_t packet_size);


#endif
