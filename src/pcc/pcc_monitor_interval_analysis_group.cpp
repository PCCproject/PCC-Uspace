
#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_analysis_group.h"
#else
#include "gfe/quic/core/congestion_control/pcc_monitor_interval_analysis_group.h"
#endif
#else
#include "pcc_monitor_interval_analysis_group.h"
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
using namespace net;
#endif

explicit PccMonitorIntervalAnalysisGroup::PccMonitorIntervalAnalysisGroup(int size);

void PccMonitorIntervalAnalysisGroup::AddNewInterval(MonitorInterval& mi) {
  monitor_intervals_.emplace_back(mi);
}

void PccMonitorIntervalAnalysisGroup::RemoveOldestInterval() {
  monitor_intervals_.pop();
}

bool PccMonitorIntervalAnalysisGroup::Full() {
  return (monitor_intervals_.size() >= size_);
}

float PccMonitorIntervalAnalysisGroup::ComputeUtilityGradient();

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
