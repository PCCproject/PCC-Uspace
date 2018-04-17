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
const float kLossCoefficient = 20.0f;
// Coefficient of RTT term in utility function.
const float kRttCoefficient = 1.0/2500.0f;
}  // namespace

float PccIxpUtilityCalculator::CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        MonitorInterval& cur_monitor_interval) {

  float sending_rate_bps = cur_monitor_interval.GetObsSendingRate();
  float rtt_inflation = cur_monitor_interval.GetObsRttInflation(); 
  float avg_rtt = cur_monitor_interval.GetObsRtt();
  float loss_rate = cur_monitor_interval.GetObsLossRate();

  float loss_odds = loss_rate / (1.0 - loss_rate);
  float ixp_utility = sending_rate_bps / exp(kRttCoefficient * avg_rtt + kLossCoefficient * loss_odds);

  PccLoggableEvent event("Calculate Utility", "--log-utility-calc-lite");
  event.AddValue("Utility", ixp_utility);
  event.AddValue("Actual Rate", sending_rate_bps);
  event.AddValue("Loss Rate", loss_rate);
  event.AddValue("Avg RTT", avg_rtt);
  log->LogEvent(event); 
  
  return ixp_utility;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif
