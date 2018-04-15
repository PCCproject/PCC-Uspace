#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval.h"
#else
#include "net/quic/core/congestion_control/pcc_monitor_interval.h"
#endif
#else
#include "pcc_monitor_interval.h"
#endif

#ifndef QUIC_PORT
typedef int32_t QuicPacketCount;
typedef int32_t QuicPacketNumber;
typedef int64_t QuicByteCount;
typedef int64_t QuicTime;
typedef double  QuicBandwidth;
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
using namespace net;
#endif

PacketRttSample::PacketRttSample() : packet_number(0),
#ifdef QUIC_PORT
                                     rtt(QuicTime::Delta::Zero()) {}
#else
                                     rtt(0) {}
#endif

PacketRttSample::PacketRttSample(QuicPacketNumber packet_number,
#ifdef QUIC_PORT
                                 QuicTime::Delta rtt)
#else
                                 QuicTime rtt)
#endif
    : packet_number(packet_number),
      rtt(rtt) {}

MonitorInterval::MonitorInterval(QuicBandwidth sending_rate, QuicTime end_time) {
    this->sending_rate = sending_rate;
    this->end_time = end_time;
    n_packets_sent = 0;
    n_packets_accounted_for = 0;
}
void MonitorInterval::OnPacketSent(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size) {
    if (n_packets_sent == 0) {
        first_packet_sent_time = cur_time;
        first_packet_number = packet_number;
        last_packet_number_accounted_for = first_packet_number - 1;
    }
    last_packet_sent_time = cur_time;
    last_packet_number = packet_number;
    ++n_packets_sent;
    bytes_sent += packet_size;
}

void MonitorInterval::OnPacketAcked(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size, QuicTime rtt) {
    if (ContainsPacket(packet_number) && packet_number > last_packet_number_accounted_for) {
        int skipped = (packet_number - last_packet_number_accounted_for) - 1;
        n_bytes_acked += packet_size;
        n_packets_accounted_for += skipped + 1;
        packet_rtt_samples.push_back(PacketRttSample(packet_number, rtt));
    } else if (packet_number > last_packet_number) {
        n_packets_accounted_for = n_packets;
    }
}

void MonitorInterval::OnPacketLost(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size) {
    if (ContainsPacket(packet_number) && packet_number > last_packet_number_accounted_for) {
        int skipped = (packet_number - last_packet_number_accounted_for) - 1;
        n_bytes_lost += packet_size;
        n_packets_accounted_for += skipped + 1;
    } else if (packet_number > last_packet_number) {
        n_packets_accounted_for = n_packets;
    }
}

bool MonitorInterval::AllPacketsSent(QuicTime cur_time) {
    return (cur_time >= end_time);
}

bool MonitorInterval::AllPacketsAccountedFor() {
    return (n_packets_accounted_for == n_packets_sent);
}

QuicBandwidth MonitorInterval::GetObsThroughput() {
    float dur = GetObsDur();
    if (dur == 0) {
        return 0;
    }
    return bytes_acked / dur;
}

QuicBandwidth MonitorInterval::GetObsSendingRate() {
    float dur = GetObsDur();
    if (dur == 0) {
        return 0;
    }
    return bytes_sent / dur;
}

float MonitorInterval::GetObsDur() {
    return (last_packet_sent_time - first_packet_sent_time);
}

float MonitorInterval::GetObsRtt() {
    if (packet_rtt_samples.empty()) {
        return 0;
    }
    rtt_sum = 0;
    for (sample : packet_rtt_samples) {
        rtt_sum += sample.rtt;
    }
    return rtt_sum / packet_rtt_samples.size();
}

float MonitorInterval::GetObsRttInflation() {
    if (packet_rtt_samples.size() < 2) {
        return 0;
    }
    float first_half_rtt_sum = 0;
    float second_half_rtt_sum = 0;
    int half_count = packet_rtt_samples.size() / 2;
    for (int i = 0; i < 2 * half_count) {
        if (i < half_count) {
            first_half_rtt_sum += packet_rtt_samples[i].rtt;
        } else {
            second_half_rtt_sum += packet_rtt_samples[i].rtt;
        }
    }
    float rtt_inflation = 2.0 * (rtt_second_half_sum - rtt_first_half_sum) / (rtt_first_half_sum + rtt_second_half_sum);
    return rtt_inflation;
}

float MonitorInterval::GetObsLossRate() {
    return 1.0 - (bytes_acked / (float)bytes_sent);
}

void MonitorInterval::SetUtility(float new_utility) {
    utility = new_utiltiy;
}

float MonitorInterval::GetUtility() {
    return utility;
}

bool MonitorInterval::ContainsPacket(QuicPacketNumber packet_number) {
    return (packet_number >= first_packet_number && packet_number <= last_packet_number);
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_H_
