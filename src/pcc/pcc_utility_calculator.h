
#ifndef PCC_UTILITY_CALCULATOR_H_
#define PCC_UTILITY_CALCULATOR_H_

#include "pcc_monitor_interval.h"
#include "pcc_monitor_interval_analysis_group.h"

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccUtilityCalculator {
  public:
    PccUtilityCalculator() {};
    virtual ~PccUtilityCalculator() {};
    virtual float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval) = 0;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
