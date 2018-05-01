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
#include "pcc_vivace_rate_controller.h"
#endif

#include <algorithm>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {

#else
namespace gfe_quic {
#endif
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
QuicBandwidth kMinSendingRate = 0.1f * kMegabit;
// The smallest amount that the rate can be changed by at a time.
QuicBandwidth kMinimumRateChange = (int64_t)(0.2f * kMegabit);
// Number of microseconds per second.
const float kNumMicrosPerSecond = 1000000.0f;
// Default TCPMSS used in UDT only.
const size_t kDefaultTCPMSS = 1400;
// An inital RTT value to use (10ms)
const size_t kInitialRttMicroseconds = 1 * 1000;
#endif
// Number of bits per byte.
const size_t kBitsPerByte = 8;
// The factor that converts average utility gradient to a rate change (in Mbps).
float kUtilityGradientToRateChangeFactor = 1.0f * kMegabit;
// The initial maximum proportional rate change.
float kInitialMaximumProportionalChange = 0.05f;
// The additional maximum proportional change each time it is incremented.
float kMaximumProportionalChangeStepSize = 0.06f;
}  // namespace

PccVivaceRateController::PccVivaceRateController() {
    //rounds_ = 0;
    previous_change_ = 0;
    rate_change_proportion_allowance_ = 0.0;
    rate_change_amplifier_ = 0;
    swing_buffer_ = 0;
    in_startup_ = true;
}

QuicBandwidth PccVivaceRateController::GetNextSendingRate(
        PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        QuicBandwidth current_rate,
        QuicTime cur_time) {
    
    float utility_gradient = past_monitor_intervals.ComputeUtilityGradient();

    if (in_startup_ and utility_gradient > 0) {
        previous_change_ = current_rate;
        return current_rate * 2.0;
    }
    in_startup_ = false;

    QuicBandwidth change = kUtilityGradientToRateChangeFactor * utility_gradient;

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
  
    float change_ratio = 1.0;

    if (current_rate > 0) {
        #ifdef QUIC_PORT
        change_ratio = static_cast<float>(change.ToBitsPerSecond()) /
            static_cast<float>(sending_rate_.ToBitsPerSecond());
        #else
        change_ratio = (float)change / (float)current_rate;
        #endif
        change_ratio = change_ratio > 0 ? change_ratio : -1 * change_ratio;
    }

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
            change = -1 * max_allowed_change_ratio * current_rate;
        } else {
            change = max_allowed_change_ratio * current_rate;
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
    } else if (change >= QuicBandwidth::Zero() && change < kMinimumRateChange) {
    #else
    } else if (change >= 0 && change < kMinimumRateChange) {
    #endif
        change = kMinimumRateChange;
    }
    
    previous_change_ = change;
    QuicBandwidth new_rate = current_rate + change;
    if (new_rate < kMinSendingRate) {
        new_rate = kMinSendingRate;
    }
    previous_change_ = new_rate - current_rate;
    return new_rate;
}
