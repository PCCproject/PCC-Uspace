// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_PYTHON_RATE_CONTROLLER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_PYTHON_RATE_CONTROLLER_H_

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
#include "pcc_monitor_interval.h"
#include "pcc_monitor_interval_queue.h"
#include "pcc_logger.h"
#include "pcc_rate_controller.h"
#include <python3.5/Python.h>
#include <iostream>
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
#endif
#endif

class PccPythonRateController : public PccRateController {
 public:

  PccPythonRateController();
  ~PccPythonRateController() {};

  QuicBandwidth GetNextSendingRate(
        PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        QuicBandwidth current_rate,
        QuicTime cur_time);

  void Reset();
 private:
  
  void GiveSample(double rate, double recv_rate, double lat, double loss, double lat_infl, double utility);
  void GiveMiSample(MonitorInterval& mi);
  
  PyObject* module;
  PyObject* give_sample_func;
  PyObject* get_rate_func;
  PyObject* reset_func;

  int last_given_mi_id;

};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
