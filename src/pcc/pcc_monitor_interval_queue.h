#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_

#include "quic_types/quic_bandwidth.h"

#include <deque>
#include <utility>
#include <vector>

//#include "third_party/quic/core/congestion_control/send_algorithm_interface.h"
//#include "third_party/quic/core/quic_time.h"
//#include "third_party/quic/core/quic_types.h"

// namespace quic {

// PacketRttSample, stores packet number and the corresponding RTT.
struct PacketRttSample {
  PacketRttSample();
  PacketRttSample(QuicPacketNumber packet_number,
                  QuicTime::Delta rtt,
                  QuicTime ack_timestamp,
                  bool reliability,
                  bool gradient_reliability);
  ~PacketRttSample() {}

  // Packet number of the sampled packet.
  QuicPacketNumber packet_number;
  // RTT corresponding to the sampled packet.
  QuicTime::Delta sample_rtt;
  // Timestamp when the ACK of the sampled packet is received.
  QuicTime ack_timestamp;

  // Flag representing if the RTT sample is reliable for utility calculation.
  bool is_reliable;
  bool is_reliable_for_gradient_calculation;
};

struct LostPacketSample {
  LostPacketSample();
  LostPacketSample(QuicPacketNumber packet_number,
                  QuicByteCount bytes);

  QuicPacketNumber packet_number;
  QuicByteCount bytes;
};

// MonitorInterval, as the queue's entry struct, stores the information
// of a PCC monitor interval (MonitorInterval) that can be used to
// - pinpoint a acked/lost packet to the corresponding MonitorInterval,
// - calculate the MonitorInterval's utility value.
struct MonitorInterval {
  MonitorInterval();
  MonitorInterval(QuicBandwidth sending_rate,
                  bool is_useful,
                  float rtt_fluctuation_tolerance_ratio,
                  QuicTime::Delta rtt);
  ~MonitorInterval() {}

  // Sending rate.
  QuicBandwidth sending_rate;
  // True if calculating utility for this MonitorInterval.
  bool is_useful;
  // The tolerable rtt fluctuation ratio.
  float rtt_fluctuation_tolerance_ratio;

  // Sent time of the first packet.
  QuicTime first_packet_sent_time;
  // Sent time of the last packet.
  QuicTime last_packet_sent_time;

  // PacketNumber of the first sent packet.
  QuicPacketNumber first_packet_number;
  // PacketNumber of the last sent packet.
  QuicPacketNumber last_packet_number;

  // Number of bytes which are sent in total.
  QuicByteCount bytes_sent;
  // Number of bytes which have been acked.
  QuicByteCount bytes_acked;
  // Number of bytes which are considered as lost.
  QuicByteCount bytes_lost;

  // Smoothed RTT when the first packet is sent.
  QuicTime::Delta rtt_on_monitor_start;
  // RTT when all sent packets are either acked or lost.
  QuicTime::Delta rtt_on_monitor_end;
  // Minimum RTT seen by PCC sender.
  QuicTime::Delta min_rtt;

  // Interval since previous sent packet for each packet in the interval.
  std::vector<QuicTime::Delta> packet_sent_intervals;
  // Packet RTT sample for each sent packet in the monitor interval.
  std::vector<PacketRttSample> packet_rtt_samples;
  // Lost packet sample for each lost packet in the monitor interval.
  std::vector<LostPacketSample> lost_packet_samples;

  size_t num_reliable_rtt;
  size_t num_reliable_rtt_for_gradient_calculation;
  // True if the interval has enough number of reliable RTT samples.
  bool has_enough_reliable_rtt;

  // True only if the monitor duration is doubled due to lack of reliable RTTs.
  bool is_monitor_duration_extended;
};

// A delegate interface for further processing when all
// 'useful' MonitorIntervals' utilities are available.
class PccMonitorIntervalQueueDelegateInterface {
 public:
  virtual ~PccMonitorIntervalQueueDelegateInterface() {}

  virtual void OnUtilityAvailable(
      const std::vector<const MonitorInterval *>& useful_intervals,
      QuicTime event_time) = 0;
};

// PccMonitorIntervalQueue contains a queue of MonitorIntervals.
// New MonitorIntervals are added to the tail of the queue.
// Existing MonitorIntervals are removed from the queue when all
// 'useful' intervals' utilities are available.
class PccMonitorIntervalQueue {
 public:
  explicit PccMonitorIntervalQueue(
      PccMonitorIntervalQueueDelegateInterface* delegate);
  PccMonitorIntervalQueue(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue& operator=(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue(PccMonitorIntervalQueue&&) = delete;
  PccMonitorIntervalQueue& operator=(PccMonitorIntervalQueue&&) = delete;
  ~PccMonitorIntervalQueue() {}

  // Creates a new MonitorInterval and add it to the tail of the
  // monitor interval queue, provided the necessary variables
  // for MonitorInterval initialization.
  void EnqueueNewMonitorInterval(QuicBandwidth sending_rate,
                                 bool is_useful,
                                 float rtt_fluctuation_tolerance_ratio,
                                 QuicTime::Delta rtt);

  // Called when a packet belonging to current monitor interval is sent.
  void OnPacketSent(QuicTime sent_time,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes,
                    QuicTime::Delta sent_interval);

  // Called when packets are acked or considered as lost.
  void OnCongestionEvent(const AckedPacketVector& acked_packets,
                         const LostPacketVector& lost_packets,
                         QuicTime::Delta avg_rtt,
                         QuicTime::Delta latest_rtt,
                         QuicTime::Delta min_rtt,
                         QuicTime event_time,
                         QuicTime::Delta ack_interval);

  // Called when RTT inflation ratio is greater than
  // max_rtt_fluctuation_tolerance_ratio_in_starting.
  void OnRttInflationInStarting();

  // Returns the fisrt MonitorInterval in the front of the queue. The caller
  // needs to make sure the queue is not empty before calling this function.
  const MonitorInterval& front() const;
  // Returns the most recent MonitorInterval in the tail of the queue. The
  // caller needs to make sure the queue is not empty before calling this
  // function.
  const MonitorInterval& current() const;
  // Mark the most recent MonitorInterval as already extended.
  void extend_current_interval();
  size_t num_useful_intervals() const { return num_useful_intervals_; }
  size_t num_available_intervals() const { return num_available_intervals_; }
  bool empty() const;
  size_t size() const;

 private:
  // Returns true if the utility of |interval| is available, i.e.,
  // when all the interval's packets are either acked or lost.
  bool IsUtilityAvailable(const MonitorInterval& interval) const;

  // Retruns true if |packet_number| belongs to |interval|.
  bool IntervalContainsPacket(const MonitorInterval& interval,
                              QuicPacketNumber packet_number) const;

  // Returns true if the utility of |interval| is invalid, i.e., if it only
  // contains a single sent packet.
  bool HasInvalidUtility(const MonitorInterval* interval) const;

  std::deque<MonitorInterval> monitor_intervals_;
  // Vector of acked packets with pending RTT reliability.
  std::vector<AckedPacket> pending_acked_packets_;
  // Latest RTT corresponding to pending acked packets.
  QuicTime::Delta pending_rtt_;
  // Average RTT corresponding to pending acked packets.
  QuicTime::Delta pending_avg_rtt_;
  // ACK interval corresponding to pending acked packets.
  QuicTime::Delta pending_ack_interval_;
  // ACK reception time corresponding to pending acked packets.
  QuicTime pending_event_time_;

  bool burst_flag_;

  // EWMA of ratio between two consecutive ACK intervals, i.e., interval between
  // reception time of two consecutive ACKs.
  float avg_interval_ratio_;

  // Number of useful intervals in the queue.
  size_t num_useful_intervals_;
  // Number of useful intervals in the queue with available utilities.
  size_t num_available_intervals_;
  // Delegate interface, not owned.
  PccMonitorIntervalQueueDelegateInterface* delegate_;
};

// }  // namespace quic

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
