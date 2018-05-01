
#ifndef PCC_RATE_CONTROLLER_H_
#define PCC_RATE_CONTROLLER_H_

#include "pcc_monitor_interval_analysis_group.h"

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccRateController {
  public:
    PccRateController() {};
    virtual ~PccRateController() {};
    virtual QuicBandwidth GetNextSendingRate(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, QuicBandwidth current_rate,
            QuicTime cur_time) = 0;
    virtual void Reset() {std::cout << "DEFAULT RESET CALLED" << std::endl; };
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
