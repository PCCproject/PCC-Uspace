
#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

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

PccMonitorIntervalAnalysisGroup::PccMonitorIntervalAnalysisGroup(int size) {
  size_ = size;
}

void PccMonitorIntervalAnalysisGroup::AddNewInterval(MonitorInterval& mi) {
  monitor_intervals_.emplace_back(mi);
}

void PccMonitorIntervalAnalysisGroup::RemoveOldestInterval() {
  monitor_intervals_.pop_front();
}

bool PccMonitorIntervalAnalysisGroup::Full() {
  return (monitor_intervals_.size() >= size_);
}

static double Slope_(std::vector<double>& x, std::vector<double>& y) {
    double n = x.size();

    double avgX = accumulate(x.begin(), x.end(), 0.0) / n;
    double avgY = accumulate(y.begin(), y.end(), 0.0) / n;

    double numerator = 0.0;
    double denominator = 0.0;

    for (int i = 0; i < n; ++i){
        numerator += (x[i] - avgX) * (y[i] - avgY);
        denominator += (x[i] - avgX) * (x[i] - avgX);
    }

    return numerator / denominator;
}

float PccMonitorIntervalAnalysisGroup::ComputeUtilityGradient() {
  
  std::vector<double> rates, utilities;
  
  // Compute the slope of a linear regression over the current monitor intervals.
  for (std::deque<MonitorInterval>::iterator it = monitor_intervals_.begin();
          it != monitor_intervals_.end(); ++it) {

    rates.push_back(it->GetObsSendingRate());
    utilities.push_back(it->GetObsUtility());

  }

  double slope = Slope_(rates, utilities);
  return slope;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
