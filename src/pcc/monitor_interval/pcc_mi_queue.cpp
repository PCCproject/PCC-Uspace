#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_queue.h"
#include "net/quic/core/congestion_control/rtt_stats.h"
#else
#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"
#include "gfe/quic/core/congestion_control/rtt_stats.h"
#endif
#else
#include "pcc_mi_queue.h"
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
#define DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
#define DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
#define DEBUG_INTERVAL_SIZE

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
void PccMonitorIntervalQueue::Push(MonitorInterval mi) {
    monitor_intervals_.emplace_back(mi);
}

MonitorInterval PccMonitorIntervalQueue::Pop() {
    MonitorInterval mi = monitor_intervals_.front();
    monitor_intervals_.pop_front();
    return mi;
}

bool PccMonitorIntervalQueue::HasFinishedInterval(QuicTime cur_time) {
    if (monitor_intervals_.empty()) {
        return false;
    }
    return monitor_intervals_.front().AllPacketsAccountedFor(cur_time);
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
    if (interval.AllPacketsAccountedFor(event_time)) {
      // Skips intervals that have available utilities.
      continue;
    }

    for (const AckedPacket& acked_packet : acked_packets) {
      interval.OnPacketAcked(event_time, acked_packet.packet_number, acked_packet.bytes_acked, rtt_us);
    }

    for (const LostPacket& lost_packet : lost_packets) {
      interval.OnPacketLost(event_time, lost_packet.packet_number, lost_packet.bytes_lost);
    }
  }
}

const MonitorInterval& PccMonitorIntervalQueue::Current() const {
  #ifdef QUIC_PORT
  DCHECK(!monitor_intervals_.empty());
  #endif
  return monitor_intervals_.back();
}

bool PccMonitorIntervalQueue::Empty() const {
  return monitor_intervals_.empty();
}

#ifndef QUIC_PORT_LOCAL
size_t PccMonitorIntervalQueue::Size() const {
  return monitor_intervals_.size();
}

#endif

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
