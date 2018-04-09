#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_

#include <deque>
#include <utility>
#include <vector>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "gfe/quic/core/congestion_control/send_algorithm_interface.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include <cstdint>
#include <cstdlib>
#include <cmath>
#endif

#ifndef QUIC_PORT
typedef int32_t QuicPacketCount;
typedef int32_t QuicPacketNumber;
typedef int64_t QuicByteCount;
typedef int64_t QuicTime;
typedef double  QuicBandwidth;

typedef struct CongestionEvent {
    int32_t packet_number;
    int32_t bytes_acked;
    int32_t bytes_lost;
    uint64_t time;
} CongestionEvent;

typedef CongestionEvent AckedPacket;
typedef CongestionEvent LostPacket;
typedef std::vector<CongestionEvent> AckedPacketVector;
typedef std::vector<CongestionEvent> LostPacketVector;

class PccSender;
typedef PccSender PccMonitorIntervalQueueDelegateInterface;
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
namespace {
bool FLAGS_use_utility_version_2 = true;
}
#else
namespace gfe_quic {
DECLARE_bool(use_utility_version_2);
#endif
using namespace net;
#endif

// PacketRttSample, stores the packet number and its corresponding RTT
struct PacketRttSample {
  PacketRttSample();
  #ifdef QUIC_PORT
  PacketRttSample(QuicPacketNumber packet_number, QuicTime::Delta rtt);
  #else
  PacketRttSample(QuicPacketNumber packet_number, QuicTime rtt);
  #endif
  ~PacketRttSample() {}

  // Packet number of the sampled packet.
  QuicPacketNumber packet_number;
  // RTT corresponding to the sampled packet.
  #ifdef QUIC_PORT
  QuicTime::Delta sample_rtt;
  #else
  QuicTime sample_rtt;
  #endif
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
                  int64_t rtt_us,
                  QuicTime end_time);
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  explicit MonitorInterval(const MonitorInterval&);
  #endif
  #ifdef QUIC_PORT_LOCAL
  ~MonitorInterval();
  #else
  ~MonitorInterval() {}
  #endif

  // Sending rate.
  QuicBandwidth sending_rate;
  // True if calculating utility for this MonitorInterval.
  bool is_useful;
  // The tolerable rtt fluctuation ratio.
  float rtt_fluctuation_tolerance_ratio;
  // The end time for this monitor interval in microseconds.
  QuicTime end_time;

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

  // RTT when the first packet is sent.
  int64_t rtt_on_monitor_start_us;
  // RTT when all sent packets are either acked or lost.
  int64_t rtt_on_monitor_end_us;

  // Utility value of this MonitorInterval, which is calculated
  // when all sent packets are either acked or lost.
  float utility;

  // The number of packets in this monitor interval.
  int n_packets;
  // A sample of the RTT for each packet.
  std::vector<PacketRttSample> packet_rtt_samples;
};

// UtilityInfo is used to store <sending_rate, utility> pairs
struct UtilityInfo {
  UtilityInfo();
  UtilityInfo(QuicBandwidth rate, float utility);
  ~UtilityInfo() {}

  QuicBandwidth sending_rate;
  float utility;
};

#ifdef QUIC_PORT
// A delegate interface for further processing when all
// 'useful' MonitorIntervals' utilities are available.
class PccMonitorIntervalQueueDelegateInterface {
 public:
  virtual ~PccMonitorIntervalQueueDelegateInterface() {}

  virtual void OnUtilityAvailable(
      const std::vector<UtilityInfo>& utility_info) = 0;
};
#endif

// PccMonitorIntervalQueue contains a queue of MonitorIntervals.
// New MonitorIntervals are added to the tail of the queue.
// Existing MonitorIntervals are removed from the queue when all
// 'useful' intervals' utilities are available.
class PccMonitorIntervalQueue {
 public:
  explicit PccMonitorIntervalQueue(
      #ifdef QUIC_PORT
      PccMonitorIntervalQueueDelegateInterface* delegate);
      #else
      PccSender* delegate);
      #endif
  PccMonitorIntervalQueue(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue& operator=(const PccMonitorIntervalQueue&) = delete;
  PccMonitorIntervalQueue(PccMonitorIntervalQueue&&) = delete;
  PccMonitorIntervalQueue& operator=(PccMonitorIntervalQueue&&) = delete;
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  ~PccMonitorIntervalQueue();
  #else
  ~PccMonitorIntervalQueue() {}
  #endif

  // Creates a new MonitorInterval and add it to the tail of the
  // monitor interval queue, provided the necessary variables
  // for MonitorInterval initialization.
  void EnqueueNewMonitorInterval(QuicBandwidth sending_rate,
                                 bool is_useful,
                                 float rtt_fluctuation_tolerance_ratio,
                                 int64_t rtt_us,
                                 QuicTime end_time);

  // Called when a packet belonging to current monitor interval is sent.
  void OnPacketSent(QuicTime sent_time,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes);

  // Called when packets are acked or considered as lost.
  void OnCongestionEvent(const AckedPacketVector& acked_packets,
                         const LostPacketVector& lost_packets,
                         int64_t rtt_us,
                         QuicTime event_time);

  // Called when RTT inflation ratio is greater than
  // max_rtt_fluctuation_tolerance_ratio_in_starting.
  void OnRttInflationInStarting();

  // Returns the most recent MonitorInterval in the tail of the queue
  const MonitorInterval& current() const;
  size_t num_useful_intervals() const { return num_useful_intervals_; }
  size_t num_available_intervals() const { return num_available_intervals_; }
  bool empty() const;
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  size_t size() const { return monitor_intervals_.size(); }
  #else
  size_t size() const;
  #endif

 private:
  // Returns true if the utility of |interval| is available, i.e.,
  // when all the interval's packets are either acked or lost.
  bool IsUtilityAvailable(const MonitorInterval& interval, QuicTime cur_time) const;

  // Retruns true if |packet_number| belongs to |interval|.
  bool IntervalContainsPacket(const MonitorInterval& interval,
                              QuicPacketNumber packet_number) const;

  #ifdef QUIC_PORT
  // Calculates utility for |interval|. Returns true if |interval| has valid
  // utility, false otherwise.
  bool CalculateUtility(MonitorInterval* interval);
  // Calculates utility for |interval| using version-2 utility function. Returns
  // true if |interval| has valid utility, false otherwise.
  bool CalculateUtility2(MonitorInterval* interval);
  #else
  // Calculates utility for |interval|. Returns true if |interval| has valid
  // utility, false otherwise.
  bool CalculateUtility(MonitorInterval* interval);
  #endif

  std::deque<MonitorInterval> monitor_intervals_;
  // Number of useful intervals in the queue.
  size_t num_useful_intervals_;
  // Number of useful intervals in the queue with available utilities.
  size_t num_available_intervals_;
  // Delegate interface, not owned.
  PccMonitorIntervalQueueDelegateInterface* delegate_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
