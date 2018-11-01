#include "pcc_rc_factory.h"

PccRateController* PccRateControllerFactory::Create(const std::string& name, PccEventLogger* log) {
    
    if (name == "vivace") {
        return new PccVivaceRateController(log);
    }
    return new PccVivaceRateController(log);
}
