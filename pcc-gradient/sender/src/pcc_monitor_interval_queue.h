#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_

#include <deque>
#include <utility>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cmath>

typedef struct CongestionEvent {
    int32_t seq_no;
    int32_t acked_bytes;
    int32_t lost_bytes;
    uint64_t time;
} CongestionEvent;

class PccSender;
struct CongestionEvent;
typedef std::vector<CongestionEvent> CongestionVector;

typedef int32_t QuicPacketNumber;
typedef int64_t QuicByteCount;
typedef int64_t QuicTime;

// MonitorInterval, as the queue's entry struct, stores the information
// of a PCC monitor interval (MonitorInterval) that can be used to
// - pinpoint a acked/lost packet to the corresponding MonitorInterval,
// - calculate the MonitorInterval's utility value.
struct MonitorInterval {
  MonitorInterval();
  MonitorInterval(float sending_rate_mbps, bool is_useful, int64_t rtt_us);
  ~MonitorInterval() {}

  // Sending rate in Mbit/s.
  float sending_rate_mbps;
  // True if calculating utility for this MonitorInterval.
  bool is_useful;

  // Sent time of the first packet.
  QuicTime first_packet_sent_time;
  // Sent time of the last packet.
  QuicTime last_packet_sent_time;

  // PacketNumber of the first sent packet.
  QuicPacketNumber first_packet_number;
  // PacketNumber of the last sent packet.
  QuicPacketNumber last_packet_number;

  // Number of bytes which are sent in total.
  QuicByteCount bytes_total;
  // Number of bytes which have been acked.
  QuicByteCount bytes_acked;
  // Number of bytes which are considered as lost.
  QuicByteCount bytes_lost;

  // RTT when the first packet is sent.
  int64_t rtt_on_monitor_start_us;
  // RTT when all sent packets are either acked or lost.
  int64_t rtt_on_monitor_end_us;

  // Utility value of this MonitorInterval, which is calculated
  // when all sent packets are either acked or lost.
  float utility;
};

// UtilityInfo is used to store <sending_rate_mbps, utility> pairs
struct UtilityInfo {
  UtilityInfo();
  UtilityInfo(float rate, float utility);
  ~UtilityInfo() {}

  float sending_rate_mbps;
  float utility;
};

// A delegate interface for further processing when all
// 'useful' MonitorIntervals' utilities are available.
class PccMonitorIntervalQueueDelegateInterface {
 public:
  virtual ~PccMonitorIntervalQueueDelegateInterface() {}

  virtual void OnUtilityAvailable(
      const std::vector<UtilityInfo>& utility_info) = 0;
};

// PccMonitorIntervalQueue contains a queue of MonitorIntervals.
// New MonitorIntervals are added to the tail of the queue.
// Existing MonitorIntervals are removed from the queue when all
// 'useful' intervals' utilities are available.
class PccMonitorIntervalQueue {
 public:
  explicit PccMonitorIntervalQueue(
      PccSender* delegate);
  PccMonitorIntervalQueue(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue& operator=(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue(PccMonitorIntervalQueue&&) = delete;
  PccMonitorIntervalQueue& operator=(PccMonitorIntervalQueue&&) = delete;
  ~PccMonitorIntervalQueue() {}

  // Creates a new MonitorInterval and add it to the tail of the
  // monitor interval queue, provided the necessary variables
  // for MonitorInterval initialization.
  void EnqueueNewMonitorInterval(float sending_rate_mbps,
                                 bool is_useful,
                                 int64_t rtt_us);

  // Called when a packet belonging to current monitor interval is sent.
  void OnPacketSent(QuicTime sent_time,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes);

  // Called when packets are acked or considered as lost.
  void OnCongestionEvent(
      const CongestionVector& acked_packets,
      const CongestionVector& lost_packets,
      int64_t rtt_us);

  // Returns the most recent MonitorInterval in the tail of the queue
  const MonitorInterval& current() const;
  size_t num_useful_intervals() const { return num_useful_intervals_; }
  bool empty() const;
  size_t size() const;

 private:
  // Returns true if the utility of |interval| is available, i.e.,
  // when all the interval's packets are either acked or lost.
  bool IsUtilityAvailable(const MonitorInterval& interval) const;

  // Retruns true if |packet_number| belongs to |interval|.
  bool IntervalContainsPacket(const MonitorInterval& interval,
                              QuicPacketNumber packet_number) const;

  // Calculates utility for |interval|. Returns true if |interval| has valid
  // utility, false otherwise.
  bool CalculateUtility(MonitorInterval* interval);

  std::deque<MonitorInterval> monitor_intervals_;
  // Number of useful intervals in the queue.
  size_t num_useful_intervals_;
  // Number of useful intervals in the queue with available utilities.
  size_t num_available_intervals_;
  // Delegate interface, not owned.
  PccSender* delegate_;
};

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
