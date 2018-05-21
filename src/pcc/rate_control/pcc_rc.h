
#ifndef _PCC_RC_H_
#define _PCC_RC_H_

#include "../../core/options.h"
#include "../monitor_interval/metrics/pcc_mi_metric.h"
#include "../pcc_logger.h"

class PccRateController {
  public:
    PccRateController() {};
    virtual ~PccRateController() {};
    virtual QuicBandwidth GetNextSendingRate(QuicBandwidth current_rate, QuicTime cur_time) = 0;
    virtual void MonitorIntervalFinished(const MonitorInterval& mi) = 0;
    virtual void Reset() {std::cout << "DEFAULT RESET CALLED" << std::endl; };
};

#endif
