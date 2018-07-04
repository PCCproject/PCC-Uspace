
#ifndef _PCC_COPA_UCALC_H_
#define _PCC_COPA_UCALC_H_

#include "pcc_ucalc.h"

class PccCopaUtilityCalculator : public PccUtilityCalculator {
  public:
    PccCopaUtilityCalculator(PccEventLogger* logger) {this->logger = logger;};
    ~PccCopaUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* logger;
};

#endif
