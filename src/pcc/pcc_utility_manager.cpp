#include "pcc_utility_manager.h"
#include "math.h"
#include <assert.h>
#include <iostream>

//#include "third_party/pcc_quic/pcc_utility_manager.h"

//#include "third_party/quic/core/congestion_control/rtt_stats.h"

// namespace quic {

namespace {
const QuicByteCount kMaxPacketSize = 1500;
// Number of bits per byte.
const size_t kBitsPerByte = 8;
const size_t kRttHistoryLen = 6;

// Tolerance of loss rate by Allegro utility function.
const float kLossTolerance = 0.05f;
// Coefficeint of the loss rate term in Allegro utility function.
const float kLossCoefficient = -1000.0f;
// Coefficient of RTT term in Allegro utility function.
const float kRTTCoefficient = -200.0f;

// Exponent of sending rate contribution term in Vivace utility function.
const float kSendingRateExponent = 0.9f;
// Coefficient of loss penalty term in Vivace utility function.
const float kVivaceLossCoefficient = 11.35f;
// Coefficient of latency penalty term in Vivace utility function.
const float kLatencyCoefficient = 900.0f;

// Coefficient of rtt deviation term in Scavenger utility function.
const float kRttDeviationCoefficient = 0.0015f;

// The factor for sending rate transform in hybrid utility function.
const float kHybridUtilityRateTransformFactor = 0.1f;

// The update rate for moving average variable.
const float kAlpha = 0.1f;
// The order of magnitude that distinguishes abnormal sample.
const float kBeta = 100.0f;

// The threshold for ratio of monitor interval count, above which moving average
// of trending RTT metrics (gradient and deviation) would be reset.
const float kTrendingResetIntervalRatio = 0.95f;

// Number of deviation above/below average trending gradient used for RTT
//inflation tolerance for primary and scavenger senders.
const float kInflationToleranceGainHigh = 2.0f;
const float kInflationToleranceGainLow = 2.0f;

const size_t kLostPacketTolerance = 10;
}  // namespace

PccUtilityManager::PccUtilityManager()
    : utility_tag_("Allegro"),
      effective_utility_tag_("Allegro"),
      lost_bytes_tolerance_quota_(kMaxPacketSize * kLostPacketTolerance),
      avg_mi_rtt_dev_(-1.0),
      dev_mi_rtt_dev_(-1.0),
      min_rtt_(-1.0),
      avg_trending_gradient_(-1.0),
      min_trending_gradient_(-1.0),
      dev_trending_gradient_(-1.0),
      last_trending_gradient_(-1.0),
      avg_trending_dev_(-1.0),
      min_trending_dev_(-1.0),
      dev_trending_dev_(-1.0),
      last_trending_dev_(-1.0),
      ratio_inflated_mi_(0),
      ratio_fluctuated_mi_(0),
      is_rtt_inflation_tolerable_(true),
      is_rtt_dev_tolerable_(true) {}

const std::string PccUtilityManager::GetUtilityTag() const {
  return utility_tag_;
}

const std::string PccUtilityManager::GetEffectiveUtilityTag() const {
  return effective_utility_tag_;
}

void PccUtilityManager::SetUtilityTag(std::string utility_tag) {
  utility_tag_ = utility_tag;
  effective_utility_tag_ = utility_tag;
  std::cerr << "Using Utility Function: " << utility_tag_ << std::endl;
}

void PccUtilityManager::SetEffectiveUtilityTag(std::string utility_tag) {
  effective_utility_tag_ = utility_tag;
}

void* PccUtilityManager::GetUtilityParameter(int parameter_index) const {
  return utility_parameters_.size() > parameter_index
      ? utility_parameters_[parameter_index]
      : (new float(0.0f));
}

void PccUtilityManager::SetUtilityParameter(void* param) {
  if (utility_tag_ == "HybridAllegro" || utility_tag_ == "HybridVivace" ||
      utility_tag_ == "Proportional" || utility_tag_ == "Scavenger" ||
      utility_tag_ == "RateLimiter" || utility_tag_ == "TEST" ||
      utility_tag_ == "Hybrid") {
    utility_parameters_.push_back(new float(*(float *)param));
    std::cerr << "Update Utility Parameter: " << (*(float *)param) << std::endl;
  }
}

void PccUtilityManager::PrepareStatistics(const MonitorInterval* interval) {
  PreProcessing(interval);

  ComputeSimpleMetrics(interval);
  ComputeApproxRttGradient(interval);
  ComputeRttGradient(interval);
  ComputeRttDeviation(interval);
  ComputeRttGradientError(interval);

  DetermineToleranceGeneral();
  ProcessRttTrend(interval);
}

float PccUtilityManager::CalculateUtility(const MonitorInterval* interval,
                                          QuicTime::Delta event_time) {
  // The caller should guarantee utility of this interval is available.
  assert(interval->first_packet_sent_time !=
              interval->last_packet_sent_time);

  PrepareStatistics(interval);

  float utility = 0.0;
  if (utility_tag_ == "Allegro") {
    utility = CalculateUtilityAllegro(interval);
  } else if (utility_tag_ == "Vivace") {
    utility = CalculateUtilityVivace(interval);
  } else if (utility_tag_ == "Proportional") {
    float latency_coefficient = *(float *)(utility_parameters_[0]);
    float loss_coefficient = *(float *)(utility_parameters_[1]);
    utility = CalculateUtilityProportional(interval, latency_coefficient,
                                     loss_coefficient);
  } else if (utility_tag_ == "Scavenger") {
    float rtt_deviation_coefficient = *(float *)(utility_parameters_[0]);
    utility = CalculateUtilityScavenger(interval, rtt_deviation_coefficient);
  } else if (utility_tag_ == "HybridAllegro") {
    float bound = *(float *)(utility_parameters_[0]);
    utility = CalculateUtilityHybridAllegro(interval, bound);
  } else if (utility_tag_ == "HybridVivace") {
    float bound = *(float *)utility_parameters_[0];
    utility = CalculateUtilityHybridVivace(interval, bound);
  } else if (utility_tag_ == "RateLimiter") {
    float bound = *(float *)utility_parameters_[0];
    float rate_limiter_parameter = 0.9 / pow(bound, 0.1);
    utility = CalculateUtilityRateLimiter(interval, rate_limiter_parameter);
  } else if (utility_tag_ == "Hybrid") {
    float rate_bound = *(float *)utility_parameters_[0];
    utility = CalculateUtilityHybrid(interval, rate_bound);
  } else if (utility_tag_ == "TEST") {
    float latency_coefficient = *(float *)(utility_parameters_[0]);
    float loss_coefficient = *(float *)(utility_parameters_[1]);
    utility = CalculateUtilityTEST(interval, latency_coefficient,
                                   loss_coefficient);
  } else {
    std::cerr << "Unrecognized utility tag, use Allegro instead" << std::endl;
    utility = CalculateUtilityAllegro(interval);
  }
#ifdef PER_MI_DEBUG_
  std::cerr << event_time.ToMicroseconds() / 1000000.0 << " "
            << interval->sending_rate.ToKBitsPerSecond() << " "
            << interval_stats_.actual_sending_rate_mbps << " "
            << interval_stats_.avg_rtt << " "
            << interval_stats_.min_rtt << " "
            << interval_stats_.max_rtt << " "
            << interval_stats_.max_rtt - interval_stats_.min_rtt << " "
            << interval->rtt_on_monitor_end.ToMicroseconds() << " "
            << interval_stats_.rtt_dev << " "
            << avg_mi_rtt_dev_ << " "
            << interval_stats_.rtt_dev / interval_stats_.avg_rtt << " "
            << interval_stats_.approx_rtt_gradient << " "
            << interval_stats_.loss_rate << " "
            << utility << " "
            << std::abs(interval_stats_.rtt_gradient) << " "
            << interval_stats_.rtt_gradient_error << " "
            << std::abs(interval_stats_.rtt_gradient) -
                   interval_stats_.rtt_gradient_error << " "
            << interval_stats_.trending_gradient << " "
            << avg_trending_gradient_ << " "
            << dev_trending_gradient_ << " "
            << avg_trending_gradient_ - 2 * dev_trending_gradient_ << " "
            << avg_trending_gradient_ + 2 * dev_trending_gradient_;
  if (utility_tag_ == "Scavenger") {
    std::cerr << " " << CalculateUtilityVivace(interval);
  }
  std::cerr << std::endl;
#endif
  return utility;
}

float PccUtilityManager::CalculateUtilityAllegro(
    const MonitorInterval* interval) {
  if (interval_stats_.rtt_ratio >
          1.0 - interval->rtt_fluctuation_tolerance_ratio &&
      interval_stats_.rtt_ratio <
          1.0 + interval->rtt_fluctuation_tolerance_ratio) {
    interval_stats_.rtt_ratio = 1.0;
  }
  float latency_penalty = 1.0 -
      1.0 / (1.0 + exp(kRTTCoefficient * (1.0 - interval_stats_.rtt_ratio)));

  float loss_penalty =
      1.0 - 1.0 / (1.0 + exp(kLossCoefficient *
                             (interval_stats_.loss_rate - kLossTolerance)));

  float bytes_acked = static_cast<float>(interval->bytes_acked);
  float bytes_lost = static_cast<float>(interval->bytes_lost);
  return (bytes_acked / interval_stats_.interval_duration * loss_penalty *
              latency_penalty -
          bytes_lost / interval_stats_.interval_duration) * 1000.0;
}

float PccUtilityManager::CalculateUtilityVivace(
    const MonitorInterval* interval) {
  return CalculateUtilityProportional(interval, kLatencyCoefficient,
                                   kVivaceLossCoefficient);
}

float PccUtilityManager::CalculateUtilityProportional(
    const MonitorInterval* interval,
    float latency_coefficient,
    float loss_coefficient) {
  float sending_rate_contribution =
      pow(interval_stats_.actual_sending_rate_mbps, kSendingRateExponent);

  float rtt_gradient =
      is_rtt_inflation_tolerable_ ? 0.0 : interval_stats_.rtt_gradient;
  if (interval->rtt_fluctuation_tolerance_ratio > 50.0 &&
      std::abs(rtt_gradient) < 1000.0 / interval_stats_.interval_duration) {
    rtt_gradient = 0.0;
  }
  if (rtt_gradient < 0) {
    rtt_gradient = 0.0;
  }
  /*rtt_gradient =
      static_cast<float>(static_cast<int>(rtt_gradient * 100.0)) / 100.0;*/
  /*if ((rtt_gradient > -1.0 * interval->rtt_fluctuation_tolerance_ratio &&
       rtt_gradient < interval->rtt_fluctuation_tolerance_ratio) ||
      interval_stats_.actual_sending_rate_mbps < 2) {
    rtt_gradient =
        // static_cast<float>(static_cast<int>(rtt_gradient * 100.0)) / 100.0;
  }*/
  float latency_penalty = latency_coefficient * rtt_gradient *
                          interval_stats_.actual_sending_rate_mbps;

  float loss_penalty = loss_coefficient * interval_stats_.loss_rate *
                       interval_stats_.actual_sending_rate_mbps;

  return sending_rate_contribution - latency_penalty - loss_penalty;
}

float PccUtilityManager::CalculateUtilityScavenger(
    const MonitorInterval* interval,
    float rtt_variance_coefficient) {
  float sending_rate_contribution =
      pow(interval_stats_.actual_sending_rate_mbps, kSendingRateExponent);
  float loss_penalty = kVivaceLossCoefficient * interval_stats_.loss_rate *
                       interval_stats_.actual_sending_rate_mbps;

  float rtt_gradient =
      is_rtt_inflation_tolerable_ ? 0.0 : interval_stats_.rtt_gradient;
  if (interval->rtt_fluctuation_tolerance_ratio > 50.0 &&
      std::abs(rtt_gradient) < 1000.0 / interval_stats_.interval_duration) {
    rtt_gradient = 0.0;
  }
  if (rtt_gradient < 0) {
    rtt_gradient = 0.0;
  }
  float latency_penalty = kLatencyCoefficient * rtt_gradient *
                          interval_stats_.actual_sending_rate_mbps;

  float rtt_dev = is_rtt_dev_tolerable_ ? 0.0 : interval_stats_.rtt_dev;
  if (interval->rtt_fluctuation_tolerance_ratio > 50.0) {
    rtt_dev = 0.0;
  }
  float rtt_dev_penalty = rtt_variance_coefficient * rtt_dev *
                          interval_stats_.actual_sending_rate_mbps;

  return sending_rate_contribution - loss_penalty -
      latency_penalty - rtt_dev_penalty;
}

float PccUtilityManager::CalculateUtilityHybridAllegro(
    const MonitorInterval* interval,
    float bound) {
  float allegro_utility = CalculateUtilityAllegro(interval);

  if (interval_stats_.actual_sending_rate_mbps < bound) {
    return allegro_utility;
  } else {
    float perfect_utility = CalculatePerfectUtilityAllegro(
        interval_stats_.actual_sending_rate_mbps);

    float bounded_sending_rate_mbps = bound +
        (interval_stats_.actual_sending_rate_mbps - bound) *
            kHybridUtilityRateTransformFactor;
    float bounded_perfect_utility =
        CalculatePerfectUtilityAllegro(bounded_sending_rate_mbps);

    return bounded_perfect_utility * (allegro_utility / perfect_utility);
  }
}

float PccUtilityManager::CalculateUtilityHybridVivace(
    const MonitorInterval* interval,
    float bound) {
  float vivace_utility = CalculateUtilityVivace(interval);

  if (interval_stats_.actual_sending_rate_mbps < bound) {
    return vivace_utility;
  } else {
    float perfect_utility = CalculatePerfectUtilityVivace(
        interval_stats_.actual_sending_rate_mbps);

    float bounded_sending_rate_mbps = bound +
        (interval_stats_.actual_sending_rate_mbps - bound) *
            kHybridUtilityRateTransformFactor;
    float bounded_perfect_utility =
        CalculatePerfectUtilityVivace(bounded_sending_rate_mbps);

    return bounded_perfect_utility + vivace_utility - perfect_utility;
  }
}

float PccUtilityManager::CalculateUtilityHybridVivace2(
    const MonitorInterval* interval,
    float bound) {
  float vivace_utility = CalculateUtilityVivace(interval);

  if (interval_stats_.actual_sending_rate_mbps < bound) {
    return vivace_utility;
  } else {
    float perfect_utility = CalculatePerfectUtilityVivace(
        interval_stats_.actual_sending_rate_mbps);

    float bounded_sending_rate_mbps = bound +
        (interval_stats_.actual_sending_rate_mbps - bound) *
            kHybridUtilityRateTransformFactor;
    float bounded_perfect_utility =
        CalculatePerfectUtilityVivace(bounded_sending_rate_mbps);

    return bounded_perfect_utility * (vivace_utility / perfect_utility);
  }
}

float PccUtilityManager::CalculateUtilityRateLimiter(
    const MonitorInterval* interval,
    float rate_limiter_parameter) {
  float vivace_utility = CalculateUtilityVivace(interval);

  float rate_penalty =
      rate_limiter_parameter * interval_stats_.actual_sending_rate_mbps;
  return vivace_utility - rate_penalty;
}

float PccUtilityManager::CalculateUtilityHybrid(
    const MonitorInterval* interval,
    float rate_bound) {
  if (interval_stats_.actual_sending_rate_mbps < rate_bound) {
    return CalculateUtilityVivace(interval);
  } else {
    return CalculateUtilityScavenger(interval, kRttDeviationCoefficient);
  }
}

float PccUtilityManager::CalculateUtilityTEST(
    const MonitorInterval* interval,
    float latency_coefficient,
    float loss_coefficient) {
  return CalculateUtilityVivace(interval);
}

float PccUtilityManager::CalculatePerfectUtilityAllegro(
    float sending_rate_mbps) {
  float rtt_ratio = 1.0;
  float latency_penalty =
      1.0 - 1.0 / (1.0 + exp(kRTTCoefficient * (1.0 - rtt_ratio)));

  float loss_rate = 0.0f;
  float loss_penalty =
      1.0 - 1.0 / (1.0 + exp(kLossCoefficient * (loss_rate - kLossTolerance)));

  float sending_rate_bype_per_usec =
      sending_rate_mbps / static_cast<float>(kBitsPerByte);

  return (sending_rate_bype_per_usec * loss_penalty * latency_penalty) * 1000.0;
}

float PccUtilityManager::CalculatePerfectUtilityVivace(
    float sending_rate_mbps) {
  return pow(sending_rate_mbps, kSendingRateExponent);
}

void PccUtilityManager::PreProcessing(const MonitorInterval* interval) {
  interval_stats_.marked_lost_bytes = 0;
  /*size_t num_lost_samples = interval->lost_packet_samples.size();
  for (size_t i = 0; i < num_lost_samples; ++i) {
    if ((i == 0 || interval->lost_packet_samples[i - 1].packet_number <
                       interval->lost_packet_samples[i].packet_number - 1) &&
        (i == num_lost_samples - 1 ||
         interval->lost_packet_samples[i + 1].packet_number >
             interval->lost_packet_samples[i].packet_number + 1)) {
      interval_stats_.marked_lost_bytes +=
          interval->lost_packet_samples[i].bytes;
    }
  }

  if (lost_bytes_tolerance_quota_ > 0) {
    int64_t tolerated_lost_bytes =
        std::min(lost_bytes_tolerance_quota_,
                 interval->bytes_lost - interval_stats_.marked_lost_bytes);
    lost_bytes_tolerance_quota_ -= tolerated_lost_bytes;
    interval_stats_.marked_lost_bytes += tolerated_lost_bytes;
  }*/
}

void PccUtilityManager::ComputeSimpleMetrics(const MonitorInterval* interval) {
  // Add the transfer time of the last packet in the monitor interval when
  // calculating monitor interval duration.
  interval_stats_.interval_duration = static_cast<float>(
      (interval->last_packet_sent_time - interval->first_packet_sent_time +
       interval->sending_rate.TransferTime(kMaxPacketSize)).ToMicroseconds());

  interval_stats_.rtt_ratio =
      static_cast<float>(interval->rtt_on_monitor_start.ToMicroseconds()) /
      static_cast<float>(interval->rtt_on_monitor_end.ToMicroseconds());
  interval_stats_.loss_rate =
      static_cast<float>(interval->bytes_lost -
                         interval_stats_.marked_lost_bytes) /
      static_cast<float>(interval->bytes_sent);
  interval_stats_.actual_sending_rate_mbps =
      static_cast<float>(interval->bytes_sent) *
      static_cast<float>(kBitsPerByte) / interval_stats_.interval_duration;

  size_t num_rtt_samples = interval->packet_rtt_samples.size();
  if (num_rtt_samples > 1) {
    float ack_duration = static_cast<float>(
        (interval->packet_rtt_samples[num_rtt_samples - 1].ack_timestamp -
         interval->packet_rtt_samples[0].ack_timestamp).ToMicroseconds());
    interval_stats_.ack_rate_mbps =
        static_cast<float>(interval->bytes_acked - kMaxPacketSize) *
        static_cast<float>(kBitsPerByte) / ack_duration;
  } else if (num_rtt_samples == 1) {
    interval_stats_.ack_rate_mbps =
        static_cast<float>(interval->bytes_acked) /
        interval_stats_.interval_duration;
  } else {
    interval_stats_.ack_rate_mbps = 0;
  }
}

void PccUtilityManager::ComputeApproxRttGradient(
    const MonitorInterval* interval) {
  // Separate all RTT samples in the interval into two halves, and calculate an
  // approximate RTT gradient.
  QuicTime::Delta rtt_first_half = QuicTime::Delta::Zero();
  QuicTime::Delta rtt_second_half = QuicTime::Delta::Zero();
  size_t num_half_samples = interval->packet_rtt_samples.size() / 2;
  size_t num_first_half_samples = 0;
  size_t num_second_half_samples = 0;
  for (size_t i = 0; i < num_half_samples; ++i) {
    if (interval->packet_rtt_samples[i].is_reliable_for_gradient_calculation) {
      rtt_first_half = rtt_first_half +
          interval->packet_rtt_samples[i].sample_rtt;
      num_first_half_samples++;
    }
    if (interval->packet_rtt_samples[i + num_half_samples]
                 .is_reliable_for_gradient_calculation) {
      rtt_second_half = rtt_second_half +
          interval->packet_rtt_samples[i + num_half_samples].sample_rtt;
      num_second_half_samples++;
    }
  }

  if (num_first_half_samples == 0 || num_second_half_samples == 0) {
    interval_stats_.approx_rtt_gradient = 0.0;
    return;
  }
  rtt_first_half =
      rtt_first_half * (1.0 / static_cast<float>(num_first_half_samples));
  rtt_second_half =
      rtt_second_half * (1.0 / static_cast<float>(num_second_half_samples));
  interval_stats_.approx_rtt_gradient = 2.0 *
      static_cast<float>((rtt_second_half - rtt_first_half).ToMicroseconds()) /
      static_cast<float>((rtt_second_half + rtt_first_half).ToMicroseconds());
}

void PccUtilityManager::ComputeRttGradient(const MonitorInterval* interval) {
  if (interval->num_reliable_rtt_for_gradient_calculation < 2) {
    interval_stats_.rtt_gradient = 0;
    interval_stats_.rtt_gradient_cut = 0;
    return;
  }

  // Calculate RTT gradient using linear regression.
  float gradient_x_avg = 0.0;
  float gradient_y_avg = 0.0;
  float gradient_x = 0.0;
  float gradient_y = 0.0;
  for (const PacketRttSample& rtt_sample : interval->packet_rtt_samples) {
    if (!rtt_sample.is_reliable_for_gradient_calculation) {
      continue;
    }
    gradient_x_avg += static_cast<float>(rtt_sample.packet_number);
    gradient_y_avg +=
        static_cast<float>(rtt_sample.sample_rtt.ToMicroseconds());
  }
  gradient_x_avg /=
      static_cast<float>(interval->num_reliable_rtt_for_gradient_calculation);
  gradient_y_avg /=
      static_cast<float>(interval->num_reliable_rtt_for_gradient_calculation);
  for (const PacketRttSample& rtt_sample : interval->packet_rtt_samples) {
    if (!rtt_sample.is_reliable_for_gradient_calculation) {
      continue;
    }
    float delta_packet_number =
        static_cast<float>(rtt_sample.packet_number) - gradient_x_avg;
    float delta_rtt_sample =
        static_cast<float>(rtt_sample.sample_rtt.ToMicroseconds()) -
        gradient_y_avg;
    gradient_x += delta_packet_number * delta_packet_number;
    gradient_y += delta_packet_number * delta_rtt_sample;
  }
  interval_stats_.rtt_gradient = gradient_y / gradient_x;
  interval_stats_.rtt_gradient /=
      static_cast<float>(interval->sending_rate.TransferTime(kMaxPacketSize)
                                               .ToMicroseconds());
  interval_stats_.avg_rtt = gradient_y_avg;
  interval_stats_.rtt_gradient_cut =
      gradient_y_avg - interval_stats_.rtt_gradient * gradient_x_avg;
}

void PccUtilityManager::ComputeRttGradientError(
    const MonitorInterval* interval) {
  interval_stats_.rtt_gradient_error = 0.0;
  if (interval->num_reliable_rtt_for_gradient_calculation < 2) {
    return;
  }

  for (const PacketRttSample& rtt_sample : interval->packet_rtt_samples) {
    if (!rtt_sample.is_reliable_for_gradient_calculation) {
      continue;
    }
    float regression_rtt = static_cast<float>(rtt_sample.packet_number *
                                              interval_stats_.rtt_gradient) +
                           interval_stats_.rtt_gradient_cut;
    interval_stats_.rtt_gradient_error +=
        pow(rtt_sample.sample_rtt.ToMicroseconds() - regression_rtt, 2.0);
  }
  interval_stats_.rtt_gradient_error /=
      static_cast<float>(interval->num_reliable_rtt_for_gradient_calculation);
  interval_stats_.rtt_gradient_error = sqrt(interval_stats_.rtt_gradient_error);
  interval_stats_.rtt_gradient_error /=
      static_cast<float>(interval_stats_.avg_rtt);
}

void PccUtilityManager::ComputeRttDeviation(const MonitorInterval* interval) {
  if (interval->num_reliable_rtt < 2) {
    interval_stats_.rtt_dev = 0;
    return;
  }

  // Calculate RTT deviation.
  interval_stats_.rtt_dev = 0.0;
  interval_stats_.max_rtt = -1;
  interval_stats_.min_rtt = -1;
  for (const PacketRttSample& rtt_sample : interval->packet_rtt_samples) {
    if (!rtt_sample.is_reliable) {
      continue;
    }
    float delta_rtt_sample =
        static_cast<float>(rtt_sample.sample_rtt.ToMicroseconds()) -
        interval_stats_.avg_rtt;
    interval_stats_.rtt_dev += delta_rtt_sample * delta_rtt_sample;

    if (min_rtt_ < 0 || rtt_sample.sample_rtt.ToMicroseconds() < min_rtt_) {
      min_rtt_ = rtt_sample.sample_rtt.ToMicroseconds();
    }
    if (interval_stats_.min_rtt < 0 ||
        rtt_sample.sample_rtt.ToMicroseconds() < interval_stats_.min_rtt) {
      interval_stats_.min_rtt =
          static_cast<float>(rtt_sample.sample_rtt.ToMicroseconds());
    }
    if (interval_stats_.max_rtt < 0 ||
        rtt_sample.sample_rtt.ToMicroseconds() > interval_stats_.max_rtt) {
      interval_stats_.max_rtt =
          static_cast<float>(rtt_sample.sample_rtt.ToMicroseconds());
    }
  }
  interval_stats_.rtt_dev =
      sqrt(interval_stats_.rtt_dev /
      static_cast<float>(interval->num_reliable_rtt));
}

void PccUtilityManager::ProcessRttTrend(
    const MonitorInterval* interval) {
  if (interval->num_reliable_rtt < 2) {
    return;
  }

  mi_avg_rtt_history_.emplace_back(interval_stats_.avg_rtt);
  mi_rtt_dev_history_.emplace_back(interval_stats_.rtt_dev);
  if (mi_avg_rtt_history_.size() > kRttHistoryLen) {
    mi_avg_rtt_history_.pop_front();
  }
  if (mi_rtt_dev_history_.size() > kRttHistoryLen) {
    mi_rtt_dev_history_.pop_front();
  }

  if (mi_avg_rtt_history_.size() >= kRttHistoryLen) {
    ComputeTrendingGradient();
    ComputeTrendingGradientError();

    DetermineToleranceInflation();
  }

  if (mi_rtt_dev_history_.size() >= kRttHistoryLen) {
    ComputeTrendingDeviation();

    DetermineToleranceDeviation();
  }
}

void PccUtilityManager::ComputeTrendingGradient() {
  // Calculate RTT gradient using linear regression.
  float gradient_x_avg = 0.0;
  float gradient_y_avg = 0.0;
  float gradient_x = 0.0;
  float gradient_y = 0.0;
  size_t num_sample = mi_avg_rtt_history_.size();
  for (size_t i = 0; i < num_sample; ++i) {
    gradient_x_avg += static_cast<float>(i);
    gradient_y_avg += mi_avg_rtt_history_[i];
  }
  gradient_x_avg /= static_cast<float>(num_sample);
  gradient_y_avg /= static_cast<float>(num_sample);
  for (size_t i = 0; i < num_sample; ++i) {
    float delta_x = static_cast<float>(i) - gradient_x_avg;
    float delta_y = mi_avg_rtt_history_[i] - gradient_y_avg;
    gradient_x += delta_x * delta_x;
    gradient_y += delta_x * delta_y;
  }
  interval_stats_.trending_gradient = gradient_y / gradient_x;
  interval_stats_.trending_gradient_cut =
      gradient_y_avg - interval_stats_.trending_gradient * gradient_x_avg;
}

void PccUtilityManager::ComputeTrendingGradientError() {
  size_t num_sample = mi_avg_rtt_history_.size();
  interval_stats_.trending_gradient_error = 0.0;
  for (size_t i = 0; i < num_sample; ++i) {
    float regression_rtt =
        static_cast<float>(i) * interval_stats_.trending_gradient +
        interval_stats_.trending_gradient_cut;
    interval_stats_.trending_gradient_error +=
        pow(mi_avg_rtt_history_[i] - regression_rtt, 2.0);
  }
  interval_stats_.trending_gradient_error /= static_cast<float>(num_sample);
  interval_stats_.trending_gradient_error =
      sqrt(interval_stats_.trending_gradient_error);
}

void PccUtilityManager::ComputeTrendingDeviation() {
  size_t num_sample = mi_rtt_dev_history_.size();
  float avg_rtt_dev = 0.0;
  for (size_t i = 0; i < num_sample; ++i) {
    avg_rtt_dev += mi_rtt_dev_history_[i];
  }
  avg_rtt_dev /= static_cast<float>(num_sample);

  interval_stats_.trending_deviation = 0.0;
  for (size_t i = 0; i < num_sample; ++i) {
    float delta_dev = avg_rtt_dev - mi_rtt_dev_history_[i];
    interval_stats_.trending_deviation += (delta_dev * delta_dev);
  }
  interval_stats_.trending_deviation /= static_cast<float>(num_sample);
  interval_stats_.trending_deviation = sqrt(interval_stats_.trending_deviation);
}

void PccUtilityManager::DetermineToleranceGeneral() {
  if (interval_stats_.rtt_gradient_error <
      std::abs(interval_stats_.rtt_gradient)) {
    is_rtt_inflation_tolerable_ = false;
    is_rtt_dev_tolerable_ = false;
  } else {
    is_rtt_inflation_tolerable_ = true;
    is_rtt_dev_tolerable_ = true;
  }
}

void PccUtilityManager::DetermineToleranceInflation() {
  ratio_inflated_mi_ *= (1 - kAlpha);

  if (utility_tag_ != "Scavenger" &&
    mi_avg_rtt_history_.size() < kRttHistoryLen) {
    return;
  }

  if (min_trending_gradient_ < 0.000001 ||
      std::abs(interval_stats_.trending_gradient) <
          min_trending_gradient_ / kBeta) {
    avg_trending_gradient_ = 0.0f;
    min_trending_gradient_ = std::abs(interval_stats_.trending_gradient);
    dev_trending_gradient_ = std::abs(interval_stats_.trending_gradient);
    last_trending_gradient_ = interval_stats_.trending_gradient;
  } else {
    float dev_gain = interval_stats_.rtt_dev < 1000
        ? kInflationToleranceGainLow : kInflationToleranceGainHigh;
    float tolerate_threshold_h =
        avg_trending_gradient_ + dev_gain * dev_trending_gradient_;
    float tolerate_threshold_l =
        avg_trending_gradient_ - dev_gain * dev_trending_gradient_;
    if (interval_stats_.trending_gradient < tolerate_threshold_l ||
        interval_stats_.trending_gradient > tolerate_threshold_h) {
      if (interval_stats_.trending_gradient > 0) {
        is_rtt_inflation_tolerable_ = false;
      }
      is_rtt_dev_tolerable_ = false;
      ratio_inflated_mi_ += kAlpha;
    } else {
      dev_trending_gradient_ =
          dev_trending_gradient_ * (1 - kAlpha) +
          std::abs(interval_stats_.trending_gradient -
                   last_trending_gradient_) * kAlpha;
      avg_trending_gradient_ =
          avg_trending_gradient_ * (1 - kAlpha) +
          interval_stats_.trending_gradient * kAlpha;
      last_trending_gradient_ = interval_stats_.trending_gradient;
    }

    min_trending_gradient_ =
        std::min(min_trending_gradient_,
                 std::abs(interval_stats_.trending_gradient));
  }

  /*if (ratio_inflated_mi_ > kTrendingResetIntervalRatio) {
    // TODO: reset based on minimum RTT observation.
    avg_trending_gradient_ = 0.0;
    dev_trending_gradient_ = min_trending_gradient_;
    ratio_inflated_mi_ = 0;
  }*/
}

void PccUtilityManager::DetermineToleranceDeviation() {
  ratio_fluctuated_mi_ *= (1 - kAlpha);

  if (avg_mi_rtt_dev_ < 0.000001) {
    avg_mi_rtt_dev_ = interval_stats_.rtt_dev;
    dev_mi_rtt_dev_ = 0.5 * interval_stats_.rtt_dev;
  } else {
    if (interval_stats_.rtt_dev > avg_mi_rtt_dev_ + dev_mi_rtt_dev_ * 4.0 &&
        interval_stats_.rtt_dev > 1000) {
      is_rtt_dev_tolerable_ = false;
      ratio_fluctuated_mi_ += kAlpha;
    } else {
      dev_mi_rtt_dev_ =
          dev_mi_rtt_dev_ * (1 - kAlpha) +
          std::abs(interval_stats_.rtt_dev - avg_mi_rtt_dev_) * kAlpha;
      avg_mi_rtt_dev_ =
          avg_mi_rtt_dev_ * (1 - kAlpha) + interval_stats_.rtt_dev * kAlpha;
    }
  }

  if (ratio_fluctuated_mi_ > kTrendingResetIntervalRatio) {
    avg_mi_rtt_dev_ = -1;
    dev_mi_rtt_dev_ = -1;
    ratio_fluctuated_mi_ = 0;
  }
}

// }  // namespace quic
