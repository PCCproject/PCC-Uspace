// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_PYTHON_HELPER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_PYTHON_HELPER_H_

#include <vector>
#include <map>
#include "../core/options.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <python2.7/Python.h>
#define QUIC_EXPORT_PRIVATE


class QUIC_EXPORT_PRIVATE PccPythonHelper {
 public:
  PccPythonHelper(const std::string& python_filename);
  ~PccPythonHelper();
  void GiveSample(double sending_rate, double latency, double loss_rate, double latency_inflation, double utility);
  double GetRate();
 private:
  PyObject* module;
  PyObject* give_sample_func;
  PyObject* get_rate_func;
};

#endif
