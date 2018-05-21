
#include "pcc_mi_metric.h"

static double ack_dur_func(const MonitorInterval& mi) {
    return mi.last_packet_ack_time - mi.first_packet_ack_time;
}
static MonitorIntervalMetric ack_dur_metric =
        MonitorIntervalMetric("AckDuration", &ack_dur_func);

static double throughput_func(const MonitorInterval& mi) {
    double ack_dur = ack_dur_func(mi);
    if (ack_dur == 0) {
        return 0;
    }
    return 8 * mi.bytes_acked * 1000000.0 / ack_dur;
}
static MonitorIntervalMetric throughput_metric =
        MonitorIntervalMetric("Throughput", &throughput_func);

static double target_rate_func(const MonitorInterval& mi) {
    return mi.target_sending_rate;
}
static MonitorIntervalMetric target_rate_metric =
        MonitorIntervalMetric("TargetRate", &target_rate_func);

static double send_dur_func(const MonitorInterval& mi) {
    return mi.last_packet_sent_time - mi.first_packet_sent_time;
}
static MonitorIntervalMetric send_dur_metric =
        MonitorIntervalMetric("SendDuration", &send_dur_func);

static double send_rate_func(const MonitorInterval& mi) {
    double send_dur = send_dur_func(mi);
    if (send_dur == 0) {
        return 0;
    }
    return 8 * mi.bytes_sent * 1000000.0 / send_dur;
}
static MonitorIntervalMetric send_rate_metric =
        MonitorIntervalMetric("SendRate", &send_rate_func);

static double avg_rtt_func(const MonitorInterval& mi) {
    if (mi.packet_rtt_samples.empty()) {
        return 0;
    }
    double rtt_sum = 0.0;
    for (const PacketRttSample& sample : mi.packet_rtt_samples) {
        rtt_sum += sample.rtt;
    }
    return rtt_sum / mi.packet_rtt_samples.size();
}
static MonitorIntervalMetric avg_rtt_metric =
        MonitorIntervalMetric("AverageRtt", &avg_rtt_func);

static double rtt_inflation_func(const MonitorInterval& mi) {
    if (mi.packet_rtt_samples.size() < 2) {
        return 0;
    }
    double first_half_rtt_sum = 0;
    double second_half_rtt_sum = 0;
    int half_count = mi.packet_rtt_samples.size() / 2;
    for (int i = 0; i < 2 * half_count; ++i) {
        if (i < half_count) {
            first_half_rtt_sum += mi.packet_rtt_samples[i].rtt;
        } else {
            second_half_rtt_sum += mi.packet_rtt_samples[i].rtt;
        }
    }
    double rtt_inflation = 2.0 * (second_half_rtt_sum - first_half_rtt_sum) / (first_half_rtt_sum + second_half_rtt_sum);
    return rtt_inflation;
}
static MonitorIntervalMetric rtt_inflation_metric =
        MonitorIntervalMetric("RttInflation", &rtt_inflation_func);

static double loss_rate_func(const MonitorInterval& mi) {
    if (mi.bytes_sent == 0) {
        return 0;
    }
    return 1.0 - (mi.bytes_acked / (double)mi.bytes_sent);
}
static MonitorIntervalMetric loss_rate_metric =
        MonitorIntervalMetric("LossRate", &loss_rate_func);
