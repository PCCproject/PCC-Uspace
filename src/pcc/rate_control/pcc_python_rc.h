
#ifndef _PCC_PYTHON_RC_H_
#define _PCC_PYTHON_RC_H_

#include <vector>
#include <queue>

#include "pcc_rc.h"
#include <python3.5/Python.h>
#include <iostream>
#include <sstream>

class PccPythonRateController : public PccRateController {
 public:

  PccPythonRateController(double call_freq, PccEventLogger* log);
  ~PccPythonRateController() {};

  QuicBandwidth GetNextSendingRate(QuicBandwidth current_rate, QuicTime cur_time);
  void MonitorIntervalFinished(const MonitorInterval& mi);

  void Reset();
 private:
  
  void GiveSample(double rate, double recv_rate, double lat, double loss, double lat_infl, double utility);
  void GiveMiSample(const MonitorInterval& mi);
  
  PyObject* module;
  PyObject* give_sample_func;
  PyObject* get_rate_func;
  PyObject* reset_func;

  int last_given_mi_id;
  std::vector<MonitorInterval> mi_cache;
};

#endif
