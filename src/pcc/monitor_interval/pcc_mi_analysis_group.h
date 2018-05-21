#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_ANALYSIS_GROUP_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_ANALYSIS_GROUP_H_

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
#include "pcc_mi.h"
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

struct ComputedGradient {
    QuicTime time;
    QuicBandwidth rate;
    float gradient;
};

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

  std::deque<MonitorInterval>::iterator Begin();
  std::deque<MonitorInterval>::iterator End();

  // Creates a new MonitorInterval and add it to the tail of the
  // monitor interval queue, provided the necessary variables
  // for MonitorInterval initialization.
  void AddNewInterval(MonitorInterval& mi);
  void RemoveOldestInterval();

  MonitorInterval& GetMostRecentInterval();

  bool Full();
  bool Empty();

  float ComputeWeightedUtilityGradient(QuicTime cur_time, float target_rate,
        float time_decay, float rate_decay);
  float ComputeUtilityGradient();

 private:
  void ComputeUtilityGradientVector_(std::vector<ComputedGradient>* gradients);
  
  std::deque<MonitorInterval> monitor_intervals_;
  int size_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_ANALYSIS_GROUP_H_

