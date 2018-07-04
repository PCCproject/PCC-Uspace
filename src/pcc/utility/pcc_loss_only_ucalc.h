
#ifndef _PCC_LOSS_ONLY_UCALC_H_
#define _PCC_LOSS_ONLY_UCALC_H_

#include "pcc_ucalc.h"

class PccLossOnlyUtilityCalculator : public PccUtilityCalculator {
  public:
    PccLossOnlyUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccLossOnlyUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#endif
