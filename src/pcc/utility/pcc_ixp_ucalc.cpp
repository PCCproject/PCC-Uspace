
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

  float throughput = cur_mi.GetObsThroughput();
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();

  float utility = throughput / exp(kRttCoefficient * avg_rtt + kLossCoefficient * loss_rate);

  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", utility);
  event.AddValue("MI Start Time", cur_mi.GetStartTime());
  event.AddValue("Target Rate", cur_mi.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_mi.GetObsSendingRate());
  event.AddValue("Throughput", throughput);
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  log->LogEvent(event);

  return utility;
}
