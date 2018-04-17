
#ifndef PCC_IXP_UTILITY_CALCULATOR_H_
#define PCC_IXP_UTILITY_CALCULATOR_H_

#include "pcc_utility_calculator.h"

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccIxpUtilityCalculator : public PccUtilityCalculator {
  public:
    PccIxpUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccIxpUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
