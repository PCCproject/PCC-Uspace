
#include "pcc_lin_ucalc.h"

float PccLinearUtilityCalculator::CalculateUtility(MonitorInterval& cur_mi) {
  
  float throughput = cur_mi.GetObsThroughput();
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();

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
