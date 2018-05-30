
#ifndef _PCC_VIVACE_UCALC_H_
#define _PCC_VIVACE_UCALC_H_

#include "pcc_ucalc.h"

class PccVivaceUtilityCalculator : public PccUtilityCalculator {
  public:
    PccVivaceUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccVivaceUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#endif
