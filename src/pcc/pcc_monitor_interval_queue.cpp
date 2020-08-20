#include "pcc_monitor_interval_queue.h"

#include <assert.h>
#include <iostream>


//#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"

//#include "third_party/quic/core/congestion_control/rtt_stats.h"

// namespace quic {

namespace {
// Minimum number of reliable RTT samples per monitor interval.
const size_t kMinReliableRtt = 4;
}

PacketRttSample::PacketRttSample()
    : packet_number(0),
      sample_rtt(QuicTime::Delta::Zero()),
      ack_timestamp(QuicTime::Zero()),
      is_reliable(false),
      is_reliable_for_gradient_calculation(false) {}

PacketRttSample::PacketRttSample(QuicPacketNumber packet_number,
                                 QuicTime::Delta rtt,
                                 QuicTime ack_timestamp,
                                 bool reliability,
                                 bool gradient_reliability)
    : packet_number(packet_number),
      sample_rtt(rtt),
      ack_timestamp(ack_timestamp),
      is_reliable(reliability),
      is_reliable_for_gradient_calculation(gradient_reliability) {}

LostPacketSample::LostPacketSample() : packet_number(0), bytes(0) {}

LostPacketSample::LostPacketSample(
    QuicPacketNumber packet_number,
    QuicByteCount bytes) : packet_number(packet_number),
                           bytes(bytes) {}
                                   

MonitorInterval::MonitorInterval()
    : sending_rate(QuicBandwidth::Zero()),
      is_useful(false),
      rtt_fluctuation_tolerance_ratio(0.0),
      first_packet_sent_time(QuicTime::Zero()),
      last_packet_sent_time(QuicTime::Zero()),
      first_packet_number(0),
      last_packet_number(0),
      bytes_sent(0),
      bytes_acked(0),
      bytes_lost(0),
      rtt_on_monitor_start(QuicTime::Delta::Zero()),
      rtt_on_monitor_end(QuicTime::Delta::Zero()),
      min_rtt(QuicTime::Delta::Zero()),
      num_reliable_rtt(0),
      num_reliable_rtt_for_gradient_calculation(0),
      has_enough_reliable_rtt(false),
      is_monitor_duration_extended(false) {}

MonitorInterval::MonitorInterval(QuicBandwidth sending_rate,
                                 bool is_useful,
                                 float rtt_fluctuation_tolerance_ratio,
                                 QuicTime::Delta rtt)
    : sending_rate(sending_rate),
      is_useful(is_useful),
      rtt_fluctuation_tolerance_ratio(rtt_fluctuation_tolerance_ratio),
      first_packet_sent_time(QuicTime::Zero()),
      last_packet_sent_time(QuicTime::Zero()),
      first_packet_number(0),
      last_packet_number(0),
      bytes_sent(0),
      bytes_acked(0),
      bytes_lost(0),
      rtt_on_monitor_start(rtt),
      rtt_on_monitor_end(rtt),
      min_rtt(rtt),
      num_reliable_rtt(0),
      num_reliable_rtt_for_gradient_calculation(0),
      has_enough_reliable_rtt(false),
      is_monitor_duration_extended(false) {}

PccMonitorIntervalQueue::PccMonitorIntervalQueue(
    PccMonitorIntervalQueueDelegateInterface* delegate)
    : pending_rtt_(QuicTime::Delta::Zero()),
      pending_avg_rtt_(QuicTime::Delta::Zero()),
      pending_ack_interval_(QuicTime::Delta::Zero()),
      pending_event_time_(QuicTime::Zero()),
      burst_flag_(false),
      avg_interval_ratio_(-1.0),
      num_useful_intervals_(0),
      num_available_intervals_(0),
      delegate_(delegate) {}

void PccMonitorIntervalQueue::EnqueueNewMonitorInterval(
    QuicBandwidth sending_rate,
    bool is_useful,
    float rtt_fluctuation_tolerance_ratio,
    QuicTime::Delta rtt) {
  if (is_useful) {
    ++num_useful_intervals_;
  }

  monitor_intervals_.emplace_back(sending_rate, is_useful,
                                  rtt_fluctuation_tolerance_ratio, rtt);
}

void PccMonitorIntervalQueue::OnPacketSent(QuicTime sent_time,
                                           QuicPacketNumber packet_number,
                                           QuicByteCount bytes,
                                           QuicTime::Delta sent_interval) {
  if (monitor_intervals_.empty()) {
    std::cerr << "OnPacketSent called with empty queue.";
    return;
  }

  if (monitor_intervals_.back().bytes_sent == 0) {
    // This is the first packet of this interval.
    monitor_intervals_.back().first_packet_sent_time = sent_time;
    monitor_intervals_.back().first_packet_number = packet_number;
  }

  monitor_intervals_.back().last_packet_sent_time = sent_time;
  monitor_intervals_.back().last_packet_number = packet_number;
  monitor_intervals_.back().bytes_sent += bytes;

  monitor_intervals_.back().packet_sent_intervals.push_back(sent_interval);
}

void PccMonitorIntervalQueue::OnCongestionEvent(
    const AckedPacketVector& acked_packets,
    const LostPacketVector& lost_packets,
    QuicTime::Delta avg_rtt,
    QuicTime::Delta latest_rtt,
    QuicTime::Delta min_rtt,
    QuicTime event_time,
    QuicTime::Delta ack_interval) {
  num_available_intervals_ = 0;
  if (num_useful_intervals_ == 0) {
    // Skip all the received packets if no intervals are useful.
    return;
  }

  bool has_invalid_utility = false;
  for (MonitorInterval& interval : monitor_intervals_) {
    if (!interval.is_useful) {
      // Skips useless monitor intervals.
      continue;
    }

    if (IsUtilityAvailable(interval)) {
      // Skip intervals with available utilities.
      ++num_available_intervals_;
      continue;
    }

    for (const LostPacket& lost_packet : lost_packets) {
      if (IntervalContainsPacket(interval, lost_packet.packet_number)) {
        interval.bytes_lost += lost_packet.bytes_lost;
        interval.lost_packet_samples.push_back(LostPacketSample(
            lost_packet.packet_number, lost_packet.bytes_lost));
      }
    }

    for (const AckedPacket& acked_packet : pending_acked_packets_) {
      if (IntervalContainsPacket(interval, acked_packet.packet_number)) {
        if (interval.bytes_acked == 0) {
          // This is the RTT before starting sending at interval.sending_rate.
          interval.rtt_on_monitor_start = pending_avg_rtt_;
        }
        interval.bytes_acked += acked_packet.bytes_acked;

        bool is_reliable = false;
        if (!pending_ack_interval_.IsZero()) {
          float interval_ratio =
              static_cast<float>(pending_ack_interval_.ToMicroseconds()) /
              static_cast<float>(ack_interval.ToMicroseconds());
          if (interval_ratio < 1.0) {
            interval_ratio = 1.0 / interval_ratio;
          }
          if (avg_interval_ratio_ < 0) {
            avg_interval_ratio_ = interval_ratio;
          }

          if (interval_ratio > 50.0 * avg_interval_ratio_) {
            burst_flag_ = true;
          } else if (burst_flag_) {
            if (latest_rtt > pending_rtt_ && pending_rtt_ < pending_avg_rtt_) {
              burst_flag_ = false;
            }
          } else {
            is_reliable = true;
            interval.num_reliable_rtt++;
          }

          avg_interval_ratio_ =
              avg_interval_ratio_ * 0.9 + interval_ratio * 0.1;
        }

        bool is_reliable_for_gradient_calculation = false;
        if (is_reliable) {
        //if (latest_rtt > pending_rtt_) {
          is_reliable_for_gradient_calculation = true;
          interval.num_reliable_rtt_for_gradient_calculation++;
        }

        interval.packet_rtt_samples.push_back(PacketRttSample(
            acked_packet.packet_number, pending_rtt_, pending_event_time_,
            is_reliable, is_reliable_for_gradient_calculation));
        if (interval.num_reliable_rtt >= kMinReliableRtt) {
          interval.has_enough_reliable_rtt = true;
        }
      }
    }

    if (IsUtilityAvailable(interval)) {
      interval.rtt_on_monitor_end = avg_rtt;
      interval.min_rtt = min_rtt;
      has_invalid_utility = HasInvalidUtility(&interval);
      if (has_invalid_utility) {
        break;
      }
      ++num_available_intervals_;
      assert(num_available_intervals_ <= num_useful_intervals_);
    }
  }

  pending_acked_packets_.clear();
  for (const AckedPacket acked_packet : acked_packets) {
    pending_acked_packets_.push_back(acked_packet);
  }
  pending_rtt_ = latest_rtt;
  pending_avg_rtt_ = avg_rtt;
  pending_ack_interval_ = ack_interval;
  pending_event_time_ = event_time;

  if (num_useful_intervals_ > num_available_intervals_ &&
      !has_invalid_utility) {
    return;
  }

  if (!has_invalid_utility) {
    assert(num_useful_intervals_ > 0u);

    std::vector<const MonitorInterval *> useful_intervals;
    for (const MonitorInterval& interval : monitor_intervals_) {
      if (!interval.is_useful) {
        continue;
      }
      useful_intervals.push_back(&interval);
    }
    assert(num_available_intervals_ == useful_intervals.size());

    delegate_->OnUtilityAvailable(useful_intervals, event_time);
  }

  // Remove MonitorIntervals from the head of the queue,
  // until all useful intervals are removed.
  while (num_useful_intervals_ > 0) {
    if (monitor_intervals_.front().is_useful) {
      --num_useful_intervals_;
    }
    monitor_intervals_.pop_front();
  }
  num_available_intervals_ = 0;
}

const MonitorInterval& PccMonitorIntervalQueue::front() const {
  assert(!monitor_intervals_.empty());
  return monitor_intervals_.front();
}

const MonitorInterval& PccMonitorIntervalQueue::current() const {
  assert(!monitor_intervals_.empty());
  return monitor_intervals_.back();
}

void PccMonitorIntervalQueue::extend_current_interval() {
  assert(!monitor_intervals_.empty());
  monitor_intervals_.back().is_monitor_duration_extended = true;
}

bool PccMonitorIntervalQueue::empty() const {
  return monitor_intervals_.empty();
}

size_t PccMonitorIntervalQueue::size() const {
  return monitor_intervals_.size();
}

void PccMonitorIntervalQueue::OnRttInflationInStarting() {
  monitor_intervals_.clear();
  num_useful_intervals_ = 0;
  num_available_intervals_ = 0;
}

bool PccMonitorIntervalQueue::IsUtilityAvailable(
    const MonitorInterval& interval) const {
  return (interval.has_enough_reliable_rtt &&
          interval.bytes_acked + interval.bytes_lost == interval.bytes_sent);
}

bool PccMonitorIntervalQueue::IntervalContainsPacket(
    const MonitorInterval& interval,
    QuicPacketNumber packet_number) const {
  return (packet_number >= interval.first_packet_number &&
          packet_number <= interval.last_packet_number);
}

bool PccMonitorIntervalQueue::HasInvalidUtility(
    const MonitorInterval* interval) const {
  return interval->first_packet_sent_time == interval->last_packet_sent_time;
}

// }  // namespace quic
