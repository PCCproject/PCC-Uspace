
#include <cmath>
#include "pcc_copa_ucalc.h"

float PccCopaUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_mi) {

  float throughput = cur_mi.GetObsThroughput();
  float rtt_inflation = cur_mi.GetObsRttInflation(); 
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();

  float throughput_contribution = 0;
  float latency_contribution = 0;
  if (loss_rate == 1.0) {
      avg_rtt = last_avg_rtt;
  } else {
      last_avg_rtt = avg_rtt;
  }
  
  float utility = throughput / avg_rtt;
  
  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", utility);
  event.AddValue("MI Start Time", cur_mi.GetStartTime());
  event.AddValue("Target Rate", cur_mi.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_mi.GetObsSendingRate());
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  logger->LogEvent(event); 
  
  return utility;
}
