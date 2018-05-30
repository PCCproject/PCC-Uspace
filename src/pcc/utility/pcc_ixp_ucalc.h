
#ifndef _PCC_IXP_UCALC_H_
#define _PCC_IXP_UCALC_H_

#include "pcc_ucalc.h"

class PccIxpUtilityCalculator : public PccUtilityCalculator {
  public:
    PccIxpUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccIxpUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#endif
