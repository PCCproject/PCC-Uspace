#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_

#include <deque>
#include <utility>
#include <vector>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/quic/core/congestion_control/pcc_monitor_interval.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "gfe/quic/core/congestion_control/send_algorithm_interface.h"
#include "gfe/quic/core/congestion_control/pcc_monitor_interval.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include "pcc_mi.h"
#include <cstdint>
#include <cstdlib>
#include <cmath>
#endif

#ifndef QUIC_PORT
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
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
namespace {
}
#else
namespace gfe_quic {
#endif
using namespace net;
#endif

// PccMonitorIntervalQueue contains a queue of MonitorIntervals.
// New MonitorIntervals are added to the tail of the queue.
// Existing MonitorIntervals are removed from the queue when all
// 'useful' intervals' utilities are available.
class PccMonitorIntervalQueue {
 public:
  explicit PccMonitorIntervalQueue();
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
  void Push(MonitorInterval mi);

  // Called when a packet belonging to current monitor interval is sent.
  void OnPacketSent(QuicTime sent_time,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes);

  // Called when packets are acked or considered as lost.
  void OnCongestionEvent(const AckedPacketVector& acked_packets,
                         const LostPacketVector& lost_packets,
                         int64_t rtt_us,
                         QuicTime event_time);

  // Returns the most recent MonitorInterval in the tail of the queue
  const MonitorInterval& Current() const;
  bool HasFinishedInterval(QuicTime cur_time);
  MonitorInterval Pop();
  
  bool Empty() const;
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  size_t Size() const { return monitor_intervals_.size(); }
  #else
  size_t Size() const;
  #endif

 private:
  std::deque<MonitorInterval> monitor_intervals_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
