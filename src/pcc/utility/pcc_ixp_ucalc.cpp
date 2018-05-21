
#include "pcc_ixp_ucalc.h"
#include <cmath>

namespace {
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = 5.0f;
// Coefficient of RTT term in utility function.
const float kRttCoefficient = 1.0/30000.0f;
}  // namespace

float PccIxpUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_mi) {

  static const MonitorIntervalMetric* thpt_metric =
        MonitorIntervalMetric::GetByName("Throughput");
  static const MonitorIntervalMetric* avg_rtt_metric =
        MonitorIntervalMetric::GetByName("AverageRtt");
  static const MonitorIntervalMetric* loss_metric =
        MonitorIntervalMetric::GetByName("LossRate");

  //std::cout << "Getting observations" << std::endl;
  float throughput = thpt_metric->Evaluate(cur_mi);
  float avg_rtt = avg_rtt_metric->Evaluate(cur_mi);
  float loss_rate = loss_metric->Evaluate(cur_mi);
  //std::cout << "Got observations" << std::endl;

  float loss_odds = loss_rate / (1.0 - loss_rate);
  float ixp_utility = throughput / exp(kRttCoefficient * avg_rtt + kLossCoefficient * loss_odds);

  //std::cout << "r = " << cur_monitor_interval.GetTargetSendingRate() << ", u = " << ixp_utility << std::endl;

  float bandwidth_diff_utility = 2.0 * throughput - cur_mi.GetTargetSendingRate();
  
  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", ixp_utility);
  //event.AddValue("Utility", bandwidth_diff_utility);
  event.AddValue("MI Start Time", cur_mi.GetStartTime());
  event.AddValue("Target Rate", cur_mi.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_mi.GetObsSendingRate());
  event.AddValue("Throughput", throughput);
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  log->LogEvent(event);

  if (loss_rate == 1.0) {
      std::cerr << "100% loss!" << std::endl;
  }
  

  //return bandwidth_diff_utility;
  return ixp_utility;
}
