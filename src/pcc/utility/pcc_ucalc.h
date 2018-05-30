
#ifndef _PCC_UCALC_H_
#define _PCC_UCALC_H_

#include "../monitor_interval/pcc_mi.h"
#include "../monitor_interval/pcc_mi_analysis_group.h"
#include "../pcc_logger.h"

class PccUtilityCalculator {
  public:
    PccUtilityCalculator(/*PccEventLogger* log*/) {};
    virtual ~PccUtilityCalculator() {};
    virtual float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval) = 0;
};

#endif
