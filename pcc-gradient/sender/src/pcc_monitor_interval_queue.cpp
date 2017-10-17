#include "pcc_monitor_interval_queue.h"
#include "pcc_sender.h"
#include <iostream>
      
      
//#define DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
//#define DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
//#define DEBUG_INTERVAL_SIZE

namespace {
// Number of probing MonitorIntervals necessary for Probing.
const size_t kRoundsPerProbing = 4;
// Tolerance of loss rate by utility function.
const float kLossTolerance = 0.05f;
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = -1000.0f;
// Coefficient of RTT term in utility function.
const float kRTTCoefficient = -200.0f;

}  // namespace

MonitorInterval::MonitorInterval()
    : first_packet_sent_time(0),
      last_packet_sent_time(0),
      first_packet_number(0),
      last_packet_number(0),
      bytes_total(0),
      bytes_acked(0),
      bytes_lost(0),
      utility(0.0),
      end_time(0.0){}

MonitorInterval::MonitorInterval(float sending_rate_mbps,
                                 bool is_useful,
                                 int64_t rtt_us,
                                 uint64_t end_time)
    : sending_rate_mbps(sending_rate_mbps),
      is_useful(is_useful),
      first_packet_sent_time(0),
      last_packet_sent_time(0),
      first_packet_number(0),
      last_packet_number(0),
      bytes_total(0),
      bytes_acked(0),
      bytes_lost(0),
      rtt_on_monitor_start_us(rtt_us),
      rtt_on_monitor_end_us(rtt_us),
      utility(0.0),
      end_time(end_time){}

void MonitorInterval::DumpMiPacketStates() {
  for (std::map<QuicPacketNumber, MiPacketState>::iterator it = pkt_state_map.begin(); it != pkt_state_map.end(); ++it) {
    if (it->second == MI_PACKET_STATE_SENT) {
      std::cout << it->first << " in state SENT" << std::endl;
    }
  }
}

void PccMonitorIntervalQueue::DumpIntervalMiPacketStates() {
  for (MonitorInterval& interval : monitor_intervals_) {
    interval.DumpMiPacketStates();
  }
}

UtilityInfo::UtilityInfo() : sending_rate_mbps(0.0), utility(0.0) {}

UtilityInfo::UtilityInfo(float rate, float utility)
    : sending_rate_mbps(rate), utility(utility) {}

PccMonitorIntervalQueue::PccMonitorIntervalQueue(
    PccSender* delegate)
    : num_useful_intervals_(0),
      num_available_intervals_(0),
      delegate_(delegate) {}

void PccMonitorIntervalQueue::EnqueueNewMonitorInterval(float sending_rate_mbps,
                                                        bool is_useful,
                                                        int64_t rtt_us,
                                                        uint64_t end_time) {
  //std::cout << "Added new monitor interval" << std::endl;
  if (is_useful) {
    ++num_useful_intervals_;
    //std::cout << "\tInterval is useful! (now have " << num_useful_intervals_ << ")" << std::endl;
  }

  monitor_intervals_.emplace_back(sending_rate_mbps, is_useful, rtt_us, end_time);
}

void PccMonitorIntervalQueue::OnPacketSent(QuicTime sent_time,
                                           QuicPacketNumber packet_number,
                                           QuicByteCount bytes) {

  if (monitor_intervals_.back().bytes_total == 0) {
    // This is the first packet of this interval.
    monitor_intervals_.back().first_packet_sent_time = sent_time;
    monitor_intervals_.back().first_packet_number = packet_number;
  }

  if (packet_number < monitor_intervals_.back().last_packet_number) {
    return;
  }
  monitor_intervals_.back().last_packet_sent_time = sent_time;
  monitor_intervals_.back().last_packet_number = packet_number;
  monitor_intervals_.back().bytes_total += bytes;

  //monitor_intervals_.back().pkt_state_map.insert(std::make_pair(packet_number, MI_PACKET_STATE_SENT));
#ifdef DEBUG_INTERVAL_SIZE
  if (monitor_intervals_.back().is_useful) {
    std::cout << "Added packet " << packet_number << " to monitor interval, now " << monitor_intervals_.back().bytes_total << " bytes " << std::endl;
  }
#endif
}

void PccMonitorIntervalQueue::OnCongestionEvent(
    const CongestionVector& acked_packets,
    const CongestionVector& lost_packets,
    int64_t rtt_us,
    uint64_t event_time) {
  if (num_useful_intervals_ == 0) {
    // Skip all the received packets if no intervals are useful.
    return;
  }

  bool has_invalid_utility = false;
  //std::cout << "MIQ: " << num_useful_intervals_ << " useful intervals" << std::endl;
  for (MonitorInterval& interval : monitor_intervals_) {
    if (!interval.is_useful || IsUtilityAvailable(interval, event_time)) {
      // Skips intervals that are not useful, or have available utilities
      continue;
    }

    for (CongestionVector::const_iterator it =
             lost_packets.cbegin();
         it != lost_packets.cend(); ++it) {
      #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
      std::cout << "Lost packet : " << it->seq_no << std::endl;
      #endif
      if (IntervalContainsPacket(interval, it->seq_no)) {
        //std::map<QuicPacketNumber, MiPacketState>::iterator element = interval.pkt_state_map.find(it->seq_no);
        //interval.pkt_state_map.erase(element);
        //interval.pkt_state_map.insert(std::make_pair(it->seq_no, MI_PACKET_STATE_LOST));
        interval.bytes_lost += it->lost_bytes;
        #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
        std::cout << "\tattributed bytes to an interval" << std::endl;
        std::cout << "\tacked " << interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        std::cout << "\tlost " << interval.bytes_lost << "/" << interval.bytes_total << std::endl;
        std::cout << "\ttotal " << interval.bytes_lost + interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        #endif
      }
    }

    for (CongestionVector::const_iterator it =
             acked_packets.cbegin();
         it != acked_packets.cend(); ++it) {
      #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
      std::cout << "Acked packet : " << it->seq_no << std::endl;
      #endif
      if (IntervalContainsPacket(interval, it->seq_no)) {
        //std::map<QuicPacketNumber, MiPacketState>::iterator element = interval.pkt_state_map.find(it->seq_no);
        //interval.pkt_state_map.erase(element);
        //interval.pkt_state_map.insert(std::make_pair(it->seq_no, MI_PACKET_STATE_LOST));
        interval.bytes_acked += it->acked_bytes;
        #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
        std::cout << "\tattributed bytes to an interval" << std::endl;
        std::cout << "\tacked " << interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        std::cout << "\tlost " << interval.bytes_lost << "/" << interval.bytes_total << std::endl;
        std::cout << "\ttotal " << interval.bytes_lost + interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        #endif
      }
    }

    if (IsUtilityAvailable(interval, event_time)) {
      interval.rtt_on_monitor_end_us = rtt_us;
      has_invalid_utility = !CalculateUtility(&interval);
      if (has_invalid_utility) {
        break;
      }
      ++num_available_intervals_;
    }
  }

  num_available_intervals_ = 0;
  for (MonitorInterval& interval : monitor_intervals_) {
    if (interval.is_useful && IsUtilityAvailable(interval, event_time)) {
      if (interval.utility == 0) {
        CalculateUtility(&interval);
      }
      ++num_available_intervals_;
    }
  }

  //std::cout << "MIQ: num_useful = " << num_useful_intervals_ << ", num_avail = " << num_available_intervals_ << " invalid utility = " << has_invalid_utility << std::endl;
  if (num_useful_intervals_ > num_available_intervals_ &&
      !has_invalid_utility) {
    return;
  }

  if (!has_invalid_utility) {

    std::vector<UtilityInfo> utility_info;
    for (const MonitorInterval& interval : monitor_intervals_) {
      if (!interval.is_useful) {
        continue;
      }
      // All the useful intervals should have available utilities now.
      utility_info.push_back(
          UtilityInfo(interval.sending_rate_mbps, interval.utility));
    }

    delegate_->OnUtilityAvailable(utility_info);
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

const MonitorInterval& PccMonitorIntervalQueue::current() const {
  return monitor_intervals_.back();
}

bool PccMonitorIntervalQueue::empty() const {
  return monitor_intervals_.empty();
}

size_t PccMonitorIntervalQueue::size() const {
  return monitor_intervals_.size();
}

bool PccMonitorIntervalQueue::IsUtilityAvailable(
    const MonitorInterval& interval,
    uint64_t event_time) const {
    
    //std::cout << "interval [" << interval.first_packet_number << ", " << interval.last_packet_number << "] ends at " <<
    //    interval.end_time << " (now: " << event_time << ")" << std::endl;
    return (event_time >= interval.end_time && interval.bytes_acked + interval.bytes_lost == interval.bytes_total);
}

bool PccMonitorIntervalQueue::IntervalContainsPacket(
    const MonitorInterval& interval,
    QuicPacketNumber packet_number) const {
  bool result =  (packet_number >= interval.first_packet_number &&
          packet_number <= interval.last_packet_number);
#ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
  std::cout << "Checking for packet " << packet_number << " in interval: [" << interval.first_packet_number << ", " << interval.last_packet_number << "]" << std::endl;
#else
#ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
  std::cout << "Checking for packet " << packet_number << " in interval: [" << interval.first_packet_number << ", " << interval.last_packet_number << "]" << std::endl;
#endif
#endif
  return result;
}

bool PccMonitorIntervalQueue::CalculateUtility(MonitorInterval* interval) {
  if (interval->last_packet_sent_time == interval->first_packet_sent_time) {
    // std::cout << "Invalid utility: single packet in interval" << std::endl;
    // Cannot get valid utility if interval only contains one packet.
    return false;
  }
  const int64_t kMinTransmissionTime = 1l;
  int64_t mi_duration = std::max(
      kMinTransmissionTime,
      (interval->last_packet_sent_time - interval->first_packet_sent_time));

  float rtt_ratio = static_cast<float>(interval->rtt_on_monitor_start_us) /
                    static_cast<float>(interval->rtt_on_monitor_end_us);

  float bytes_acked = static_cast<float>(interval->bytes_acked);
  float bytes_lost = static_cast<float>(interval->bytes_lost);
  float bytes_total = static_cast<float>(interval->bytes_total);
      
  float current_utility =
      (bytes_acked / static_cast<float>(mi_duration) *
           (1.0 -
            1.0 / (1.0 + std::exp(kLossCoefficient *
                             (bytes_lost / bytes_total - kLossTolerance)))) *
           (1.0 - 1.0 / (1.0 + std::exp(kRTTCoefficient * (1.0 - rtt_ratio)))) -
       bytes_lost / static_cast<float>(mi_duration)) *
      1000.0;
  /*
  std::cout << "Utility = " << current_utility << " = " << bytes_acked << " / " << static_cast<float>(mi_duration) << " * "
           << "(1.0 - 1.0 / (1.0 + e^(" << kLossCoefficient << " * (" << bytes_lost << " / " << bytes_total
           << " - " << kLossTolerance << ")))) * (1.0 - 1.0 / (1.0 + e^( * " << kRTTCoefficient
           << " * (1.0 - " << rtt_ratio << ")))) - " << bytes_lost << " / " << static_cast<float>(mi_duration) <<
           ") * 1000" << std::endl;
  */
  interval->utility = current_utility;
  return true;
}
