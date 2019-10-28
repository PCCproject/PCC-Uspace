
#ifndef _PCC_RC_H_
#define _PCC_RC_H_

#include "../monitor_interval/pcc_mi.h"
#include "../../core/options.h"
#include "../pcc_logger.h"

class PccRateController {
  public:
    PccRateController() {};
    virtual ~PccRateController() {};
    virtual MonitorInterval GetNextMonitorInterval(QuicTime cur_time, QuicTime cur_rtt) = 0;
    virtual void MonitorIntervalFinished(const MonitorInterval& mi) = 0;
    virtual void Reset() {std::cout << "DEFAULT RESET CALLED" << std::endl; };
};

#endif
