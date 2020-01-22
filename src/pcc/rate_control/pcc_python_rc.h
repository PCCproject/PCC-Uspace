
#ifndef _PCC_PYTHON_RC_H_
#define _PCC_PYTHON_RC_H_

#include <mutex>
#include <queue>
#include <vector>

#include "pcc_rc.h"
#include <python3.6/Python.h>
#include <iostream>
#include <sstream>

#ifndef USEC_PER_SEC
    #define USEC_PER_SEC 1000000
#endif

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

  void GiveSample(int bytes_sent,
                  int bytes_acked,
                  int bytes_lost,
                  double send_start_time_sec,
                  double send_end_time_sec,
                  double recv_start_time_sec,
                  double recv_end_time_sec,
                  double first_ack_latency_sec,
                  double last_ack_latency_sec,
                  int packet_size,
                  double utility);

  int id;
  bool has_time_offset;
  uint64_t time_offset_usec;
  
  PyObject* module;
  PyObject* give_sample_func;
  PyObject* get_rate_func;
  PyObject* reset_func;

};

#endif
