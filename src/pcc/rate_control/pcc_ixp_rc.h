// PCC (Performance Oriented Congestion Control) algorithm

#ifndef _PCC_IXP_RC_H_
#define _PCC_IXP_RC_H_

#include <vector>
#include <queue>

#include "../../core/options.h"
#include "pcc_rc.h"
#include <iostream>

struct RateSample {
    double rate;
    double u_mean;
    double u_var;
    double ud_mean;
    double ud_var;
    double ud_smallest;
    int n_samples;

    void AddSample(const MonitorInterval* first_mi, const MonitorInterval* second_mi);
    double GetDeltaCertainty() const;
    RateSample(double rate);
};

class PccIxpRateController : public PccRateController {
 public:

  PccIxpRateController(double call_freq, PccEventLogger* log);
  ~PccIxpRateController();

  QuicBandwidth GetNextSendingRate(QuicBandwidth current_rate, QuicTime cur_time);
  void MonitorIntervalFinished(const MonitorInterval& mi);
 private:
  double ComputeProjectedUtilityGradient();
  double ComputeProjectedUtility(const RateSample* rs);
  std::deque<RateSample*> rate_samples_;
  int cur_replica_;
  MonitorInterval* cur_mi_;
  RateSample* cur_rs_;
  PccEventLogger* log_;
  bool last_change_pos_;
  double step_size_;
};

#endif
