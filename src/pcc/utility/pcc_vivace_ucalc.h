
#ifndef _PCC_VIVACE_UCALC_H_
#define _PCC_VIVACE_UCALC_H_

#include "pcc_ucalc.h"
#include<string>

class PccVivaceUtilityCalculator : public PccUtilityCalculator {
  public:
    PccVivaceUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccVivaceUtilityCalculator() {};
    float CalculateUtility(MonitorInterval& cur_mi);
  private:
    PccEventLogger* log;
};

#endif
