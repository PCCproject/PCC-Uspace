
#ifndef _PCC_COPA_UCALC_H_
#define _PCC_COPA_UCALC_H_

#include "pcc_ucalc.h"

class PccCopaUtilityCalculator : public PccUtilityCalculator {
  public:
    PccCopaUtilityCalculator(PccEventLogger* logger) {this->logger = logger; this->last_avg_rtt = 1000; };
    ~PccCopaUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* logger;
    float last_avg_rtt;
};

#endif
