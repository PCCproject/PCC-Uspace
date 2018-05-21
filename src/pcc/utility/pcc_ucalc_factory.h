
#ifndef _PCC_UCALC_FACTORY_H_
#define _PCC_UCALC_FACTORY_H_

#include "pcc_all_ucalc.h"
#include "../pcc_logger.h"

class PccUtilityCalculatorFactory {
  public:
    static PccUtilityCalculator* Create(const std::string& name, PccEventLogger* log);
};

#endif
