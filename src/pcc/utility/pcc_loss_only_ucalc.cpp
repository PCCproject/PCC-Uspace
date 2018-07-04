
#include "pcc_loss_only_ucalc.h"

namespace {
// Number of probing MonitorIntervals necessary for Probing.
//const size_t kRoundsPerProbing = 4;
// Tolerance of loss rate by utility function.
const float kLossTolerance = 0.05f;
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = -1000.0f;
// Coefficient of RTT term in utility function.
const float kRTTCoefficient = -200.0f;
// Alpha factor in the utility function.
const float kAlpha = 1;
// An exponent in the utility function.
const float kExponent = 0.9;
// Number of bits in a megabit.
const float kBitsPerMegabit = 1024 * 1024;
}  // namespace

float PccLossOnlyUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_mi) {

  float throughput = cur_mi.GetObsThroughput();
  float sending_rate_mbps = cur_mi.GetObsSendingRate() / kBitsPerMegabit;
  float rtt_inflation = cur_mi.GetObsRttInflation(); 
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();

  float loss_contribution = 11.35 * loss_rate;
  float sending_factor = kAlpha * pow(sending_rate_mbps, kExponent);
  loss_contribution *= -1.0 * sending_rate_mbps;
  
  float utility = sending_factor + loss_contribution;
  
  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", utility);
  event.AddValue("MI Start Time", cur_mi.GetStartTime());
  event.AddValue("Target Rate", cur_mi.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_mi.GetObsSendingRate());
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  log->LogEvent(event); 
  
  return utility;
}
