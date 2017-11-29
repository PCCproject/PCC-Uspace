#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_sender.h"
#include "net/quic/core/congestion_control/rtt_stats.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/platform/api/quic_str_cat.h"
#else
#include "third_party/pcc_quic/pcc_sender.h"
#include "/quic/src/core/congestion_control/rtt_stats.h"
#include "/quic/src/net/platform/api/quic_str_cat.h"
#include "base_commandlineflags.h"
#endif
#else
#include "pcc_sender.h"

//#define DEBUG_RATE_CONTROL
#endif

#include <algorithm>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {

#else
namespace gfe_quic {

DEFINE_double(max_rtt_fluctuation_tolerance_ratio_in_starting, 0.3,
              "Ignore RTT fluctuation within 30 percent in STARTING mode");
DEFINE_double(max_rtt_fluctuation_tolerance_ratio_in_decision_made, 0.05,
              "Ignore RTT fluctuation within 5 percent in DECISION_MADE mode");
#endif
#endif

#if ! defined(QUIC_PORT) || defined(QUIC_PORT_LOCAL)
static float FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting = 0.3f;
static float FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made = 0.05f;
#endif

namespace {
// Number of bits per Mbit.
const size_t kMegabit = 1024 * 1024;
// Minimum sending rate of the connection.
#ifdef QUIC_PORT
const QuicBandwidth kMinSendingRate = QuicBandwidth::FromKBitsPerSecond(2000);
// The smallest amount that the rate can be changed by at a time.
const QuicBandwidth kMinimumRateChange = QuicBandwidth::FromBitsPerSecond(
    static_cast<int64_t>(0.5f * kMegabit));
#else
const QuicBandwidth kMinSendingRate = 2.0f * kMegabit;
// The smallest amount that the rate can be changed by at a time.
const QuicBandwidth kMinimumRateChange = (int64_t)(0.5f * kMegabit);
// Number of microseconds per second.
const float kNumMicrosPerSecond = 1000000.0f;
// Default TCPMSS used in UDT only.
const size_t kDefaultTCPMSS = 1400;
// An inital RTT value to use (10ms)
const size_t kInitialRttMicroseconds = 1 * 1000;
#endif
// Step size for rate change in PROBING mode.
const float kProbingStepSize = 0.05f;
// Base step size for rate change in DECISION_MADE mode.
const float kDecisionMadeStepSize = 0.02f;
// Maximum step size for rate change in DECISION_MADE mode.
const float kMaxDecisionMadeStepSize = 0.10f;
// Groups of useful monitor intervals each time in PROBING mode.
const size_t kNumIntervalGroupsInProbing = 2;
// Number of bits per byte.
const size_t kBitsPerByte = 8;
// Minimum number of packers per interval.
const size_t kMinimumPacketsPerInterval = 10;
// Number of gradients to average.
const size_t kAvgGradientSampleSize = 1;
// The factor that converts average utility gradient to a rate change (in Mbps).
float kUtilityGradientToRateChangeFactor = 1.0f * kMegabit;
// The initial maximum proportional rate change.
float kInitialMaximumProportionalChange = 0.05f;
// The additional maximum proportional change each time it is incremented.
float kMaximumProportionalChangeStepSize = 0.06f;
}  // namespace

#ifdef QUIC_PORT
QuicTime::Delta PccSender::ComputeMonitorDuration(
    QuicBandwidth sending_rate, 
    QuicTime::Delta rtt) {

  return QuicTime::Delta::FromMicroseconds(
      std::max(1.5f * rtt.ToMicroseconds(), 
               kMinimumPacketsPerInterval * kBitsPerByte * 
                   kDefaultTCPMSS / static_cast<float>(
                       sending_rate.ToBitsPerSecond())));
}
#else
QuicTime PccSender::ComputeMonitorDuration(
    QuicBandwidth sending_rate, 
    QuicTime rtt) {

  return
      std::max(1.5 * rtt, 
               kMinimumPacketsPerInterval * kBitsPerByte * 
                   kDefaultTCPMSS / sending_rate);
}
#endif

#ifdef QUIC_PORT
PccSender::PccSender(const RttStats* rtt_stats,
                     QuicPacketCount initial_congestion_window,
                     QuicPacketCount max_congestion_window,
                     QuicRandom* random)
#else
PccSender::PccSender(QuicTime initial_rtt_us,
                     QuicPacketCount initial_congestion_window,
                     QuicPacketCount max_congestion_window)
#endif
    : mode_(STARTING),
#ifdef QUIC_PORT
      sending_rate_(QuicBandwidth::FromBitsPerSecond(
          initial_congestion_window * kDefaultTCPMSS * kBitsPerByte *
          kNumMicrosPerSecond / rtt_stats->initial_rtt_us())),
      monitor_duration_(QuicTime::Delta::Zero()),
#else
      sending_rate_(
          initial_congestion_window * kDefaultTCPMSS * kBitsPerByte *
          kNumMicrosPerSecond / initial_rtt_us),
      monitor_duration_(0),
#endif
      direction_(INCREASE),
      rounds_(1),
      interval_queue_(/*delegate=*/this),
      #ifndef QUIC_PORT
      avg_rtt_(0),
      #endif
      avg_gradient_(0),
      swing_buffer_(0),
      rate_change_amplifier_(0),
      rate_change_proportion_allowance_(0),
      #ifdef QUIC_PORT
      previous_change_(QuicBandwidth::Zero()) {
      #else
      previous_change_(0) {
      #endif
  latest_utility_info_.utility = 0.0f;
  #ifdef QUIC_PORT
  latest_utility_info_.sending_rate = QuicBandwidth::Zero();
  #else
  latest_utility_info_.sending_rate = 0;
  #endif
}

#if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
PccSender::~PccSender() {}

#endif
void PccSender::OnPacketSent(QuicTime sent_time,
                             QuicByteCount bytes_in_flight,
                             QuicPacketNumber packet_number,
                             QuicByteCount bytes,
                             HasRetransmittableData is_retransmittable) {

  // Start a new monitor interval if the interval queue is empty. If latest RTT
  // is available, start a new monitor interval if (1) there is no useful
  // interval or (2) it has been more than monitor_duration since the last
  // interval starts.
  #ifdef QUIC_PORT
  if (interval_queue_.empty() ||
      (!rtt_stats_->latest_rtt().IsZero() &&
       (interval_queue_.num_useful_intervals() == 0 ||
        sent_time - interval_queue_.current().first_packet_sent_time >
            monitor_duration_))) {
  #else
  if (interval_queue_.num_useful_intervals() == 0 ||
      (avg_rtt_ != 0 &&
        sent_time - interval_queue_.current().first_packet_sent_time >
            monitor_duration_)) {
  #endif
    MaybeSetSendingRate();
    // Set the monitor duration to 1.5 of smoothed rtt.
    monitor_duration_ = ComputeMonitorDuration(sending_rate_, 
    #ifdef QUIC_PORT
                                               rtt_stats_->smoothed_rtt());
    #else
                                               avg_rtt_);
    #endif

    float rtt_fluctuation_tolerance_ratio = 0.0;
    // No rtt fluctuation tolerance no during PROBING.
    if (mode_ == STARTING) {
      // Use a larger tolerance at START to boost sending rate.
      rtt_fluctuation_tolerance_ratio =
          FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting;
    } else if (mode_ == DECISION_MADE) {
      rtt_fluctuation_tolerance_ratio =
          FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made;
    }

    bool is_useful = CreateUsefulInterval();
    interval_queue_.EnqueueNewMonitorInterval(
        sending_rate_, is_useful,
        rtt_fluctuation_tolerance_ratio,
    #ifdef QUIC_PORT
        rtt_stats_->smoothed_rtt().ToMicroseconds(), sent_time + monitor_duration_);
    #else
        avg_rtt_, sent_time + monitor_duration_);
    #endif
    #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
    printf("S %d | st=%d r=%6.3lf rtt=%7ld\n",
           is_useful, mode_,
           interval_queue_.current().sending_rate.ToKBitsPerSecond() / 1000.0,
           rtt_stats_->smoothed_rtt().ToMicroseconds());

    #endif
  }
  interval_queue_.OnPacketSent(sent_time, packet_number, bytes);
}

void PccSender::OnCongestionEvent(bool rtt_updated,
                                  QuicByteCount bytes_in_flight,
                                  QuicTime event_time,
  #ifndef QUIC_PORT
                                  QuicTime rtt,
  #endif
                                  const AckedPacketVector& acked_packets,
                                  const LostPacketVector& lost_packets) {
  #ifdef QUIC_PORT
  int64_t avg_rtt_us = rtt_stats_->smoothed_rtt().ToMicroseconds();
  #else
  int64_t avg_rtt_us = rtt;
  #endif

  if (avg_rtt_us == 0) {
    #ifdef QUIC_PORT
    QUIC_BUG_IF(mode_ != STARTING);
    avg_rtt_us = rtt_stats_->initial_rtt_us();
    #endif
  } else {
    #ifndef QUIC_PORT
    if (avg_rtt_ == 0) {
        avg_rtt_ = rtt;
    } else {
        avg_rtt_ = (avg_rtt_ * 3.0 + rtt) / 4.0;
    }
    #endif
    if (mode_ == STARTING && !interval_queue_.empty() &&
        interval_queue_.current().rtt_on_monitor_start_us != 0 &&
        avg_rtt_us >
            static_cast<int64_t>(
                (1 + FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting) *
                static_cast<float>(
                    interval_queue_.current().rtt_on_monitor_start_us))) {
      // Directly enter PROBING when rtt inflation already exceeds the tolerance
      // ratio, so as to reduce packet losses and mitigate rtt inflation.
      interval_queue_.OnRttInflationInStarting();
      EnterProbing();
      return;
    }
  }

  interval_queue_.OnCongestionEvent(acked_packets, 
                                    lost_packets,
                                    avg_rtt_us, 
                                    event_time);
}

#ifdef QUIC_PORT
bool PccSender::CanSend(QuicByteCount bytes_in_flight) {
  return true;
}
#endif

QuicBandwidth PccSender::PacingRate(QuicByteCount bytes_in_flight) const {
  QuicBandwidth result = interval_queue_.empty() ? sending_rate_
                                 : interval_queue_.current().sending_rate;
  return result;
}

#ifdef QUIC_PORT
QuicBandwidth PccSender::BandwidthEstimate() const {
  return QuicBandwidth::Zero();
}

QuicByteCount PccSender::GetCongestionWindow() const {
  // Use smoothed_rtt to calculate expected congestion window except when it
  // equals 0, which happens when the connection just starts.
  int64_t rtt_us = rtt_stats_->smoothed_rtt().ToMicroseconds() == 0
                       ? rtt_stats_->initial_rtt_us()
                       : rtt_stats_->smoothed_rtt().ToMicroseconds();
  return static_cast<QuicByteCount>(sending_rate_.ToBytesPerSecond() * rtt_us /
                                    kNumMicrosPerSecond);
}

bool PccSender::InSlowStart() const { return false; }

bool PccSender::InRecovery() const { return false; }

bool PccSender::IsProbingForMoreBandwidth() const { return false; }

QuicByteCount PccSender::GetSlowStartThreshold() const { return 0; }

CongestionControlType PccSender::GetCongestionControlType() const {
  return kPCC;
}

#ifdef QUIC_PORT_LOCAL
std::string PccSender::GetDebugState() const {
#else
string PccSender::GetDebugState() const {
#endif
  if (interval_queue_.empty()) {
    return "pcc??";
  }

  const MonitorInterval& mi = interval_queue_.current();
  std::string msg = QuicStrCat(
      "[st=", mode_, ",", "r=", sending_rate_.ToKBitsPerSecond(), ",",
      "pu=", QuicStringPrintf("%.15g", latest_utility_info_.utility), ",",
      "dir=", direction_, ",", "round=", rounds_, ",",
      "num=", interval_queue_.num_useful_intervals(), "]",
      "[r=", mi.sending_rate.ToKBitsPerSecond(), ",", "use=", mi.is_useful, ",",
      "(", mi.first_packet_sent_time.ToDebuggingValue(), "-", ">",
      mi.last_packet_sent_time.ToDebuggingValue(), ")", "(",
      mi.first_packet_number, "-", ">", mi.last_packet_number, ")", "(",
      mi.bytes_sent, "/", mi.bytes_acked, "/", mi.bytes_lost, ")", "(",
      mi.rtt_on_monitor_start_us, "-", ">", mi.rtt_on_monitor_end_us, ")");
  return msg;
}
#endif

QuicBandwidth PccSender::ComputeRateChange(
    const UtilityInfo& utility_sample_1, 
    const UtilityInfo& utility_sample_2) {

  if (utility_sample_1.sending_rate == utility_sample_2.sending_rate) {
    return kMinimumRateChange;
  }
  
  #ifdef QUIC_PORT
  float utility_gradient = 
      kMegabit * (utility_sample_1.utility - utility_sample_2.utility) / 
      static_cast<float>((utility_sample_1.sending_rate - 
          utility_sample_2.sending_rate).ToBitsPerSecond());
  #else
  float utility_gradient = 
      kMegabit * (utility_sample_1.utility - utility_sample_2.utility) / 
      static_cast<float>(utility_sample_1.sending_rate - 
          utility_sample_2.sending_rate);
  #endif

  UpdateAverageGradient(utility_gradient);
  #ifdef QUIC_PORT
  QuicBandwidth change = QuicBandwidth::FromBitsPerSecond(avg_gradient_ * kUtilityGradientToRateChangeFactor);
  #else
  QuicBandwidth change = avg_gradient_ * kUtilityGradientToRateChangeFactor;
  #endif

  #ifdef QUIC_PORT
  if ((change > QuicBandwidth::Zero()) != (previous_change_ > QuicBandwidth::Zero())) {
  #else
  if ((change > 0) != (previous_change_ > 0)) {
  #endif
    rate_change_amplifier_ = 0;
    rate_change_proportion_allowance_ = 0;
    if (swing_buffer_ < 2) {
      ++swing_buffer_;
    }
  }

  if (rate_change_amplifier_ < 3) {
    change = change * (rate_change_amplifier_ + 1);
  } else if (rate_change_amplifier_ < 6) {
    change = change * (2 * rate_change_amplifier_ - 2);
  } else if (rate_change_amplifier_ < 9) {
    change = change * (4 * rate_change_amplifier_ - 14);
  } else {
    change = change * (9 * rate_change_amplifier_ - 50);
  }

  #ifdef QUIC_PORT
  if ((change > QuicBandwidth::Zero()) == 
      (previous_change_ > QuicBandwidth::Zero())) {
  #else
  if ((change > 0) == (previous_change_ > 0)) {
  #endif
    if (swing_buffer_ == 0) {
      if (rate_change_amplifier_ < 3) {
        rate_change_amplifier_ += 0.5;
      } else {
        ++rate_change_amplifier_;
      }
    }
    if (swing_buffer_ > 0) {
      --swing_buffer_;
    }
  }

  float max_allowed_change_ratio = 
    kInitialMaximumProportionalChange + 
    rate_change_proportion_allowance_ * kMaximumProportionalChangeStepSize;
  
  #ifdef QUIC_PORT
  float change_ratio = static_cast<float>(change.ToBitsPerSecond()) /
      static_cast<float>(sending_rate_.ToBitsPerSecond());
  #else
  float change_ratio = (float)change / (float)sending_rate_;
  #endif
  change_ratio = change_ratio > 0 ? change_ratio : -1 * change_ratio;

  if (change_ratio > max_allowed_change_ratio) {
    ++rate_change_proportion_allowance_;
    #ifdef QUIC_PORT
    if (change < QuicBandwidth::Zero()) {
      change = QuicBandwidth::FromBitsPerSecond(static_cast<int64_t>(
          -1 * max_allowed_change_ratio * sending_rate_.ToBitsPerSecond()));
    } else {
      change = QuicBandwidth::FromBitsPerSecond(static_cast<int64_t>(
          max_allowed_change_ratio * sending_rate_.ToBitsPerSecond()));
    }
    #else
    if (change < 0) {
      change = -1 * max_allowed_change_ratio * sending_rate_;
    } else {
      change = max_allowed_change_ratio * sending_rate_;
    }
    #endif
  } else {
    if (rate_change_proportion_allowance_ > 0) {
      --rate_change_proportion_allowance_;
    }
  }

  #ifdef QUIC_PORT
  if ((change > QuicBandwidth::Zero()) != 
      (previous_change_ > QuicBandwidth::Zero())) {
  #else
  if ((change > 0) != (previous_change_ > 0)) {
  #endif
    rate_change_amplifier_ = 0;
    rate_change_proportion_allowance_ = 0;
  }

  #ifdef QUIC_PORT
  if (change < QuicBandwidth::Zero() && change > -1 * kMinimumRateChange) {
  #else
  if (change < 0 && change > -1 * kMinimumRateChange) {
  #endif
    change = -1 * kMinimumRateChange;
  #ifdef QUIC_PORT
  } else if (change > QuicBandwidth::Zero() && change < kMinimumRateChange) {
  #else
  } else if (change > 0 && change < kMinimumRateChange) {
  #endif
    change = kMinimumRateChange;
  }

  #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
    std::cerr << "CalculateRateChange:" << std::endl;
    std::cerr << "\tUtility 1    = " << utility_sample_1.utility << std::endl;
    std::cerr << "\tRate 1       = " << utility_sample_1.sending_rate << "mbps" << std::endl    ;
    std::cerr << "\tUtility 2    = " << utility_sample_2.utility << std::endl;
    std::cerr << "\tRate 2       = " << utility_sample_2.sending_rate << "mbps" << std::endl    ;
    std::cerr << "\tGradient     = " << utility_gradient << std::endl;
    std::cerr << "\tAvg Gradient = " << avg_gradient_ << std::endl;
    std::cerr << "\tRate Change  = " << change << "mbps" << std::endl;
  #endif

  return change;
}

void PccSender::UpdateAverageGradient(float new_gradient) {
  if (gradient_samples_.empty()) {
    avg_gradient_ = new_gradient;
  } else if (gradient_samples_.size() < kAvgGradientSampleSize) {
    avg_gradient_ *= gradient_samples_.size();
    avg_gradient_ += new_gradient;
    avg_gradient_ /= gradient_samples_.size() + 1;
  } else {
    float oldest_gradient = gradient_samples_.front();
    avg_gradient_ -= oldest_gradient / kAvgGradientSampleSize;
    avg_gradient_ += new_gradient / kAvgGradientSampleSize;
    gradient_samples_.pop();
  }
  gradient_samples_.push(new_gradient);
}

void PccSender::OnUtilityAvailable(
    const std::vector<UtilityInfo>& utility_info) {
  #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "OnUtilityAvailable" << std::endl;
  #endif
  switch (mode_) {
    case STARTING:
      #ifdef QUIC_PORT
      DCHECK_EQ(1u, utility_info.size());
      #endif
      if (utility_info[0].utility > latest_utility_info_.utility) {
        // Stay in STARTING mode. Double the sending rate and update
        // latest_utility.
        sending_rate_ = sending_rate_ * 2;
        #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
        std::cerr << "Starting mode rate: " << sending_rate_ / 2.0 << "-->" << sending_rate_ << std::endl;
        #endif
        latest_utility_info_ = utility_info[0];
        ++rounds_;
      } else {
        // Enter PROBING mode if utility decreases.
        EnterProbing();
      }
      break;
    case PROBING:
      if (CanMakeDecision(utility_info)) {
        #ifdef QUIC_PORT
        DCHECK_EQ(2 * kNumIntervalGroupsInProbing, utility_info.size());
        #endif
        // Enter DECISION_MADE mode if a decision is made.
        direction_ = (utility_info[0].utility > utility_info[1].utility)
                         ? ((utility_info[0].sending_rate >
                             utility_info[1].sending_rate)
                                ? INCREASE
                                : DECREASE)
                         : ((utility_info[0].sending_rate >
                             utility_info[1].sending_rate)
                                ? DECREASE
                                : INCREASE);
        latest_utility_info_ = 
            utility_info[2 * kNumIntervalGroupsInProbing - 2].utility >
            utility_info[2 * kNumIntervalGroupsInProbing - 1].utility ?
            utility_info[2 * kNumIntervalGroupsInProbing - 2] :
            utility_info[2 * kNumIntervalGroupsInProbing - 1];

        QuicBandwidth rate_change = 
            ComputeRateChange(utility_info[0], utility_info[1]);
        if (sending_rate_ + rate_change < kMinSendingRate) {
            rate_change = kMinSendingRate - sending_rate_;
        }
        previous_change_ = rate_change;
        EnterDecisionMade(sending_rate_ + rate_change);
      } else {
        // Stays in PROBING mode.
        EnterProbing();
      }
      break;
    case DECISION_MADE:
      #ifdef QUIC_PORT
      DCHECK_EQ(1u, utility_info.size());
      #endif
      QuicBandwidth rate_change = 
          ComputeRateChange(utility_info[0], latest_utility_info_);
      if (sending_rate_ + rate_change < kMinSendingRate) {
        rate_change = kMinSendingRate - sending_rate_;
      }
      // Test if we are adjusting sending rate in the same direction.
      #ifdef QUIC_PORT
      if ((rate_change > QuicBandwidth::Zero()) == (previous_change_ > QuicBandwidth::Zero())) {
      #else
      if ((rate_change > 0) == (previous_change_ > 0)) {
      #endif
        // Remain in DECISION_MADE mode. Keep increasing or decreasing the
        // sending rate.
        previous_change_ = rate_change;
        sending_rate_ = sending_rate_ + rate_change;
        #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
        std::cerr << "Decision made rate: " << sending_rate_ - rate_change << "-->" << sending_rate_ << std::endl;
        #endif
        latest_utility_info_ = utility_info[0];
      } else {
        // Enter PROBING if our old rate change is no longer best.
        EnterProbing();
      }
      break;
  }
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  printf("E T | st=%d r=%6.3lf rtt=%7ld\n",
         mode_, sending_rate_.ToKBitsPerSecond() / 1000.0,
         rtt_stats_->smoothed_rtt().ToMicroseconds());
  #endif
}

#if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
void PccSender::SetFlag(double val) {
  FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting = val;
  FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made = val;
}

#endif
bool PccSender::CreateUsefulInterval() const {
  #ifdef QUIC_PORT
  if (rtt_stats_->smoothed_rtt().ToMicroseconds() == 0) {
  #else
  if (avg_rtt_ == 0) {
  #endif
    // Create non useful intervals upon starting a connection, until there is
    // valid rtt stats.
    #ifdef QUIC_PORT
    QUIC_BUG_IF(mode_ != STARTING);
    #endif
    return false;
  }
  // In STARTING and DECISION_MADE mode, there should be at most one useful
  // intervals in the queue; while in PROBING mode, there should be at most
  // 2 * kNumIntervalGroupsInProbing.
  size_t max_num_useful =
      (mode_ == PROBING) ? 2 * kNumIntervalGroupsInProbing : 1;
  return interval_queue_.num_useful_intervals() < max_num_useful;
}

void PccSender::MaybeSetSendingRate() {
  if (mode_ != PROBING || (interval_queue_.num_useful_intervals() ==
                               2 * kNumIntervalGroupsInProbing &&
                           !interval_queue_.current().is_useful)) {
    // Do not change sending rate when (1) current mode is STARTING or
    // DECISION_MADE (since sending rate is already changed in
    // OnUtilityAvailable), or (2) more than 2 * kNumIntervalGroupsInProbing
    // intervals have been created in PROBING mode.
    return;
  }

  if (interval_queue_.num_useful_intervals() != 0) {
    // Restore central sending rate.
    if (direction_ == INCREASE) {
      sending_rate_ = sending_rate_ * (1.0 / (1 + kProbingStepSize));
      #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "Maybe undo increase: " << sending_rate_ * (1.0 + kProbingStepSize) << "-->" << sending_rate_ << std::endl;
      #endif
    } else {
      sending_rate_ = sending_rate_ * (1.0 / (1 - kProbingStepSize));
      #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "Maybe undo decrease: " << sending_rate_ * (1.0 - kProbingStepSize) << "-->" << sending_rate_ << std::endl;
      #endif
    }

    if (interval_queue_.num_useful_intervals() ==
        2 * kNumIntervalGroupsInProbing) {
      // This is the first not useful monitor interval, its sending rate is the
      // central rate.
      return;
    }
  }

  // Sender creates several groups of monitor intervals. Each group comprises an
  // interval with increased sending rate and an interval with decreased sending
  // rate. Which interval goes first is randomly decided.
  if (interval_queue_.num_useful_intervals() % 2 == 0) {
    direction_ = (rand() % 2 == 1) ? INCREASE : DECREASE;
  } else {
    direction_ = (direction_ == INCREASE) ? DECREASE : INCREASE;
  }
  if (direction_ == INCREASE) {
    sending_rate_ = sending_rate_ * (1 + kProbingStepSize);
    #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
    std::cerr << "Maybe probe increase: " << sending_rate_ / (1.0 + kProbingStepSize) << "-->" << sending_rate_ << std::endl;
    #endif
  } else {
    sending_rate_ = sending_rate_ * (1 - kProbingStepSize);
    #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
    std::cerr << "Maybe probe decrease: " << sending_rate_ / (1.0 - kProbingStepSize) << "-->" << sending_rate_ << std::endl;
    #endif
  }
}

bool PccSender::CanMakeDecision(
    const std::vector<UtilityInfo>& utility_info) const {
  // Determine whether increased or decreased probing rate has better utility.
  // Cannot make decision if number of utilities are less than
  // 2 * kNumIntervalGroupsInProbing. This happens when sender does not have
  // enough data to send.
  if (utility_info.size() < 2 * kNumIntervalGroupsInProbing) {
    return false;
  }

  bool increase = false;
  // All the probing groups should have consistent decision. If not, directly
  // return false.
  for (size_t i = 0; i < kNumIntervalGroupsInProbing; ++i) {
    bool increase_i =
        utility_info[2 * i].utility > utility_info[2 * i + 1].utility
            ? utility_info[2 * i].sending_rate >
                  utility_info[2 * i + 1].sending_rate
            : utility_info[2 * i].sending_rate <
                  utility_info[2 * i + 1].sending_rate;

    if (i == 0) {
      increase = increase_i;
    }
    // Cannot make decision if groups have inconsistent results.
    if (increase_i != increase) {
      return false;
    }
  }

  return true;
}

void PccSender::EnterProbing() {
  switch (mode_) {
    case STARTING:
      // Use half sending_rate_ as central probing rate.
      sending_rate_ = sending_rate_ * 0.5;
      #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "Probing after starting: " << sending_rate_ * 2.0 << "-->" << sending_rate_ << std::endl;
      #endif
      break;
    case DECISION_MADE:
      // Use sending rate right before utility decreases as central probing
      // rate.
      if (direction_ == INCREASE) {
        sending_rate_ = sending_rate_ *
                        (1.0 / (1 + std::min(rounds_ * kDecisionMadeStepSize,
                                             kMaxDecisionMadeStepSize)));
      #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "Decision made resotore: " << sending_rate_ * (1.0 + std::min(rounds_ * kDecisionMadeStepSize,
      kMaxDecisionMadeStepSize)) << "-->" << sending_rate_ << std::endl;
      #endif
      } else {
        sending_rate_ = sending_rate_ *
                        (1.0 / (1 - std::min(rounds_ * kDecisionMadeStepSize,
                                             kMaxDecisionMadeStepSize)));
      #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
      std::cerr << "Decision made resotore: " << sending_rate_ * (1.0 - std::min(rounds_ * kDecisionMadeStepSize,
      kMaxDecisionMadeStepSize)) << "-->" << sending_rate_ << std::endl;
      #endif
      }
      break;
    case PROBING:
      // Reset sending rate to central rate when sender does not have enough
      // data to send more than 2 * kNumIntervalGroupsInProbing intervals.
      if (interval_queue_.current().is_useful) {
        if (direction_ == INCREASE) {
          sending_rate_ = sending_rate_ * (1.0 / (1 + kProbingStepSize));
          #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
          std::cerr << "Probing restore: " << sending_rate_ * (1.0 + kProbingStepSize) << "-->" << sending_rate_ << std::endl;
          #endif
        } else {
          sending_rate_ = sending_rate_ * (1.0 / (1 - kProbingStepSize));
          #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
          std::cerr << "Probing restore: " << sending_rate_ * (1.0 - kProbingStepSize) << "-->" << sending_rate_ << std::endl;
          #endif
        }
      }
      break;
  }

  if (mode_ == PROBING) {
    ++rounds_;
    return;
  }

  mode_ = PROBING;
  rounds_ = 1;
}

void PccSender::EnterDecisionMade(QuicBandwidth new_rate) {
  #ifdef QUIC_PORT
  DCHECK_EQ(PROBING, mode_);
  #endif
  #if ! defined(QUIC_PORT) && defined(DEBUG_RATE_CONTROL)
  std::cerr << "Made decision: " << sending_rate_ << "-->" << new_rate << std::endl;
  #endif
  sending_rate_ = new_rate;
  mode_ = DECISION_MADE;
  rounds_ = 1;
}

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

