// PCC (Performance Oriented Congestion Control) algorithm

#ifndef _PCC_VIVACE_RC_H_
#define _PCC_VIVACE_RC_H_

#include <vector>
#include <queue>

#include "../../core/options.h"
#include "pcc_rc.h"
#include <iostream>

enum VivaceRcState {STARTING, PROBING, MOVING};

struct RateSample {
    double rate;
    double utility;
};

class PccVivaceRateController : public PccRateController {
 public:

  PccVivaceRateController(PccEventLogger* log);
  ~PccVivaceRateController() {};

  QuicBandwidth GetNextSendingRate(QuicBandwidth current_rate, QuicTime cur_time, int id);
  void MonitorIntervalFinished(const MonitorInterval& mi);

  /* Do nothing for reset. */
  void Reset() {};

 private:

  /* STARTING state functions */
  void StartingMonitorIntervalFinished(const MonitorInterval& mi);
  QuicBandwidth GetNextStartingSendingRate(QuicBandwidth current_rate, QuicTime cur_time, int id);

  /* PROBING state functions */
  void TransitionToProbing();
  void ProbingMonitorIntervalFinished(const MonitorInterval& mi);
  QuicBandwidth GetNextProbingSendingRate(QuicBandwidth current_rate, QuicTime cur_time, int id);
  void ProbingFinished();

  /* Functions related to the rate probes */
  bool ProbingPairWasHigherBetter(int);
  double ProbingPairGetBetterRate(int);
  bool WasProbeConclusive();

  /* MOVING state functions */
  void TransitionToMoving();
  void MovingMonitorIntervalFinished(const MonitorInterval& mi);
  QuicBandwidth GetMovingStep();
  QuicBandwidth GetNextMovingSendingRate(QuicBandwidth current_rate, QuicTime cur_time, int id);

  double ComputeUtilityGradient(const RateSample& rs1, const RateSample& rs2);

  double target_rate_;
  double rate_change_bound_;
  double rate_change_amplifier_;
  double min_rate_change_for_gradient_;

  RateSample probing_rate_samples_ [4];

  RateSample last_rate_sample_;
  double last_gradient_;

  int probing_seed_;
  int first_probing_sample_id_;

  PccEventLogger* log_;
  bool last_change_pos_;

  VivaceRcState state_;
};

#endif
