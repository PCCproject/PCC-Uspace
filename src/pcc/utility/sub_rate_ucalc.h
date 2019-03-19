
#ifndef _PCC_SUB_RATE_UCALC_H_
#define _PCC_SUB_RATE_UCALC_H_

#include "pcc_ucalc.h"

class PccSubRateUtilityCalculator : public PccUtilityCalculator {
  public:
    PccSubRateUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccSubRateUtilityCalculator() {};
    float CalculateUtility(MonitorInterval& cur_mi);
  private:
    PccEventLogger* log;
};

#endif
