
#ifndef _PCC_LIN_UCALC_H_
#define _PCC_LIN_UCALC_H_

#include "pcc_ucalc.h"

class PccLinearUtilityCalculator : public PccUtilityCalculator {
  public:
    PccLinearUtilityCalculator(PccEventLogger* log) {this->log = log; last_rtt = 0;};
    ~PccLinearUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
    float last_rtt;
};

#endif
