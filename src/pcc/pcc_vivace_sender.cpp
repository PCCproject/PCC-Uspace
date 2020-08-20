#include "pcc_vivace_sender.h"

#include <assert.h>
#include <iostream>

#include <algorithm>

namespace {
// Number of bits per Mbit.
const size_t kMegabit = 1024 * 1024;
// Step size for rate change in PROBING mode.
const float kProbingStepSize = 0.05f;
// The factor that converts utility gradient to sending rate change.
float kUtilityGradientToRateChangeFactor = 1.0f;
// The exponent to amplify sending rate change based on number of consecutive
// rounds in DECISION_MADE mode.
float kRateChangeAmplifyExponent = 1.2f;
static bool FLAGS_restore_central_rate_upon_app_limited =  false;

// The initial maximum rate change step size in Vivace.
float kInitialMaxStepSize = 0.05f;
// The incremental rate change step size allowed on basis of current maximum
// step size every time the calculated rate change exceeds the current max.
float kIncrementalStepSize = 0.05;
// The smallest sending rate change allowed by Vivace.
const QuicBandwidth kMinRateChange = QuicBandwidth::FromKBitsPerSecond(500);
// The smallest sending rate supported by Vivace.
const QuicBandwidth kMinSendingRate = QuicBandwidth::FromKBitsPerSecond(500);
}


PccVivaceSender::PccVivaceSender(//const RttStats* rtt_stats,
                                 //const QuicUnackedPacketMap* unacked_packets,
                                 QuicPacketCount initial_congestion_window,
                                 QuicPacketCount max_congestion_window)
                                 // QuicRandom* random)
    : PccSender(//rtt_stats,
                //unacked_packets,
                initial_congestion_window,
                max_congestion_window),
                //random),
      latest_utility_info_(QuicBandwidth::Zero(), 0.0),
      incremental_rate_change_step_allowance_(0) {}

QuicBandwidth PccVivaceSender::GetSendingRateForNonUsefulInterval() const {
  return sending_rate_;
}

void PccVivaceSender::OnUtilityAvailable(
    const std::vector<const MonitorInterval *>& useful_intervals,
    QuicTime event_time) {
  // Calculate the utilities for all available intervals.
  std::vector<UtilityInfo> utility_info;
  for(size_t i = 0; i < useful_intervals.size(); ++i) {
    utility_info.push_back(
        UtilityInfo(useful_intervals[i]->sending_rate,
                    utility_manager_.CalculateUtility(
                        useful_intervals[i], event_time - conn_start_time_)));
    /*std::cerr << "End MI (rate: "
              << useful_intervals[i]->sending_rate.ToKBitsPerSecond()
              << ", rtt "
              << useful_intervals[i]->rtt_on_monitor_start.ToMicroseconds()
              << "->"
              << useful_intervals[i]->rtt_on_monitor_end.ToMicroseconds()
              << ", " << incremental_rate_change_step_allowance_
              << ", " << useful_intervals[i]->bytes_acked << "/"
              << useful_intervals[i]->bytes_sent << ") with utility "
              << utility_manager_.CalculateUtility(useful_intervals[i])
              << "(latest " << latest_utility_info_.utility << ")" << std::endl;*/
  }

  switch (mode_) {
    case STARTING:
      assert(utility_info.size() == 1u);
      if (utility_info[0].utility > latest_utility_info_.utility) {
        // Stay in STARTING mode. Double the sending rate and update
        // latest_utility.
        sending_rate_ = sending_rate_ * 2;
        latest_utility_info_ = utility_info[0];
        ++rounds_;
      } else {
        // Enter PROBING mode if utility decreases.
        PccSender::EnterProbing();
      }
      break;
    case PROBING:
      if (CanMakeDecision(utility_info)) {
        if (FLAGS_restore_central_rate_upon_app_limited &&
            interval_queue_.current().is_useful) {
          // If there is no non-useful interval in this round of PROBING, sender
          // needs to change sending_rate_ back to central rate.
          RestoreCentralSendingRate();
        }
        assert(utility_info.size() == 2 * GetNumIntervalGroupsInProbing());
        // Enter DECISION_MADE mode if a decision is made.
        SetRateChangeDirection(utility_info);
        EnterDecisionMade(utility_info);
      } else {
        PccSender::EnterProbing();
      }
      if ((rounds_ > 1 || mode_ == DECISION_MADE) && sending_rate_ <= kMinSendingRate) {
        sending_rate_ = kMinSendingRate;
        incremental_rate_change_step_allowance_ = 0;
        rounds_ = 1;
        mode_ = STARTING;
      }
      break;
    case DECISION_MADE:
      assert(utility_info.size() == 1u);
      if ((direction_ == INCREASE &&
           utility_info[0].utility > latest_utility_info_.utility &&
           utility_info[0].sending_rate > latest_utility_info_.sending_rate) ||
          (direction_ == INCREASE &&
           utility_info[0].utility < latest_utility_info_.utility &&
           utility_info[0].sending_rate < latest_utility_info_.sending_rate) ||
          (direction_ == DECREASE &&
           utility_info[0].utility > latest_utility_info_.utility &&
           utility_info[0].sending_rate < latest_utility_info_.sending_rate) ||
          (direction_ == DECREASE &&
           utility_info[0].utility < latest_utility_info_.utility &&
           utility_info[0].sending_rate > latest_utility_info_.sending_rate)) {
        // Remain in DECISION_MADE mode. Keep increasing or decreasing the
        // sending rate.
        EnterDecisionMade(utility_info);
        latest_utility_info_ = utility_info[0];
      } else {
        // Enter PROBING mode if utility decreases.
        EnterProbing(utility_info);
      }
      break;
  }
}

bool PccVivaceSender::CanMakeDecision(
    const std::vector<UtilityInfo>& utility_info) const {
  if (utility_info.size() < 2 * GetNumIntervalGroupsInProbing()) {
    return false;
  }

  size_t count_increase = 0;
  size_t count_decrease = 0;
  for (size_t i = 0; i < GetNumIntervalGroupsInProbing(); ++i) {
    bool increase_i =
        utility_info[2 * i].utility > utility_info[2 * i + 1].utility
            ? utility_info[2 * i].sending_rate >
                  utility_info[2 * i + 1].sending_rate
            : utility_info[2 * i].sending_rate <
                  utility_info[2 * i + 1].sending_rate;

    if (increase_i) {
      count_increase++;
    } else {
      count_decrease++;
    }
  }

  return utility_manager_.GetEffectiveUtilityTag() != "Scavenger"
      ? (count_increase > GetNumIntervalGroupsInProbing() / 2 ||
         count_decrease == GetNumIntervalGroupsInProbing())
      : (count_decrease > GetNumIntervalGroupsInProbing() / 2 ||
         count_increase == GetNumIntervalGroupsInProbing());
}

void PccVivaceSender::SetRateChangeDirection(
    const std::vector<UtilityInfo>& utility_info) {
  size_t count_increase = 0;
  size_t count_decrease = 0;
  for (size_t i = 0; i < GetNumIntervalGroupsInProbing(); ++i) {
    bool increase_i =
        utility_info[2 * i].utility > utility_info[2 * i + 1].utility
            ? utility_info[2 * i].sending_rate >
                  utility_info[2 * i + 1].sending_rate
            : utility_info[2 * i].sending_rate <
                  utility_info[2 * i + 1].sending_rate;

    if (increase_i) {
      count_increase++;
    } else {
      count_decrease++;
    }
  }

  direction_ = count_increase > count_decrease ? INCREASE : DECREASE;

  // Store latest utility in the meanwhile.
  for (size_t i = 0; i < GetNumIntervalGroupsInProbing(); ++i) {
    bool increase_i =
        utility_info[2 * i].utility > utility_info[2 * i + 1].utility
            ? utility_info[2 * i].sending_rate >
                  utility_info[2 * i + 1].sending_rate
            : utility_info[2 * i].sending_rate <
                  utility_info[2 * i + 1].sending_rate;

    if ((increase_i && direction_ == INCREASE) ||
        (!increase_i && direction_ == DECREASE)) {
      latest_utility_info_ =
          utility_info[2 * i].utility > utility_info[2 * i + 1].utility
              ? utility_info[2 * i]
              : utility_info[2 * i + 1];
    }
  }
}

void PccVivaceSender::EnterProbing(
    const std::vector<UtilityInfo>& utility_info) {
  assert(DECISION_MADE == mode_);
  rounds_ = 1;

  QuicBandwidth rate_change = ComputeRateChange(utility_info);
  if (direction_ == INCREASE) {
    sending_rate_ = sending_rate_ - rate_change;
  } else {
    sending_rate_ = sending_rate_ + rate_change;
  }

  if (sending_rate_ < kMinSendingRate) {
    sending_rate_ = kMinSendingRate;
    incremental_rate_change_step_allowance_ = 0;
  }
  mode_ = PROBING;

  if (utility_manager_.GetUtilityTag() == "Hybrid") {
    std::string effective_utility_tag = "Hybrid";
    float higher_probing_rate_mbps = static_cast<float>(
            (sending_rate_ * (1 + kProbingStepSize)).ToBitsPerSecond()) /
        static_cast<float>(kMegabit);
    float hybrid_switching_rate_mbps =
        *(float *)utility_manager_.GetUtilityParameter(0);
    if (higher_probing_rate_mbps > hybrid_switching_rate_mbps) {
      effective_utility_tag = "Scavenger";
    }
    utility_manager_.SetEffectiveUtilityTag(effective_utility_tag);
  }
}

void PccVivaceSender::EnterDecisionMade(
    const std::vector<UtilityInfo>& utility_info) {
  if (mode_ == PROBING) {
    sending_rate_ = direction_ == INCREASE
        ? sending_rate_ * (1 + kProbingStepSize)
        : sending_rate_ * (1 - kProbingStepSize);
  }

  rounds_ = mode_ == PROBING ? 1 : rounds_ + 1;

  QuicBandwidth rate_change = ComputeRateChange(utility_info);
  if (direction_ == INCREASE) {
    sending_rate_ = sending_rate_ + rate_change;
  } else {
    sending_rate_ = sending_rate_ - rate_change;
  }

  if (sending_rate_ < kMinSendingRate) {
    sending_rate_ = kMinSendingRate;
    mode_ = PROBING;
    rounds_ = 1;
    incremental_rate_change_step_allowance_ = 0;
  } else {
    mode_ = DECISION_MADE;
  }
}

QuicBandwidth PccVivaceSender::ComputeRateChange(
    const std::vector<UtilityInfo>& utility_info) {
  assert(mode_ != STARTING);

  // Compute rate difference between higher and lower sending rate, as well as
  // their utility difference.
  QuicBandwidth delta_sending_rate = QuicBandwidth::Zero();
  float delta_utility = 0.0;
  if (mode_ == PROBING) {
    delta_sending_rate =
        std::max(utility_info[0].sending_rate, utility_info[1].sending_rate) -
        std::min(utility_info[0].sending_rate, utility_info[1].sending_rate);

    for (size_t i = 0; i < GetNumIntervalGroupsInProbing(); ++i) {
      bool increase_i =
          utility_info[2 * i].utility > utility_info[2 * i + 1].utility
              ? utility_info[2 * i].sending_rate >
                    utility_info[2 * i + 1].sending_rate
              : utility_info[2 * i].sending_rate <
                    utility_info[2 * i + 1].sending_rate;
      if ((increase_i && direction_ == DECREASE) ||
          (!increase_i && direction_ == INCREASE)) {
        continue;
      }

      delta_utility = delta_utility +
          std::max(utility_info[2 * i].utility,
                   utility_info[2 * i + 1].utility) -
          std::min(utility_info[2 * i].utility,
                   utility_info[2 * i + 1].utility);
    }
    delta_utility /= static_cast<float>(GetNumIntervalGroupsInProbing());
  } else {
    delta_sending_rate =
        std::max(utility_info[0].sending_rate,
                 latest_utility_info_.sending_rate) -
        std::min(utility_info[0].sending_rate,
                 latest_utility_info_.sending_rate);
    delta_utility =
        std::max(utility_info[0].utility, latest_utility_info_.utility) -
        std::min(utility_info[0].utility, latest_utility_info_.utility);
  }

  assert(!delta_sending_rate.IsZero());

  float utility_gradient =
      kMegabit * delta_utility / delta_sending_rate.ToBitsPerSecond();
  QuicBandwidth rate_change = QuicBandwidth::FromBitsPerSecond(
      utility_gradient * kMegabit * kUtilityGradientToRateChangeFactor);
  if (mode_ == DECISION_MADE) {
    // Amplify rate change amount when sending rate changes towards the same
    // direction more than once.
    rate_change = rate_change * pow(static_cast<float>((rounds_ + 1) / 2),
                                    kRateChangeAmplifyExponent);
  } else {
    // Reset allowed incremental rate change step size upon entering PROBING.
    incremental_rate_change_step_allowance_ = 0;
  }

  QuicBandwidth max_allowed_rate_change =
      sending_rate_ * (kInitialMaxStepSize +
                       kIncrementalStepSize * static_cast<float>(
                           incremental_rate_change_step_allowance_));
  if (rate_change > max_allowed_rate_change) {
    rate_change = max_allowed_rate_change;
    // Increase incremental rate change step size if the calculated rate change
    // exceeds the current maximum.
    incremental_rate_change_step_allowance_++;
  } else if (incremental_rate_change_step_allowance_ > 0) {
    // Reduce incremental rate change allowance if calculated rate is smaller
    // than the current maximum.
    incremental_rate_change_step_allowance_--;
  }


  return std::max(rate_change, kMinRateChange);
}
