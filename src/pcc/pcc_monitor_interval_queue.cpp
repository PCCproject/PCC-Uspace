#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_queue.h"
#include "net/quic/core/congestion_control/rtt_stats.h"
#else
#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"
#include "gfe/quic/core/congestion_control/rtt_stats.h"
#endif
#else
#include "pcc_monitor_interval_queue.h"
#include "pcc_sender.h"
#include <iostream>
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

#ifndef QUIC_PORT
//#define DEBUG_UTILITY_CALC
//#define DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
//#define DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
//#define DEBUG_INTERVAL_SIZE

#endif

namespace {
// Number of probing MonitorIntervals necessary for Probing.
//const size_t kRoundsPerProbing = 4;
// Tolerance of loss rate by utility function.
const float kLossTolerance = 0.05f;
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = -1000.0f;
// Coefficient of RTT term in utility function.
const float kRTTCoefficient = -200.0f;
#ifndef QUIC_PORT_LOCAL
// Number of microseconds per second.
const float kNumMicrosPerSecond = 1000000.0f;
#endif
// Coefficienty of the latency term in the utility function.
const float kLatencyCoefficient = 1;
// Alpha factor in the utility function.
const float kAlpha = 1;
// An exponent in the utility function.
const float kExponent = 0.9;
// An exponent in the utility function.
const size_t kMegabit = 1024 * 1024;
}  // namespace

PccMonitorIntervalQueue::PccMonitorIntervalQueue() {}

#if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
PccMonitorIntervalQueue::~PccMonitorIntervalQueue() {}
#endif
void PccMonitorIntervalQueue::EnqueueMonitorInterval(
        MonitorInterval& mi) {
    monitor_intervals_.emplace_back(mi);
}

void PccMonitorIntervalQueue::OnPacketSent(QuicTime sent_time,
                                           QuicPacketNumber packet_number,
                                           QuicByteCount bytes) {
  if (monitor_intervals_.empty()) {
    #ifdef QUIC_PORT
    QUIC_BUG << "OnPacketSent called with empty queue.";
    #endif
    return;
  }

  MonitorInterval& interval = monitor_intervals_.back();
  interval.OnPacketSent(sent_time, packet_number, bytes);
}

void PccMonitorIntervalQueue::OnCongestionEvent(
    const AckedPacketVector& acked_packets,
    const LostPacketVector& lost_packets,
    int64_t rtt_us,
    QuicTime event_time) {

  if (monitor_intervals_.empty()) {
    // Skip all the received packets if we have no monitor intervals.
    return;
  }

  for (MonitorInterval& interval : monitor_intervals_) {
    if (interval.AllPacketsAccountedFor()) {
      // Skips intervals that have available utilities.
      continue;
    }

    for (const AckedPacket& acked_packet : acked_packets) {
      interval.OnPacketAck(event_time, acked_packet.packet_num, acked_packet.bytes_acked);
    }

    for (const LostPacket& lost_packet : lost_packets) {
      interval.OnPacketLost(event_time, acked_packet.packet_num, lost_packet.bytes_lost);
    }
  }
}

const MonitorInterval& PccMonitorIntervalQueue::current() const {
  #ifdef QUIC_PORT
  DCHECK(!monitor_intervals_.empty());
  #endif
  return monitor_intervals_.back();
}

bool PccMonitorIntervalQueue::empty() const {
  return monitor_intervals_.empty();
}

#ifndef QUIC_PORT_LOCAL
size_t PccMonitorIntervalQueue::size() const {
  return monitor_intervals_.size();
}

#endif

bool PccMonitorIntervalQueue::CalculateUtility2(MonitorInterval* interval) {
#else
bool PccMonitorIntervalQueue::CalculateUtility(MonitorInterval* interval) {
#endif
  if (interval->last_packet_sent_time == interval->first_packet_sent_time) {
    // Cannot get valid utility if interval only contains one packet.
    return false;
  }

  if (interval->packet_rtt_samples.size() < 2) {
      // Cannot get valid utility if we do not have at least two packet with
      // valid RTTs.
      return false;
  }
  const int64_t kMinTransmissionTime = 1l;
  int64_t mi_duration = std::max(
      kMinTransmissionTime,
  #ifdef QUIC_PORT
      (interval->last_packet_sent_time - interval->first_packet_sent_time).ToMicroseconds());
  #else
      (interval->last_packet_sent_time - interval->first_packet_sent_time));
  #endif

  float mi_time_seconds = static_cast<float>(mi_duration) / kNumMicrosPerSecond;
  float bytes_lost = static_cast<float>(interval->bytes_lost);
  float bytes_sent = static_cast<float>(interval->bytes_sent);
  
  float sending_rate_bps = bytes_sent * 8.0f / mi_time_seconds;
  float sending_factor = kAlpha * pow(sending_rate_bps/kMegabit, kExponent);

  // Approximate the derivative at each point by computing the slope of RTT to
  // the following point and average these values.
  float rtt_first_half_sum = 0.0;
  float rtt_second_half_sum = 0.0;
  int half_samples = interval->packet_rtt_samples.size() / 2;
  for (int i = 0; i < half_samples; ++i) {
    #ifdef QUIC_PORT
    rtt_first_half_sum += static_cast<float>(interval->packet_rtt_samples[i].sample_rtt.ToMicroSeconds());
    rtt_second_half_sum += static_cast<float>(interval->packet_rtt_samples[i + half_samples].sample_rtt.ToMicroSeconds());
    #else
    rtt_first_half_sum += static_cast<float>(interval->packet_rtt_samples[i].sample_rtt);
    rtt_second_half_sum += static_cast<float>(interval->packet_rtt_samples[i + half_samples].sample_rtt);
    #endif
  }
  float latency_inflation = 2.0 * (rtt_second_half_sum - rtt_first_half_sum) / (rtt_first_half_sum + rtt_second_half_sum);

  float avg_rtt = (rtt_first_half_sum + rtt_second_half_sum) / (2.0 * half_samples);

  float rtt_penalty = int(int(latency_inflation * 100) / 100.0 * 100) / 2 * 2/ 100.0;
  float rtt_contribution = kLatencyCoefficient * 11330 * bytes_sent * (pow(rtt_penalty, 1));

  float loss_rate = bytes_lost / bytes_sent;
  float loss_contribution = interval->n_packets * (11.35 * (pow((1 + loss_rate), 1) - 1));
  if (loss_rate <= 0.03) {
    loss_contribution = interval->n_packets * (1 * (pow((1 + loss_rate), 1) - 1));
  }
  loss_contribution *= -1.0 *
      (sending_rate_bps / kMegabit) / static_cast<float>(interval->n_packets);
  rtt_contribution *= -1.0 *
      (sending_rate_bps / kMegabit) / static_cast<float>(interval->n_packets);
  
  float vivace_latency_utility = sending_factor + loss_contribution + rtt_contribution;
  float vivace_loss_const = 11.35;
  float vivace_loss_utility = sending_rate_bps - vivace_loss_const * (1.0 / (1.0 - loss_rate) - 1.0);
  float cubed_loss_utility = sending_rate_bps * (1.0 - loss_rate) * (1.0 - loss_rate) * (1.0 - loss_rate);
  float alpha = 1.0 / 30000.0;
  float latency_sensitivity = 1.0;
  const char* latency_sensitivity_arg = Options::Get("--latency-sensitivity=");
  if (latency_sensitivity_arg != NULL) {
    latency_sensitivity = atof(latency_sensitivity_arg);
  }
  alpha *= latency_sensitivity;
  float beta = 20.0;
  float inverted_exponent_utility = sending_rate_bps / (exp(alpha * avg_rtt + beta * (loss_rate / (1.0 - loss_rate))));
  float pccv1_const = 11.35;
  float pccv1_utility = sending_rate_bps * (1.0 - loss_rate) * (1.0 / (1.0 + exp(pccv1_const * (loss_rate - 0.05)))) - sending_rate_bps * loss_rate;

  float lr_utility = pow(sending_rate_bps * (1.0 - loss_rate), 0.9) - 10000000.0 * loss_rate;

  float current_utility = 0.0;
  if (Options::Get("--cubed-loss-utility")) {
    current_utility = cubed_loss_utility;
  } else if (Options::Get("--inverted-exponent-utility")) {
    current_utility = inverted_exponent_utility;
  } else if (Options::Get("--vivace-loss-utility")) {
    current_utility = vivace_loss_utility;
  } else if (Options::Get("--pccv1-utility")) {
    current_utility = pccv1_utility;
  } else if (Options::Get("--lr-utility")) {
    current_utility = lr_utility;
  } else {
    current_utility = vivace_latency_utility;
  }

  #if !defined(QUIC_PORT)
    PccLoggableEvent event("Calculate Utility", "-DEBUG_UTILITY_CALC");
    event.AddValue("Vivace Latency Utility", vivace_latency_utility);
    event.AddValue("Vivace Loss Utility", vivace_loss_utility);
    event.AddValue("Cubed Loss Utility", cubed_loss_utility);
    event.AddValue("Inverted Exponent Utility", inverted_exponent_utility);
    event.AddValue("Pccv1 Utility", pccv1_utility);
    event.AddValue("LR Utility", lr_utility);
    event.AddValue("Utility", current_utility);
    event.AddValue("Number of Packets", interval->n_packets);
    event.AddValue("Target Rate", interval->sending_rate);
    event.AddValue("Actual Rate", bytes_sent * 8.0f / mi_time_seconds);
    event.AddValue("Loss Rate", loss_rate);
    event.AddValue("Throughput", (bytes_sent - bytes_lost) * 8.0f / mi_time_seconds);
    event.AddValue("Avg RTT", avg_rtt);
    event.AddValue("Latency Inflation", latency_inflation);
    event.AddValue("Throughput Utility", sending_factor);
    event.AddValue("RTT Utility", rtt_contribution);
    event.AddValue("Loss Rate Utility", loss_contribution);
    delegate_->log->LogEvent(event);
  #endif

  interval->utility = current_utility;
  interval->rtt = (rtt_first_half_sum + rtt_second_half_sum) / (2.0 * half_samples);
  interval->loss_rate = loss_rate;
  interval->latency_inflation = latency_inflation;
  return true;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
