
#ifndef _PCC_UCALC_H_
#define _PCC_UCALC_H_

#include "../monitor_interval/pcc_mi.h"
#include "../pcc_logger.h"

class PccUtilityCalculator {
  public:
    PccUtilityCalculator(/*PccEventLogger* log*/) {};
    virtual ~PccUtilityCalculator() {};
    virtual float CalculateUtility(MonitorInterval& cur_monitor_interval) = 0;
};

#endif
