#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_

#include <deque>
#include <utility>
#include <vector>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "gfe/quic/core/congestion_control/pcc_monitor_interval.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include "pcc_monitor_interval.h"
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

class PccMonitorIntervalAnalysisGroup {
 public:
  explicit PccMonitorIntervalAnalysisGroup(int size);
  PccMonitorIntervalAnalysisGroup(const PccMonitorIntervalAnalysisGroup&) = delete;
  PccMonitorIntervalAnalysisGroup& operator=(const PccMonitorIntervalAnalysisGroup&) = delete;
  PccMonitorIntervalAnalysisGroup(PccMonitorIntervalAnalysisGroup&&) = delete;
  PccMonitorIntervalAnalysisGroup& operator=(PccMonitorIntervalAnalysisGroup&&) = delete;
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  ~PccMonitorIntervalAnalysisGroup();
  #else
  ~PccMonitorIntervalAnalysisGroup() {}
  #endif

  // Creates a new MonitorInterval and add it to the tail of the
  // monitor interval queue, provided the necessary variables
  // for MonitorInterval initialization.
  void AddNewInterval(MonitorInterval& mi);
  void RemoveOldestInterval();

  bool Full();

  float ComputeUtilityGradient();

 private:
  std::deque<MonitorInterval> monitor_intervals_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_QUEUE_H_
