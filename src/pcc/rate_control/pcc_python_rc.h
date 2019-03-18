
#ifndef _PCC_PYTHON_RC_H_
#define _PCC_PYTHON_RC_H_

#include <mutex>
#include <queue>
#include <vector>

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
  
  static void InitializePython();
  static int GetNextId();
  static std::mutex interpreter_lock_;
  static bool python_initialized_;

  void GiveSample(long id, long bytes_sent, long bytes_acked, long bytes_lost,
                  double send_start_time, double send_end_time,
                  double recv_start_time, double recv_end_time,
                  std::vector<double> rtt_samples, long packet_size,
                  double utility);
  void GiveMiSample(const MonitorInterval& mi);

  int id;
  
  PyObject* module;
  PyObject* give_sample_func;
  PyObject* get_rate_func;
  PyObject* reset_func;

};

#endif
