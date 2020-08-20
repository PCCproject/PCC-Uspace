#include "pcc_sender.h"

#include <assert.h>
#include <iostream>

//#include "third_party/pcc_quic/pcc_sender.h"
//#include "third_party/pcc_quic/pcc_utility_manager.h"

#include <algorithm>

//#include "base/commandlineflags.h"
//#include "third_party/quic/core/congestion_control/rtt_stats.h"
//#include "third_party/quic/core/quic_time.h"
//#include "third_party/quic/platform/api/quic_str_cat.h"

static bool FLAGS_enable_rtt_deviation_based_early_termination =  true;
static bool FLAGS_trigger_early_termination_based_on_interval_queue_front =  false;
static bool FLAGS_enable_early_termination_based_on_latest_rtt_trend =  false;
static double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting = 100.0;
static double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made =  1.0;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_starting =  2.5;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_decision_made =  1.5;
static double FLAGS_rtt_fluctuation_tolerance_gain_in_probing =  1.0;
static bool FLAGS_can_send_respect_congestion_window =  true;
static double FLAGS_bytes_in_flight_gain =  2.5;
static bool FLAGS_exit_starting_based_on_sampled_bandwidth =  false;
static bool FLAGS_restore_central_rate_upon_app_limited =  false;
// namespace quic {

/*
DEFINE_bool(enable_rtt_deviation_based_early_termination, false,
            "Whether trigger early termination by comparing inflated rtt with "
            "rtt deviation");
DEFINE_bool(trigger_early_termination_based_on_interval_queue_front, false,
            "Whether trigger early termination by comparing most recent "
            "smoothed rtt and the rtt upon start of the interval at the front "
            "of the interval queue. The current interval in the queue is used "
            "by default");
DEFINE_bool(enable_early_termination_based_on_latest_rtt_trend, false,
            "Whether trigger early termination by comparing latest RTT and "
            "smoothed RTT");
DEFINE_double(max_rtt_fluctuation_tolerance_ratio_in_starting, 0.3,
              "Ignore RTT fluctuation within 30 percent in STARTING mode");
DEFINE_double(max_rtt_fluctuation_tolerance_ratio_in_decision_made, 0.05,
              "Ignore RTT fluctuation within 5 percent in DECISION_MADE mode");
DEFINE_double(rtt_fluctuation_tolerance_gain_in_starting, 2.5,
              "Ignore rtt fluctuation within 2.5 multiple of rtt deviation in "
              "STARTING mode.");
DEFINE_double(rtt_fluctuation_tolerance_gain_in_decision_made, 1.5,
              "Ignore rtt fluctuation within 1.5 multiple of rtt deviation in "
              "DECISION_MADE modes.");
DEFINE_bool(can_send_respect_congestion_window, false,
            "Use calculated congestion window to determine whether CanSend "
            "should return true.");
DEFINE_double(bytes_in_flight_gain, 2.5,
              "Enable a specific multiple of approxmate cwnd bytes in flight.");
DEFINE_bool(exit_starting_based_on_sampled_bandwidth, false,
            "When exiting STARTING, fall back to the minimum of the max "
            "bandwidth by bandwidth sampler and half of current sending rate");
DEFINE_bool(restore_central_rate_upon_app_limited, false,
            "Whether restore to central probing rate when app limitation "
            "happens and sender does not have enough packets to start four "
            "monitor intervals in PROBING");
*/

namespace {
const size_t kNumIntervalGroupsInProbingScavenger = 2;
const size_t kNumIntervalGroupsInProbingPrimary = 3;

const QuicTime::Delta kInitialRtt = QuicTime::Delta::FromMicroseconds(100000);
const QuicByteCount kDefaultTCPMSS = 1400;
// Number of bits per Mbit.
const size_t kMegabit = 1024 * 1024;
// Minimum number of packets per monitor interval.
const size_t kMinPacketPerInterval = 5;
// Step size for rate change in PROBING mode.
const float kProbingStepSize = 0.05f;
// Base percentile step size for rate change in DECISION_MADE mode.
const float kDecisionMadeStepSize = 0.02f;
// Maximum percentile step size for rate change in DECISION_MADE mode.
const float kMaxDecisionMadeStepSize = 0.10f;
// Bandwidth filter window size in round trips.
const QuicRoundTripCount kBandwidthWindowSize = 6;
// The factor that converts utility gradient to sending rate change.
float kUtilityGradientToRateChangeFactor = 1.0f;
// The exponent to amplify sending rate change based on number of consecutive
// rounds in DECISION_MADE mode.
float kRateChangeAmplifyExponent = 1.2f;

// The minimum ratio of RTT samples being reliable per MI.
const float kMinReliabilityRatio = 0.8f;
}  // namespace

UtilityInfo::UtilityInfo()
    : sending_rate(QuicBandwidth::Zero()), utility(0.0) {}

UtilityInfo::UtilityInfo(QuicBandwidth rate, float utility)
    : sending_rate(rate), utility(utility) {}

/*
PccSender::DebugState::DebugState(const PccSender& sender)
    : mode(sender.mode_),
      sending_rate(sender.interval_queue_.current().sending_rate),
      latest_rtt(sender.latest_rtt_),
      smoothed_rtt(sender.avg_rtt_),
      rtt_dev(sender.rtt_deviation_),
      is_useful(sender.interval_queue_.current().is_useful),
      first_packet_sent_time(sender.interval_queue_.current()
                                 .first_packet_sent_time),
      last_packet_sent_time(sender.interval_queue_.current()
                                 .last_packet_sent_time),
      first_packet_number(sender.interval_queue_.current().first_packet_number),
      last_packet_number(sender.interval_queue_.current().last_packet_number),
      bytes_sent(sender.interval_queue_.current().bytes_sent),
      bytes_acked(sender.interval_queue_.current().bytes_acked),
      bytes_lost(sender.interval_queue_.current().bytes_lost),
      rtt_on_monitor_start(sender.interval_queue_.current()
                                  .rtt_on_monitor_start),
      rtt_on_monitor_end(sender.interval_queue_.current().rtt_on_monitor_end),
      latest_utility(sender.latest_utility_),
      bandwidth(sender.BandwidthEstimate()) {}
*/

PccSender::PccSender(//const RttStats* rtt_stats,
                     //const QuicUnackedPacketMap* unacked_packets,
                     QuicPacketCount initial_congestion_window,
                     QuicPacketCount max_congestion_window)
                     // QuicRandom* random)
    : mode_(STARTING),
      sending_rate_(
          QuicBandwidth::FromBytesAndTimeDelta(initial_congestion_window *
                                                   kDefaultTCPMSS,
                                               kInitialRtt)),
      has_seen_valid_rtt_(false),
      latest_utility_(0.0),
      conn_start_time_(QuicTime::Zero()),
      monitor_duration_(QuicTime::Delta::Zero()),
      direction_(INCREASE),
      rounds_(1),
      interval_queue_(/*delegate=*/this),
      rtt_on_inflation_start_(QuicTime::Delta::Zero()),
      max_cwnd_bytes_(max_congestion_window * kDefaultTCPMSS),
      rtt_deviation_(QuicTime::Delta::Zero()),
      min_rtt_deviation_(QuicTime::Delta::Zero()),
      latest_rtt_(QuicTime::Delta::Zero()),
      min_rtt_(QuicTime::Delta::Zero()),
      avg_rtt_(QuicTime::Delta::Zero()),
      // unacked_packets_(unacked_packets),
      // random_(random),
      // max_bandwidth_(kBandwidthWindowSize, QuicBandwidth::Zero(), 0),
      // last_sent_packet_(0),
      // current_round_trip_end_(0),
      // round_trip_count_(0),
      exit_starting_based_on_sampled_bandwidth_(
          FLAGS_exit_starting_based_on_sampled_bandwidth),
      latest_sent_timestamp_(QuicTime::Zero()),
      latest_ack_timestamp_(QuicTime::Zero()) {
}

void PccSender::OnPacketSent(QuicTime sent_time,
                             QuicByteCount bytes_in_flight,
                             QuicPacketNumber packet_number,
                             QuicByteCount bytes,
                             HasRetransmittableData is_retransmittable) {
  if (!conn_start_time_.IsInitialized()) {
    conn_start_time_ = sent_time;
    latest_sent_timestamp_ = sent_time;
  }

  // last_sent_packet_ = packet_number;

  // Do not process not retransmittable packets. Otherwise, the interval may
  // never be able to end if one of these packets gets lost.
  if (false && is_retransmittable != HAS_RETRANSMITTABLE_DATA) {
    return;
  }

  if (CreateNewInterval(sent_time)) {
    MaybeSetSendingRate();
    // Set the monitor duration to 1.0 of min rtt.
    monitor_duration_ = min_rtt_ * 1.0;

    bool is_useful = CreateUsefulInterval();
    interval_queue_.EnqueueNewMonitorInterval(
        is_useful ? sending_rate_ : GetSendingRateForNonUsefulInterval(),
        is_useful, GetMaxRttFluctuationTolerance(), avg_rtt_);
    /*std::cerr << (sent_time - QuicTime::Zero()).ToMicroseconds() << " "
              << "Create MI (useful: " << interval_queue_.current().is_useful
              << ") with rate " << interval_queue_.current().sending_rate
                                                            .ToKBitsPerSecond()
              << ", duration " << monitor_duration_.ToMicroseconds()
              << std::endl;*/
  }
  interval_queue_.OnPacketSent(sent_time, packet_number, bytes,
                               sent_time - latest_sent_timestamp_);
  latest_sent_timestamp_ = sent_time;

  if (exit_starting_based_on_sampled_bandwidth_) {
/*
    sampler_.OnPacketSent(sent_time, packet_number, bytes, bytes_in_flight,
                          is_retraprobing_rate_samples_[offset].utility = mi.GetObsUtility();nsmittable);
*/
  }
}

void PccSender::UpdateRtt(QuicTime event_time, QuicTime::Delta rtt) {
  latest_rtt_ = rtt;
  rtt_deviation_ = rtt_deviation_.IsZero()
      ? QuicTime::Delta::FromMicroseconds(rtt.ToMicroseconds() / 2)
      : QuicTime::Delta::FromMicroseconds(static_cast<int64_t>(
            0.75 * rtt_deviation_.ToMicroseconds() +
            0.25 * std::abs((avg_rtt_ - rtt).ToMicroseconds())));
  if (min_rtt_deviation_.IsZero() || rtt_deviation_ < min_rtt_deviation_) {
    min_rtt_deviation_ = rtt_deviation_;
  }

  avg_rtt_ = avg_rtt_.IsZero() ? rtt : avg_rtt_ * 0.875 + rtt * 0.125;
  if (min_rtt_.IsZero() || rtt < min_rtt_) {
    min_rtt_ = rtt;
  }
  //std::cerr << (event_time - QuicTime::Zero()).ToMicroseconds() << " New RTT "
  //          << rtt.ToMicroseconds() << std::endl;

  latest_ack_timestamp_ = event_time;
}

void PccSender::OnCongestionEvent(bool rtt_updated, QuicTime::Delta rtt,
                                  QuicByteCount bytes_in_flight,
                                  QuicTime event_time,
                                  const AckedPacketVector& acked_packets,
                                  const LostPacketVector& lost_packets) {
  if (!latest_ack_timestamp_.IsInitialized()) {
    latest_ack_timestamp_ = event_time;
  }

  if (exit_starting_based_on_sampled_bandwidth_) {
    // UpdateBandwidthSampler(event_time, acked_packets, lost_packets);
  }

  QuicTime::Delta ack_interval = QuicTime::Delta::Zero();
  if (rtt_updated) {
    ack_interval = event_time - latest_ack_timestamp_;
    UpdateRtt(event_time, rtt);
  }
  QuicTime::Delta avg_rtt = avg_rtt_;
  // QUIC_BUG_IF(avg_rtt.IsZero());
  if (!has_seen_valid_rtt_) {
    has_seen_valid_rtt_ = true;
    // Update sending rate if the actual RTT is smaller than initial rtt value
    // in RttStats, so PCC can start with larger rate and ramp up faster.
    if (latest_rtt_ < kInitialRtt) {
      sending_rate_ = sending_rate_ *
          (static_cast<float>(kInitialRtt.ToMicroseconds()) /
           static_cast<float>(latest_rtt_.ToMicroseconds()));
    }
  }
  if (mode_ == STARTING && CheckForRttInflation()) {
    // Directly enter PROBING when rtt inflation already exceeds the tolerance
    // ratio, so as to reduce packet losses and mitigate rtt inflation.
    interval_queue_.OnRttInflationInStarting();
    EnterProbing();
    return;
  }

  interval_queue_.OnCongestionEvent(acked_packets, lost_packets, avg_rtt,
                                    latest_rtt_, min_rtt_, event_time,
                                    ack_interval);
}

size_t PccSender::GetNumIntervalGroupsInProbing() const {
  if (utility_manager_.GetEffectiveUtilityTag() == "Scavenger") {
    return kNumIntervalGroupsInProbingScavenger;
  } else {
    return kNumIntervalGroupsInProbingPrimary;
  }
}

void PccSender::SetUtilityTag(std::string utility_tag) {
  utility_manager_.SetUtilityTag(utility_tag);
}

void PccSender::SetUtilityParameter(void* param) {
  utility_manager_.SetUtilityParameter(param);
}

bool PccSender::CanSend(QuicByteCount bytes_in_flight) {
  if (!FLAGS_can_send_respect_congestion_window) {
    return true;
  }

  if (interval_queue_.size() - interval_queue_.num_useful_intervals() > 4) {
    return false;
  } else {
    return true;
  }

  if (min_rtt_ < rtt_deviation_) {
    // Avoid capping bytes in flight on highly fluctuating path, because that
    // may impact throughput.
    return true;
  }

  return bytes_in_flight < FLAGS_bytes_in_flight_gain * GetCongestionWindow();
}

QuicBandwidth PccSender::PacingRate(QuicByteCount bytes_in_flight) const {
  return interval_queue_.empty() ? sending_rate_
                                 : interval_queue_.current().sending_rate;
}

/*
QuicBandwidth PccSender::BandwidthEstimate() const {
  return exit_starting_based_on_sampled_bandwidth_ ? max_bandwidth_.GetBest()
                                                   : QuicBandwidth::Zero();
}
*/

QuicByteCount PccSender::GetCongestionWindow() const {
  // Use min rtt to calculate expected congestion window except when it equals
  // 0, which happens when the connection just starts.
  return sending_rate_ * (min_rtt_.IsZero()
                              ? kInitialRtt
                              : min_rtt_);
}

/*
bool PccSender::InSlowStart() const { return false; }
*/

/*
bool PccSender::InRecovery() const { return false; }
*/

/*
bool PccSender::ShouldSendProbingPacket() const { return false; }
*/

/*
QuicByteCount PccSender::GetSlowStartThreshold() const { return 0; }
*/

/*
CongestionControlType PccSender::GetCongestionControlType() const {
  return kPCC;
}
*/

/* void PccSender::OnApplicationLimited(QuicByteCount bytes_in_flight) {
  if (!exit_starting_based_on_sampled_bandwidth_ ||
      bytes_in_flight >= GetCongestionWindow()) {
    return;
  }
  sampler_.OnAppLimited();
}
*/

/* QuicString PccSender::GetDebugState() const {
  if (interval_queue_.empty()) {
    return "pcc??";
  }

  std::ostringstream stream;
  stream << ExportDebugState();
  return stream.str();
}
*/

/*
PccSender::DebugState PccSender::ExportDebugState() const {
  return DebugState(*this);
}
*/

/* void PccSender::UpdateBandwidthSampler(QuicTime event_time,
                                       const AckedPacketVector& acked_packets,
                                       const LostPacketVector& lost_packets) {
  // This function should not be called if latched value of
  // FLAGS_exit_starting_based_on_sampled_bandwidth is false.
  DCHECK(exit_starting_based_on_sampled_bandwidth_);

  // Update round trip count if largest acked packet number exceeds largest
  // packet number in current round trip.
  if (!acked_packets.empty() &&
      acked_packets.rbegin()->packet_number > current_round_trip_end_) {
    round_trip_count_++;
    current_round_trip_end_ = last_sent_packet_;
  }
  // Calculate bandwidth based on the acked packets.
  for (const AckedPacket& packet : acked_packets) {
    if (packet.bytes_acked == 0) {
      continue;
    }
    BandwidthSample bandwidth_sample =
        sampler_.OnPacketAcknowledged(event_time, packet.packet_number);
    if (!bandwidth_sample.is_app_limited ||
        bandwidth_sample.bandwidth > BandwidthEstimate()) {
      max_bandwidth_.Update(bandwidth_sample.bandwidth, round_trip_count_);
    }
  }
  // Remove lost and obsolete packets from bandwidth sampler.
  for (const LostPacket& packet : lost_packets) {
    sampler_.OnPacketLost(packet.packet_number);
  }
  sampler_.RemoveObsoletePackets(unacked_packets_->GetLeastUnacked());
}
*/

void PccSender::OnUtilityAvailable(
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
              << ", " << useful_intervals[i]->rtt_fluctuation_tolerance_ratio
              << ", " << useful_intervals[i]->bytes_acked << "/"
              << useful_intervals[i]->bytes_sent << ") with utility "
              << utility_manager_.CalculateUtility(useful_intervals[i])
              << " (latest " << latest_utility_ << ")" << std::endl;*/
  }

  switch (mode_) {
    case STARTING:
      assert(utility_info.size() == 1u);
      if (utility_info[0].utility > latest_utility_) {
        // Stay in STARTING mode. Double the sending rate and update
        // latest_utility.
        sending_rate_ = sending_rate_ * 2;
        latest_utility_ = utility_info[0].utility;
        ++rounds_;
      } else {
        // Enter PROBING mode if utility decreases.
        EnterProbing();
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
        direction_ = (utility_info[0].utility > utility_info[1].utility)
                         ? ((utility_info[0].sending_rate >
                             utility_info[1].sending_rate)
                                ? INCREASE
                                : DECREASE)
                         : ((utility_info[0].sending_rate >
                             utility_info[1].sending_rate)
                                ? DECREASE
                                : INCREASE);
        latest_utility_ = std::max(
            utility_info[2 * GetNumIntervalGroupsInProbing() - 2].utility,
            utility_info[2 * GetNumIntervalGroupsInProbing() - 1].utility);
        EnterDecisionMade();
      } else {
        // Stays in PROBING mode.
        EnterProbing();
      }
      break;
    case DECISION_MADE:
      assert(utility_info.size() == 1u);
      if (utility_info[0].utility > latest_utility_) {
        // Remain in DECISION_MADE mode. Keep increasing or decreasing the
        // sending rate.
        ++rounds_;
        if (direction_ == INCREASE) {
          sending_rate_ = sending_rate_ *
                          (1 + std::min(rounds_ * kDecisionMadeStepSize,
                                        kMaxDecisionMadeStepSize));
        } else {
          sending_rate_ = sending_rate_ *
                          (1 - std::min(rounds_ * kDecisionMadeStepSize,
                                        kMaxDecisionMadeStepSize));
        }
        latest_utility_ = utility_info[0].utility;
      } else {
        // Enter PROBING mode if utility decreases.
        EnterProbing();
      }
      break;
  }
}

bool PccSender::CreateNewInterval(QuicTime event_time) {
  // Start a new monitor interval upon an empty interval queue.
  if (interval_queue_.empty()) {
    return true;
  }

  // Do not start new monitor interval before latest RTT is available.
  if (latest_rtt_.IsZero()) {
    return false;
  }

  // Start a (useful) interval if latest RTT is available but the queue does not
  // contain useful interval.
  if (interval_queue_.num_useful_intervals() == 0) {
    return true;
  }

  const MonitorInterval& current_interval = interval_queue_.current();
  // Do not start new interval if there is non-useful interval in the tail.
  if (!current_interval.is_useful) {
    return false;
  }

  // Do not start new interval until current useful interval has enough reliable
  // RTT samples, and its duration exceeds the monitor_duration_.
  if (!current_interval.has_enough_reliable_rtt ||
      event_time - current_interval.first_packet_sent_time <
          monitor_duration_) {
    return false;
  }

  if (static_cast<float>(current_interval.num_reliable_rtt) /
      static_cast<float>(current_interval.packet_rtt_samples.size()) >
          kMinReliabilityRatio) {
    // Start a new interval if current useful interval has an RTT reliability
    // ratio larger than kMinReliabilityRatio.
    return true;
  } else if (current_interval.is_monitor_duration_extended) {
    // Start a new interval if current useful interval has been extended once.
    return true;
  } else {
    // Extend the monitor duration if the current useful interval has not been
    // extended yet, and its RTT reliability ratio is lower than
    // kMinReliabilityRatio.
    monitor_duration_ = monitor_duration_ * 2.0;
    interval_queue_.extend_current_interval();
    return false;
  }
}

bool PccSender::CreateUsefulInterval() const {
  if (avg_rtt_.ToMicroseconds() == 0) {
    // Create non useful intervals upon starting a connection, until there is
    // valid rtt stats.
    assert(mode_ == STARTING);
    return false;
  }
  // In STARTING and DECISION_MADE mode, there should be at most one useful
  // intervals in the queue; while in PROBING mode, there should be at most
  // 2 * GetNumIntervalGroupsInProbing().
  size_t max_num_useful =
      (mode_ == PROBING) ? 2 * GetNumIntervalGroupsInProbing() : 1;
  return interval_queue_.num_useful_intervals() < max_num_useful;
}

QuicBandwidth PccSender::GetSendingRateForNonUsefulInterval() const {
  switch (mode_) {
    case STARTING:
      // Use halved sending rate for non-useful intervals in STARTING.
      return sending_rate_ * 0.5;
    case PROBING:
      // Use the smaller probing rate in PROBING.
      return sending_rate_ * (1 - kProbingStepSize);
    case DECISION_MADE:
      // Use the last (smaller) sending rate if the sender is increasing sending
      // rate in DECISION_MADE. Otherwise, use the current sending rate.
      return direction_ == DECREASE
          ? sending_rate_
          : sending_rate_ *
                (1.0 / (1 + std::min(rounds_ * kDecisionMadeStepSize,
                                     kMaxDecisionMadeStepSize)));
  }
}

void PccSender::MaybeSetSendingRate() {
  if (mode_ != PROBING || (interval_queue_.num_useful_intervals() ==
                               2 * GetNumIntervalGroupsInProbing() &&
                           !interval_queue_.current().is_useful)) {
    // Do not change sending rate when (1) current mode is STARTING or
    // DECISION_MADE (since sending rate is already changed in
    // OnUtilityAvailable), or (2) more than 2 * GetNumIntervalGroupsInProbing()
    // intervals have been created in PROBING mode.
    return;
  }

  if (interval_queue_.num_useful_intervals() != 0) {
    // Restore central sending rate.
    RestoreCentralSendingRate();

    if (interval_queue_.num_useful_intervals() ==
        2 * GetNumIntervalGroupsInProbing()) {
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
  } else {
    sending_rate_ = sending_rate_ * (1 - kProbingStepSize);
  }
}

float PccSender::GetMaxRttFluctuationTolerance() const {
  float tolerance_ratio =
      mode_ == STARTING
          ? FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting
          : FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made;
  return tolerance_ratio;

  if (FLAGS_enable_rtt_deviation_based_early_termination) {
    float tolerance_gain = 0.0;
    if (mode_ == STARTING) {
      tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_starting;
    } else if (mode_ == PROBING) {
      tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_probing;
    } else {
      tolerance_gain = FLAGS_rtt_fluctuation_tolerance_gain_in_decision_made;
    }
    tolerance_ratio = std::min(
        tolerance_ratio,
        tolerance_gain *
            static_cast<float>(rtt_deviation_.ToMicroseconds()) /
            static_cast<float>((avg_rtt_.IsZero()? kInitialRtt : avg_rtt_)
                                   .ToMicroseconds()));
  }

  return tolerance_ratio;
}

bool PccSender::CanMakeDecision(
    const std::vector<UtilityInfo>& utility_info) const {
  // Determine whether increased or decreased probing rate has better utility.
  // Cannot make decision if number of utilities are less than
  // 2 * GetNumIntervalGroupsInProbing(). This happens when sender does not have
  // enough data to send.
  if (utility_info.size() < 2 * GetNumIntervalGroupsInProbing()) {
    return false;
  }

  bool increase = false;
  // All the probing groups should have consistent decision. If not, directly
  // return false.
  for (size_t i = 0; i < GetNumIntervalGroupsInProbing(); ++i) {
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
      // Fall back to the minimum between halved sending rate and
      // max bandwidth * (1 - 0.05) if there is valid bandwidth sample.
      // Otherwise, simply halve the current sending rate.
      sending_rate_ = sending_rate_ * 0.5;
/*
      if (!BandwidthEstimate().IsZero()) {
        DCHECK(exit_starting_based_on_sampled_bandwidth_);
        sending_rate_ = std::min(sending_rate_,
                                 BandwidthEstimate() * (1 - kProbingStepSize));
      }
*/
      break;
    case DECISION_MADE:
      // FALLTHROUGH_INTENDED;
    case PROBING:
      // Reset sending rate to central rate when sender does not have enough
      // data to send more than 2 * GetNumIntervalGroupsInProbing() intervals.
      RestoreCentralSendingRate();
      break;
  }

  if (mode_ == PROBING) {
    ++rounds_;
    return;
  }

  mode_ = PROBING;
  rounds_ = 1;

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

void PccSender::EnterDecisionMade() {
  assert(PROBING == mode_);

  // Change sending rate from central rate based on the probing rate with higher
  // utility.
  if (direction_ == INCREASE) {
    sending_rate_ = sending_rate_ * (1 + kProbingStepSize) *
                    (1 + kDecisionMadeStepSize);
  } else {
    sending_rate_ = sending_rate_ * (1 - kProbingStepSize) *
                    (1 - kDecisionMadeStepSize);
  }

  mode_ = DECISION_MADE;
  rounds_ = 1;
}

void PccSender::RestoreCentralSendingRate() {
  switch (mode_) {
    case STARTING:
      // The sending rate upon exiting STARTING is set separately. This function
      // should not be called while sender is in STARTING mode.
      std::cerr << "Attempt to set probing rate while in STARTING";
      break;
    case PROBING:
      // Change sending rate back to central probing rate.
      if (interval_queue_.current().is_useful) {
        if (direction_ == INCREASE) {
          sending_rate_ = sending_rate_ * (1.0 / (1 + kProbingStepSize));
        } else {
          sending_rate_ = sending_rate_ * (1.0 / (1 - kProbingStepSize));
        }
      }
      break;
    case DECISION_MADE:
      if (direction_ == INCREASE) {
        sending_rate_ = sending_rate_ *
                        (1.0 / (1 + std::min(rounds_ * kDecisionMadeStepSize,
                                             kMaxDecisionMadeStepSize)));
      } else {
        sending_rate_ = sending_rate_ *
                        (1.0 / (1 - std::min(rounds_ * kDecisionMadeStepSize,
                                             kMaxDecisionMadeStepSize)));
      }
      break;
  }
}

bool PccSender::CheckForRttInflation() {
  if (interval_queue_.empty() ||
      interval_queue_.front().rtt_on_monitor_start.IsZero() ||
      latest_rtt_ <= avg_rtt_) {
    // RTT is not inflated if latest RTT is no larger than smoothed RTT.
    rtt_on_inflation_start_ = QuicTime::Delta::Zero();
    return false;
  }

  // Once the latest RTT exceeds the smoothed RTT, store the corresponding
  // smoothed RTT as the RTT at the start of inflation. RTT inflation will
  // continue as long as latest RTT keeps being larger than smoothed RTT.
  if (rtt_on_inflation_start_.IsZero()) {
    rtt_on_inflation_start_ = avg_rtt_;
  }

  const float max_inflation_ratio = 1 + GetMaxRttFluctuationTolerance();
  const QuicTime::Delta rtt_on_monitor_start =
      FLAGS_trigger_early_termination_based_on_interval_queue_front
          ? interval_queue_.front().rtt_on_monitor_start
          : interval_queue_.current().rtt_on_monitor_start;
  bool is_inflated =
      max_inflation_ratio * rtt_on_monitor_start < avg_rtt_;
  if (!is_inflated &&
      FLAGS_enable_early_termination_based_on_latest_rtt_trend) {
    // If enabled, check if early termination should be triggered according to
    // the stored smoothed rtt on inflation start.
    is_inflated = max_inflation_ratio * rtt_on_inflation_start_ <
                      avg_rtt_;
  }
  if (is_inflated) {
    // RTT is inflated by more than the tolerance, and early termination will be
    // triggered. Reset the rtt on inflation start.
    rtt_on_inflation_start_ = QuicTime::Delta::Zero();
  }
  return is_inflated;
}

/* static QuicString PccSenderModeToString(PccSender::SenderMode mode) {
  switch (mode) {
    case PccSender::STARTING:
      return "STARTING";
    case PccSender::PROBING:
      return "PROBING";
    case PccSender::DECISION_MADE:
      return "DECISION_MADE";
  }
  return "???";
}
*/

/*
std::ostream& operator<<(std::ostream& os, const PccSender::DebugState& state) {
  os << "Mode: " << PccSenderModeToString(state.mode) << std::endl;
  os << "Sending rate: " << state.sending_rate.ToKBitsPerSecond() << std::endl;
  os << "Latest rtt: " << state.latest_rtt.ToMicroseconds() << std::endl;
  os << "Smoothed rtt: " << state.smoothed_rtt.ToMicroseconds() << std::endl;
  os << "Rtt deviation: " << state.rtt_dev.ToMicroseconds() << std::endl;
  os << "Monitor useful: " << (state.is_useful ? "yes" : "no") << std::endl;
  os << "Monitor packet sent time: "
     << state.first_packet_sent_time.ToDebuggingValue() << " -> "
     << state.last_packet_sent_time.ToDebuggingValue() << std::endl;
  os << "Monitor packet number: " << state.first_packet_number << " -> "
     << state.last_packet_number << std::endl;
  os << "Monitor bytes: " << state.bytes_sent << " (sent), "
     << state.bytes_acked << " (acked), " << state.bytes_lost << " (lost)"
     << std::endl;
  os << "Monitor rtt change: " << state.rtt_on_monitor_start.ToMicroseconds()
     << " -> " << state.rtt_on_monitor_end.ToMicroseconds() << std::endl;
  os << "Latest utility: " << state.latest_utility << std::endl;
  os << "Bandwidth sample: " << state.bandwidth.ToKBitsPerSecond() << std::endl;

  return os;
}
*/

// }  // namespace quic
