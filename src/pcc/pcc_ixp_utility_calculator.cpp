#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_utility_calculator.h"
#else
#include "third_party/pcc_quic/pcc_utility_calculator.h"
#endif
#else
#include "pcc_ixp_utility_calculator.h"
#endif

#include <cmath>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

namespace {
// Coefficeint of the loss rate term in utility function.
const float kLossCoefficient = 5.0f;
// Coefficient of RTT term in utility function.
const float kRttCoefficient = 1.0/30000.0f;
}  // namespace

float PccIxpUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_monitor_interval) {

  //std::cout << "Getting observations" << std::endl;
  float throughput = cur_monitor_interval.GetObsThroughput();
  float rtt_inflation = cur_monitor_interval.GetObsRttInflation(); 
  float avg_rtt = cur_monitor_interval.GetObsRtt();
  float loss_rate = cur_monitor_interval.GetObsLossRate();
  //std::cout << "Got observations" << std::endl;

  float loss_odds = loss_rate / (1.0 - loss_rate);
  float ixp_utility = throughput / exp(kRttCoefficient * avg_rtt + kLossCoefficient * loss_odds);

  //std::cout << "r = " << cur_monitor_interval.GetTargetSendingRate() << ", u = " << ixp_utility << std::endl;

  float bandwidth_diff_utility = 2.0 * throughput - cur_monitor_interval.GetTargetSendingRate();
  
  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", ixp_utility);
  //event.AddValue("Utility", bandwidth_diff_utility);
  event.AddValue("MI Start Time", cur_monitor_interval.GetStartTime());
  event.AddValue("Target Rate", cur_monitor_interval.GetTargetSendingRate());
  event.AddValue("Actual Rate", cur_monitor_interval.GetObsSendingRate());
  event.AddValue("Throughput", throughput);
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  log->LogEvent(event); 
  

  //return bandwidth_diff_utility;
  return ixp_utility;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
