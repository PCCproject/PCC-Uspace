
#ifndef _PCC_IXP_UCALC_H_
#define _PCC_IXP_UCALC_H_

#include "pcc_ucalc.h"

class PccIxpUtilityCalculator : public PccUtilityCalculator {
  public:
    PccIxpUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccIxpUtilityCalculator() {};
    float CalculateUtility(MonitorInterval& cur_mi);
  private:
    PccEventLogger* log;
};

#endif
