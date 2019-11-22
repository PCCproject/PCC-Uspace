#include "pcc_mi.h"

typedef int32_t QuicPacketCount;
typedef int32_t QuicPacketNumber;
typedef int64_t QuicByteCount;
typedef int64_t QuicTime;
typedef double  QuicBandwidth;

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

int MonitorInterval::next_id = 0;

MonitorInterval::MonitorInterval(QuicBandwidth sending_rate, QuicTime min_end_time, QuicPacketNumber min_packets) {
    this->target_sending_rate = sending_rate;
    this->min_end_time = min_end_time;
    this->min_packets = min_packets;
    bytes_sent = 0;
    bytes_acked = 0;
    bytes_lost = 0;
    n_packets_sent = 0;
    n_packets_accounted_for = 0;
    first_packet_ack_time = 0;
    last_packet_ack_time = 0;
    id = next_id;
    ++next_id;
}

void MonitorInterval::OnPacketSent(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size) {
    if (n_packets_sent == 0) {
        first_packet_sent_time = cur_time;
        first_packet_number = packet_number;
        last_packet_number_accounted_for = first_packet_number - 1;
        //std::cerr << "MI " << id << " started with " << packet_number << ", dur " << (end_time - cur_time) << std::endl; 
    }
    last_packet_sent_time = cur_time;
    last_packet_number = packet_number;
    ++n_packets_sent;
    bytes_sent += packet_size;
}

void MonitorInterval::OnPacketAcked(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size, QuicTime rtt) {
    if (ContainsPacket(packet_number)) {// && packet_number > last_packet_number_accounted_for) {
        if (first_packet_ack_time == 0) {
            first_packet_ack_time = cur_time;
        }
        int skipped = (packet_number - last_packet_number_accounted_for) - 1;
        bytes_acked += packet_size;
        n_packets_accounted_for += 1; //skipped + 1;
        packet_rtt_samples.push_back(PacketRttSample(packet_number, rtt));
        last_packet_number_accounted_for = packet_number;
        last_packet_ack_time = cur_time;
    }// else if (packet_number > last_packet_number) {
    //    n_packets_accounted_for = n_packets_sent;
    //    last_packet_number_accounted_for = last_packet_number;
    //}
    //if (packet_number >= first_packet_number && first_packet_ack_time == 0) {
    //    first_packet_ack_time = cur_time;
    //}
    //if (packet_number >= last_packet_number && last_packet_ack_time == 0) {
    //    last_packet_ack_time = cur_time;
    //}
    if (AllPacketsAccountedFor()) {
        //std::cerr << "MI " << id << " [" << first_packet_number << ", " << last_packet_number << "] finished at packet " << packet_number << std::endl; 
    }
}

void MonitorInterval::OnPacketLost(QuicTime cur_time, QuicPacketNumber packet_number, QuicByteCount packet_size) {
    if (ContainsPacket(packet_number)) {// && packet_number > last_packet_number_accounted_for) {
        int skipped = (packet_number - last_packet_number_accounted_for) - 1;
        bytes_lost += packet_size;
        n_packets_accounted_for += 1;//skipped + 1;
        last_packet_number_accounted_for = packet_number;
    }// else if (packet_number > last_packet_number) {
    //    n_packets_accounted_for = n_packets_sent;
    //    last_packet_number_accounted_for = last_packet_number;
    //}
    //if (packet_number >= first_packet_number && first_packet_ack_time == 0) {
    //    first_packet_ack_time = cur_time;
    //}
    //if (packet_number >= last_packet_number && last_packet_ack_time == 0) {
    //    last_packet_ack_time = cur_time;
    //}
    if (AllPacketsAccountedFor()) {
        //std::cerr << "MI [" << first_packet_number << ", " << last_packet_number << "] finished at packet " << packet_number << std::endl; 
    }
}

bool MonitorInterval::AllPacketsSent(QuicTime cur_time) const {
    return (cur_time >= min_end_time && n_packets_sent >= min_packets);
}

bool MonitorInterval::AllPacketsAccountedFor() {
    return (n_packets_accounted_for == n_packets_sent && n_packets_sent >= min_packets);
}

QuicTime MonitorInterval::GetStartTime() const {
    return first_packet_sent_time;
}

QuicBandwidth MonitorInterval::GetTargetSendingRate() const {
    return target_sending_rate;
}

QuicBandwidth MonitorInterval::GetObsThroughput() const {
    float dur = GetObsRecvDur();
    if (dur == 0) {
        return 0;
    }
    return 8 * bytes_acked / (dur / 1000000.0);
}

QuicBandwidth MonitorInterval::GetObsSendingRate() const {
    float dur = GetObsSendDur();
    if (dur == 0) {
        return 0;
    }
    return 8 * bytes_sent / (dur / 1000000.0);
}

float MonitorInterval::GetObsSendDur() const {
    return (last_packet_sent_time - first_packet_sent_time);
}

float MonitorInterval::GetObsRecvDur() const {
    return (last_packet_ack_time - first_packet_ack_time);
}

float MonitorInterval::GetObsRtt() const {
    if (packet_rtt_samples.empty()) {
        return 0;
    }
    double rtt_sum = 0.0;
    for (const PacketRttSample& sample : packet_rtt_samples) {
        rtt_sum += sample.rtt;
    }
    return rtt_sum / packet_rtt_samples.size();
}

float GetRttPacketSlope(const std::vector<PacketRttSample>& rtts) {
    double avgX = rtts.size() / 2.0f;
    double avgY = 0;
    for (int i = 0; i < rtts.size(); i++) {
        avgY += rtts[i].rtt;
    }
    avgY /= rtts.size();

	double numerator = 0.0;
	double denominator = 0.0;
	for(int i = 0; i < rtts.size(); i++){
        numerator += (i - avgX) * (rtts[i].rtt - avgY);
        denominator += (i - avgX) * (i - avgX);
    }

    if (denominator == 0) {
        return 0;
    }

    return numerator / denominator;
}

float MonitorInterval::GetObsRttInflation() const {
    if (packet_rtt_samples.size() < 2) {
        return 0;
    }
    if (GetObsRecvDur() == 0.0) {
        return 0;
    }
    double recv_dur_inflation = 1.0 - GetObsSendDur() / GetObsRecvDur();
    float first_half_rtt_sum = 0;
    float second_half_rtt_sum = 0;
    int half_count = packet_rtt_samples.size() / 2;
    for (int i = 0; i < 2 * half_count; ++i) {
        if (i < half_count) {
            first_half_rtt_sum += packet_rtt_samples[i].rtt;
        } else {
            second_half_rtt_sum += packet_rtt_samples[i].rtt;
        }
    }
    //float rtt_inflation = 2.0 * (second_half_rtt_sum - first_half_rtt_sum) / (first_half_rtt_sum + second_half_rtt_sum);
    float rtt_inflation = 2.0 * (second_half_rtt_sum - first_half_rtt_sum) / (half_count * GetObsSendDur());
    double result = recv_dur_inflation;
    if (fabs(recv_dur_inflation) > fabs(rtt_inflation)) {
        result = rtt_inflation;
    }
	double rtt_slope = GetRttPacketSlope(packet_rtt_samples);
	rtt_slope *= packet_rtt_samples.size();
	rtt_slope /= GetObsSendDur();
    if (fabs(result) > fabs(rtt_slope)) {
        result = rtt_slope;
    }
	double rate = GetTargetSendingRate();
    //std::cout << "rtt_inflation " << rtt_inflation << ", recv inflation " << recv_dur_inflation << ", rtt_slope " << rtt_slope << ", rate " << rate << std::endl;
    return result;
}

float MonitorInterval::GetObsLossRate() const {
    return 1.0 - (bytes_acked / (float)bytes_sent);
}

void MonitorInterval::SetUtility(float new_utility) {
    utility = new_utility;
}

float MonitorInterval::GetObsUtility() const {
    return utility;
}

bool MonitorInterval::ContainsPacket(QuicPacketNumber packet_number) {
    return (packet_number >= first_packet_number && packet_number <= last_packet_number);
}
