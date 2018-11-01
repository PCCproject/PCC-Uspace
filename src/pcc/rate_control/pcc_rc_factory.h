
#ifndef PCC_RC_FACTORY_H_
#define PCC_RC_FACTORY_H_

#include "pcc_all_rc.h"

class PccRateControllerFactory {
  public:
    static PccRateController* Create(const std::string& name, PccEventLogger* log);
};

#endif
