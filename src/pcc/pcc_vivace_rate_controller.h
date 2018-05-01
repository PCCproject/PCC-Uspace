// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_VIVACE_RATE_CONTROLLER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_VIVACE_RATE_CONTROLLER_H_

#include <vector>
#include <queue>

#ifdef QUIC_PORT
#include "base/macros.h"
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_queue.h"

#include "net/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/quic/core/quic_bandwidth.h"
#include "net/quic/core/quic_connection_stats.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"

#include "gfe/quic/core/congestion_control/send_algorithm_interface.h"
#include "gfe/quic/core/quic_bandwidth.h"
#include "gfe/quic/core/quic_connection_stats.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include "../core/options.h"
#include "pcc_monitor_interval_queue.h"
#include "pcc_logger.h"
#include "pcc_python_helper.h"
#include "pcc_rate_controller.h"
#include <iostream>
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccVivaceRateController : public PccRateController {
 public:

  PccVivaceRateController();
  ~PccVivaceRateController() {};

  QuicBandwidth GetNextSendingRate(
        PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        QuicBandwidth current_rate,
        QuicTime cur_time);
 private:

  // The number of consecutive rate changes in a single direction
  // before we accelerate the rate of change.
  size_t swing_buffer_;
  // An acceleration factor for the rate of change.
  float rate_change_amplifier_;
  // The maximum rate change as a proportion of the current rate.
  size_t rate_change_proportion_allowance_;
  // The most recent change made to the sending rate.
  QuicBandwidth previous_change_;
  // Whether the protocol is current in the startup phase.
  bool in_startup_;

};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
