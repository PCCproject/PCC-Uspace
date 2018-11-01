
#ifndef _PCC_LIN_UCALC_H_
#define _PCC_LIN_UCALC_H_

#include "pcc_ucalc.h"

class PccLinearUtilityCalculator : public PccUtilityCalculator {
  public:
    PccLinearUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccLinearUtilityCalculator() {};
    float CalculateUtility(MonitorInterval& cur_mi);
  private:
    PccEventLogger* log;
};

#endif
