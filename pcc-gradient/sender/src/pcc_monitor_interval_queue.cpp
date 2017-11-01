#include "pcc_monitor_interval_queue.h"
#include "pcc_sender.h"
#include <iostream>
      
      
#define DEBUG_UTILITY_CALC
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
// Number of microseconds per second.
const float kMicrosecondsPerSecond = 1000000.0f;
// Coefficienty of the latency term in the utility function.
const float kLatencyCoefficient = 0;//1;
// Alpha factor in the utility function.
const float kAlpha = 1;//0.2;
// An exponent in the utility function.
const float kExponent = 0.9;//1.5;
}  // namespace

MonitorInterval::MonitorInterval()
    : first_packet_sent_time(0),
      last_packet_sent_time(0),
      first_packet_number(0),
      last_packet_number(0),
      bytes_total(0),
      bytes_acked(0),
      bytes_lost(0),
      end_time(0.0),
      utility(0.0),
      n_packets(0){}

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
      end_time(end_time),
      utility(0.0),
      n_packets(0){}

void MonitorInterval::DumpMiPacketStates() {
  for (std::map<QuicPacketNumber, MiPacketState>::iterator it = pkt_state_map.begin(); it != pkt_state_map.end(); ++it) {
    if (it->second == MI_PACKET_STATE_SENT) {
      std::cerr << it->first << " in state SENT" << std::endl;
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
  //std::cerr << "Added new monitor interval" << std::endl;
  if (is_useful) {
    ++num_useful_intervals_;
    //std::cerr << "\tInterval is useful! (now have " << num_useful_intervals_ << ")" << std::endl;
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
    monitor_intervals_.back().last_packet_sent_time = sent_time;
    monitor_intervals_.back().last_packet_number = packet_number;
  }

  if (packet_number < monitor_intervals_.back().last_packet_number) {
    std::cerr << "Attempted to add packet " << packet_number << " but number is too low" << std::endl;
    return;
  }
  //std::cerr << "Sending packet #" << packet_number << std::endl;
  //std::cerr << "Last packet sent: " << monitor_intervals_.back().last_packet_number << " next packet: " << packet_number << std::endl;
  for (int i = 0; monitor_intervals_.back().last_packet_number + i + 1 < packet_number; ++i) {
    monitor_intervals_.back().sent_times.push_back(0);
    monitor_intervals_.back().packet_rtts.push_back(0l);
    //std::cerr << "Inserting blank interval: " << i << std::endl;
  }
  //std::cerr << "Sending packet at time " << sent_time << std::endl;
  monitor_intervals_.back().last_packet_sent_time = sent_time;
  monitor_intervals_.back().last_packet_number = packet_number;
  monitor_intervals_.back().bytes_total += bytes;
  monitor_intervals_.back().sent_times.push_back(sent_time);
  monitor_intervals_.back().packet_rtts.push_back(0l);
  ++monitor_intervals_.back().n_packets;

  //monitor_intervals_.back().pkt_state_map.insert(std::make_pair(packet_number, MI_PACKET_STATE_SENT));
#ifdef DEBUG_INTERVAL_SIZE
  if (monitor_intervals_.back().is_useful) {
    std::cerr << "Added packet " << packet_number << " to monitor interval, now " << monitor_intervals_.back().bytes_total << " bytes " << std::endl;
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
  //std::cerr << "MIQ: " << num_useful_intervals_ << " useful intervals" << std::endl;
  for (MonitorInterval& interval : monitor_intervals_) {
    if (!interval.is_useful || IsUtilityAvailable(interval, event_time)) {
      // Skips intervals that are not useful, or have available utilities
      continue;
    }

    for (CongestionVector::const_iterator it =
             lost_packets.cbegin();
         it != lost_packets.cend(); ++it) {
      #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
      std::cerr << "Lost packet : " << it->seq_no << std::endl;
      #endif
      if (IntervalContainsPacket(interval, it->seq_no)) {
        //std::map<QuicPacketNumber, MiPacketState>::iterator element = interval.pkt_state_map.find(it->seq_no);
        //interval.pkt_state_map.erase(element);
        //interval.pkt_state_map.insert(std::make_pair(it->seq_no, MI_PACKET_STATE_LOST));
        interval.bytes_lost += it->lost_bytes;
        #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
        std::cerr << "\tattributed bytes to an interval" << std::endl;
        std::cerr << "\tacked " << interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        std::cerr << "\tlost " << interval.bytes_lost << "/" << interval.bytes_total << std::endl;
        std::cerr << "\ttotal " << interval.bytes_lost + interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        #endif
      }
    }

    for (CongestionVector::const_iterator it =
             acked_packets.cbegin();
         it != acked_packets.cend(); ++it) {
      #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
      std::cerr << "Acked packet : " << it->seq_no << std::endl;
      #endif
      if (IntervalContainsPacket(interval, it->seq_no)) {
        //std::map<QuicPacketNumber, MiPacketState>::iterator element = interval.pkt_state_map.find(it->seq_no);
        //interval.pkt_state_map.erase(element);
        //interval.pkt_state_map.insert(std::make_pair(it->seq_no, MI_PACKET_STATE_LOST));
        interval.bytes_acked += it->acked_bytes;
        interval.packet_rtts[it->seq_no - interval.first_packet_number] = rtt_us;
        #ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
        std::cerr << "\tattributed bytes to an interval" << std::endl;
        std::cerr << "\tacked " << interval.bytes_acked << "/" << interval.bytes_total << std::endl;
        std::cerr << "\tlost " << interval.bytes_lost << "/" << interval.bytes_total << std::endl;
        std::cerr << "\ttotal " << interval.bytes_lost + interval.bytes_acked << "/" << interval.bytes_total << std::endl;
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

  //std::cerr << "MIQ: num_useful = " << num_useful_intervals_ << ", num_avail = " << num_available_intervals_ << " invalid utility = " << has_invalid_utility << std::endl;
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
    
    //std::cerr << "interval [" << interval.first_packet_number << ", " << interval.last_packet_number << "] ends at " <<
    //    interval.end_time << " (now: " << event_time << ")" << std::endl;
    return (event_time >= interval.end_time && interval.bytes_acked + interval.bytes_lost == interval.bytes_total);
}

bool PccMonitorIntervalQueue::IntervalContainsPacket(
    const MonitorInterval& interval,
    QuicPacketNumber packet_number) const {
  bool result =  (packet_number >= interval.first_packet_number &&
          packet_number <= interval.last_packet_number);
#ifdef DEBUG_MONITOR_INTERVAL_QUEUE_LOSS
  std::cerr << "Checking for packet " << packet_number << " in interval: [" << interval.first_packet_number << ", " << interval.last_packet_number << "]" << std::endl;
#else
#ifdef DEBUG_MONITOR_INTERVAL_QUEUE_ACKS
  std::cerr << "Checking for packet " << packet_number << " in interval: [" << interval.first_packet_number << ", " << interval.last_packet_number << "]" << std::endl;
#endif
#endif
  return result;
}

bool PccMonitorIntervalQueue::CalculateUtility(MonitorInterval* interval) {
  if (interval->last_packet_sent_time == interval->first_packet_sent_time) {
    // std::cerr << "Invalid utility: single packet in interval" << std::endl;
    // Cannot get valid utility if interval only contains one packet.
    return false;
  }
  const int64_t kMinTransmissionTime = 1l;
  int64_t mi_duration = std::max(
      kMinTransmissionTime,
      (interval->last_packet_sent_time - interval->first_packet_sent_time));

  float mi_time = static_cast<float>(mi_duration) / kMicrosecondsPerSecond;
  float bytes_lost = static_cast<float>(interval->bytes_lost);
  float bytes_total = static_cast<float>(interval->bytes_total);
     
  double avg_time = 0.0;
  double avg_rtt = 0.0;
  for (int i = 0; i < interval->n_packets; ++i) {
    if (interval->packet_rtts[i] != 0l) {
      //std::cerr << "Packet Sent Time = " << interval->sent_times[i] << std::endl;
      avg_time += interval->sent_times[i];
      avg_rtt += interval->packet_rtts[i];
    }
  }
  avg_time /= (double)interval->n_packets;
  avg_rtt /= (double)interval->n_packets;

  //std::cerr << "Average Sent Time = " << avg_time << std::endl;
  //std::cerr << "Average RTT       = " << avg_rtt << std::endl;

  double numerator = 0.0;
  double denominator = 0.0;
  for (int i = 0; i < interval->n_packets; ++i) {
    if (interval->packet_rtts[i] != 0l) {
      //std::cerr << "Packet Time Diff = " << (double)interval->sent_times[i] - avg_time << std::endl;
      numerator += (interval->sent_times[i] - avg_time) * (interval->packet_rtts[i] - avg_rtt);
      denominator += (interval->sent_times[i] - avg_time) * (interval->sent_times[i] - avg_time);
    }
  }

  float latency_info = numerator / denominator;

  float loss_rate = bytes_lost / bytes_total;
  float rtt_penalty = int(int(latency_info * 100) / 100.0 * 100) / 2 * 2/ 100.0;
  float loss_contribution = interval->n_packets * (11.35 * (pow((1 + loss_rate), 1) - 1));
  if (loss_rate <= 0.03) {
    loss_contribution = interval->n_packets * (1 * (pow((1 + loss_rate), 1) - 1));
  }
  float rtt_contribution = kLatencyCoefficient * 11330 * bytes_total * (pow(rtt_penalty, 1));

  float throughput_factor = kAlpha * pow(8 * bytes_total/1024/1024/mi_time, kExponent); 

  float current_utility = throughput_factor - (1*loss_contribution +
  rtt_contribution)*(8 * bytes_total / static_cast<float>(interval->n_packets))/1024/1024/mi_time;

#ifdef DEBUG_UTILITY_CALC
  std::cerr << "Calculate utility:" << std::endl;
  std::cerr << "\tutility           = " << current_utility << std::endl;
  std::cerr << "\tn_packets         = " << interval->n_packets << std::endl;
  std::cerr << "\tsend_rate         = " << bytes_total * 8.0f / (mi_time * 1000000.0f) << std::endl;
  std::cerr << "\tthroughput        = " << (bytes_total - bytes_lost) * 8.0f / (mi_time * 1000000.0f) << std::endl;
  std::cerr << "\tthroughput factor = " << throughput_factor << std::endl;
  std::cerr << "\tavg_rtt           = " << avg_rtt << std::endl;
  std::cerr << "\tlatency_info      = " << latency_info << std::endl;
  std::cerr << "\t\tnumerator       = " << numerator << std::endl;
  std::cerr << "\t\tdenominator     = " << denominator << std::endl;
  std::cerr << "\trtt_contribution  = " << rtt_contribution << std::endl;
  std::cerr << "\tloss_rate         = " << loss_rate << std::endl;
  std::cerr << "\tloss_contribution = " << loss_contribution << std::endl;
#endif

  interval->utility = current_utility;
  return true;
}
