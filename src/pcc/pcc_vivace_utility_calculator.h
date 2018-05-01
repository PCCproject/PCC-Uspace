
#ifndef PCC_VIVACE_UTILITY_CALCULATOR_H_
#define PCC_VIVACE_UTILITY_CALCULATOR_H_

#include "pcc_utility_calculator.h"

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccVivaceUtilityCalculator : public PccUtilityCalculator {
  public:
    PccVivaceUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccVivaceUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
