
#include "pcc_vivace_ucalc.h"

namespace {
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = 1e6f;
// Coefficient of RTT term in utility function.
const float kRTTCoefficient = 900.0f;
// An exponent in the utility function.
const float kExponent = 0.9;
// Number of bits in a megabit.
const float kBitsPerMegabit = 1024 * 1024;
// Filter to remove low-magnitude rtt inflation noise.
const float kRTTFilter = 0.02;
}  // namespace

float PccVivaceUtilityCalculator::CalculateUtility(MonitorInterval& cur_mi) {

  float throughput = cur_mi.GetObsThroughput();
  float sending_rate_mbps = cur_mi.GetObsSendingRate() / kBitsPerMegabit;
  float raw_rtt_inflation = cur_mi.GetObsRttInflation(); 
  float rtt_inflation = raw_rtt_inflation;
  float avg_rtt = cur_mi.GetObsRtt();
  float loss_rate = cur_mi.GetObsLossRate();
  float send_dur = cur_mi.GetObsSendDur();
  float recv_dur = cur_mi.GetObsRecvDur();

  if (rtt_inflation < kRTTFilter) {
      rtt_inflation = 0.0;
  }

  float rtt_contribution = -1.0 * kRTTCoefficient * rtt_inflation * sending_rate_mbps;
  float loss_contribution = -1.0 * kLossCoefficient * loss_rate;
  float sending_factor = pow(sending_rate_mbps, kExponent);
  
  float utility = sending_factor + loss_contribution + rtt_contribution;
  
  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", utility);
  event.AddValue("MI Start Time", cur_mi.GetStartTime());
  event.AddValue("Target Rate", cur_mi.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_mi.GetObsSendingRate());
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  event.AddValue("Raw RTT Inflation", raw_rtt_inflation);
  event.AddValue("Send Dur", send_dur);
  event.AddValue("Recv Dur", recv_dur);
  log->LogEvent(event); 
  
  return utility;
}
