#ifndef THIRD_PARTY_PCC_QUIC_PCC_UTILITY_MANAGER_H_
#define THIRD_PARTY_PCC_QUIC_PCC_UTILITY_MANAGER_H_

#include "pcc_monitor_interval_queue.h"

#include <string>

//#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"

// #define PER_MI_DEBUG_

// namespace quic {

// IntervalStats stores the performance metrics for a monitor interval, which
// is used for utility calculation.
struct IntervalStats {
  IntervalStats() : min_rtt(-1), max_rtt(-1) {}
  ~IntervalStats() {}

  float interval_duration;
  float rtt_ratio;
  int64_t marked_lost_bytes;
  float loss_rate;
  float actual_sending_rate_mbps;
  float ack_rate_mbps;

  float avg_rtt;
  float rtt_dev;
  float min_rtt;
  float max_rtt;
  float approx_rtt_gradient;

  float rtt_gradient;
  float rtt_gradient_cut;
  float rtt_gradient_error;

  float trending_gradient;
  float trending_gradient_cut;
  float trending_gradient_error;

  float trending_deviation;
};

class PccUtilityManager {
 public:
  PccUtilityManager();

  // Utility calculation interface for all pcc senders.
  float CalculateUtility(const MonitorInterval* interval,
                         QuicTime::Delta event_time);

  // Get the utility function used by pcc sender.
  const std::string GetUtilityTag() const;
  // Get the effective utility tag.
  const std::string GetEffectiveUtilityTag() const;
  // Set the utility function used by pcc sender.
  void SetUtilityTag(std::string utility_tag);
  // Set the effective utility tag.
  void SetEffectiveUtilityTag(std::string utility_tag);
  // Set the parameter needed by utility function.
  void SetUtilityParameter(void* param);
  // Get the specified utility parameter.
  void* GetUtilityParameter(int parameter_index) const;

 private:
  // Prepare performance metrics for utility calculation.
  void PrepareStatistics(const MonitorInterval* interval);
  void PreProcessing(const MonitorInterval* interval);
  void ComputeSimpleMetrics(const MonitorInterval* interval);
  void ComputeApproxRttGradient(const MonitorInterval* interval);
  void ComputeRttGradient(const MonitorInterval* interval);
  void ComputeRttGradientError(const MonitorInterval*);
  void ComputeRttDeviation(const MonitorInterval* interval);

  void ProcessRttTrend(const MonitorInterval* interval);
  void ComputeTrendingGradient();
  void ComputeTrendingGradientError();
  void ComputeTrendingDeviation();

  void DetermineToleranceGeneral();
  void DetermineToleranceInflation();
  void DetermineToleranceDeviation();

  // Calculates utility for |interval|.
  float CalculateUtilityAllegro(const MonitorInterval* interval);
  float CalculateUtilityVivace(const MonitorInterval* interval);
  float CalculateUtilityHybridAllegro(const MonitorInterval* interval,
                                      float bound);
  float CalculateUtilityProportional(const MonitorInterval* interval,
                                  float latency_coefficient,
                                  float loss_coefficient);
  float CalculateUtilityScavenger(const MonitorInterval* interval,
                                   float rtt_variance_coefficient);
  float CalculateUtilityHybridVivace(const MonitorInterval* interval,
                                     float bound);
  float CalculateUtilityHybridVivace2(const MonitorInterval* interval,
                                      float bound);
  float CalculateUtilityRateLimiter(const MonitorInterval* interval,
                                    float rate_limiter_parameter);
  float CalculateUtilityLedbat(const MonitorInterval* interval,
                               float target_delay);
  float CalculateUtilityHybrid(const MonitorInterval* interval,
                               float rate_bound);
  float CalculateUtilityTEST(const MonitorInterval* interval,
                             float latency_coefficient,
                             float loss_coefficient);

  // Calculates perfect utility with respect to a specifc sending rate, i.e.,
  // assuming no packet loss and no RTT changes.
  float CalculatePerfectUtilityAllegro(float sending_rate_mbps);
  float CalculatePerfectUtilityVivace(float sending_rate_mbps);

  // String tag that represents the utility function.
  std::string utility_tag_;
  // May be different from actual utility tag when using Hybrid utility.
  std::string effective_utility_tag_;
  // Parameters needed by some utility functions, e.g., sending rate bound used
  // in hybrid utility functions.
  std::vector<void *> utility_parameters_;

  // Performance metrics for latest monitor interval.
  IntervalStats interval_stats_;

  size_t lost_bytes_tolerance_quota_;

  float avg_mi_rtt_dev_;
  float dev_mi_rtt_dev_;
  float min_rtt_;

  std::deque<float> mi_avg_rtt_history_;
  float avg_trending_gradient_;
  float min_trending_gradient_;
  float dev_trending_gradient_;
  float last_trending_gradient_;

  std::deque<float> mi_rtt_dev_history_;
  float avg_trending_dev_;
  float min_trending_dev_;
  float dev_trending_dev_;
  float last_trending_dev_;

  float ratio_inflated_mi_;
  float ratio_fluctuated_mi_;

  bool is_rtt_inflation_tolerable_;
  bool is_rtt_dev_tolerable_;
};

// }  // namespace quic

#endif
