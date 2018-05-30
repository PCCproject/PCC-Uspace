
#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>
#include <cmath>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_analysis_group.h"
#else
#include "gfe/quic/core/congestion_control/pcc_monitor_interval_analysis_group.h"
#endif
#else
#include "pcc_mi_analysis_group.h"
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

std::deque<MonitorInterval>::iterator PccMonitorIntervalAnalysisGroup::Begin() {
    return monitor_intervals_.begin();
}

std::deque<MonitorInterval>::iterator PccMonitorIntervalAnalysisGroup::End() {
    return monitor_intervals_.end();
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
  
MonitorInterval& PccMonitorIntervalAnalysisGroup::GetMostRecentInterval() {
  return monitor_intervals_.back();
}

bool PccMonitorIntervalAnalysisGroup::Empty() {
  return monitor_intervals_.empty();
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

void PccMonitorIntervalAnalysisGroup::ComputeUtilityGradientVector_(std::vector<ComputedGradient>* gradients) {
  
  std::deque<MonitorInterval>::iterator it = monitor_intervals_.begin();
  MonitorInterval& last_mi = *it;
  for (std::deque<MonitorInterval>::iterator it = monitor_intervals_.begin();
          it != monitor_intervals_.end(); ++it) {

    MonitorInterval& cur_mi = *it;
    ComputedGradient cg;
    cg.gradient = 0.0;
    cg.time = (last_mi.GetStartTime() + cur_mi.GetStartTime()) / 2.0;
    cg.rate = (last_mi.GetTargetSendingRate() + cur_mi.GetTargetSendingRate()) / 2.0;
    if (abs(last_mi.GetTargetSendingRate() - cur_mi.GetTargetSendingRate()) > 4096.0) {
        cg.gradient = (last_mi.GetObsUtility() - cur_mi.GetObsUtility()) / (last_mi.GetTargetSendingRate() -
                cur_mi.GetTargetSendingRate());
    } else {
        cg.gradient = 0;
    }
    gradients->push_back(cg);
    last_mi = cur_mi;
  }
    
}

float PccMonitorIntervalAnalysisGroup::ComputeWeightedUtilityGradient(QuicTime cur_time, float target_rate, float
time_decay=1.0/1000000.0, float rate_decay=10.0/1000000.0) {
    std::vector<ComputedGradient> gradients;
    ComputeUtilityGradientVector_(&gradients);
    float result = 0.0;
    float n_gradients = gradients.size();
    for (std::vector<ComputedGradient>::iterator it = gradients.begin(); it != gradients.end(); ++it) {
        double weight = exp((cur_time - it->time) * time_decay +
                abs(target_rate - it->rate) * rate_decay);
        result += it->gradient * weight / n_gradients;
    }
    return result;
}

/*
double PccMonitorIntervalAnalysisGroup::ComputeUtilityGradientVariance() {
    if (monitor_intervals_.size() < 2) {
        return 0;
    }
    
    std::vector<ComputedGradient> gradients;
    ComputeUtilityGradientVector_(&gradients);
    double graident = ComputeUtilityGradient();
    double n_gradients = gradients.size();
    double result = 0;
    for (std::vector<ComputedGradient>::iterator it = gradients.begin(); it != gradients.end(); ++it) {
        result += (it->gradient - gradient) * (it->gradient - gradient);
    }
    return result / n_gradients;
}
*/

float PccMonitorIntervalAnalysisGroup::ComputeUtilityGradient() {

    if (monitor_intervals_.size() < 2) {
        return 0;
    }
    
    std::vector<ComputedGradient> gradients;
    ComputeUtilityGradientVector_(&gradients);
    double n_gradients = gradients.size();
    double result = 0;
    for (std::vector<ComputedGradient>::iterator it = gradients.begin(); it != gradients.end(); ++it) {
        result += it->gradient;
    }
    return result / n_gradients;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
