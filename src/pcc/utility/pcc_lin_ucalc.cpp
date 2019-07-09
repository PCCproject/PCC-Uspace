
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
  
  float throughput = cur_mi.GetObsThroughput();
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();

  if (loss_rate < 1.0) {
      last_rtt = avg_rtt;
  } else {
      avg_rtt = last_rtt;
  }

  float thpt_pkt_per_sec = throughput / (8.0 * 1500);
  float rtt_sec = avg_rtt / 1000000.0;

  float utility = pow(throughput, 0.9) - 1000 * avg_rtt - 11.35 * throughput * loss_rate;
  utility = throughput - 1000 * avg_rtt - 1e8 * loss_rate;
  utility = 10.0 * thpt_pkt_per_sec - 1000.0 * rtt_sec - 2000.0 * loss_rate;

  //std::cout << "LINEAR CALC! Rtt: " << rtt_sec << std::endl;

  //utility = -1 * abs(cur_mi.GetObsSendingRate() - 1e7);

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
