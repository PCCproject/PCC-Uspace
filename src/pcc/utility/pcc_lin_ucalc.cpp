
#include "pcc_lin_ucalc.h"
#include <cmath>

namespace {
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = 5.0f;
// Coefficient of RTT term in utility function.
const float kRttCoefficient = 1.0/30000.0f;
}  // namespace

float PccLinearUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_mi) {

  static float prev_sending_rate = 0;

  static const MonitorIntervalMetric* thpt_metric =
        MonitorIntervalMetric::GetByName("Throughput");
  static const MonitorIntervalMetric* avg_rtt_metric =
        MonitorIntervalMetric::GetByName("AverageRtt");
  static const MonitorIntervalMetric* loss_metric =
        MonitorIntervalMetric::GetByName("LossRate");

  float throughput = thpt_metric->Evaluate(cur_mi);
  float avg_rtt = avg_rtt_metric->Evaluate(cur_mi);
  float loss_rate = loss_metric->Evaluate(cur_mi);

  float loss_odds = (loss_rate) / (1.001 - loss_rate);

  float utility = throughput - 1000 * avg_rtt - 1e8 * loss_rate;

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
