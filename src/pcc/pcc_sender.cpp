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
#include "pcc_logger.h"
#include "stdlib.h"
#include <random>
#endif

#ifdef QUIC_PORT
#define UDT_UNUSED
#else
#define UDT_UNUSED __attribute__((unused))
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
QuicBandwidth kMinSendingRate = 0.1f * kMegabit;
// The smallest amount that the rate can be changed by at a time.
QuicBandwidth kMinimumRateChange = (int64_t)(0.5f * kMegabit);
// Number of microseconds per second.
const float kNumMicrosPerSecond = 1000000.0f;
// Default TCPMSS used in UDT only.
const size_t kDefaultTCPMSS = 1400;
// An inital RTT value to use (10ms)
const size_t kInitialRttMicroseconds = 1 * 1000;
// Chance to make a random choice between 0 and 2x current rate.
float kRandomChoice = 0.0;
std::random_device kChoiceGenDevice;
std::mt19937 kChoiceGen(kChoiceGenDevice());
std::uniform_real_distribution<> kChoiceGenDis(0.0, 1.0);
#endif
// Step size for rate change in PROBING mode.
float kProbingStepSize = 0.05f;
// Groups of useful monitor intervals each time in PROBING mode.
size_t kNumIntervalGroupsInProbing = 2;
// Number of bits per byte.
const size_t kBitsPerByte = 8;
// Minimum number of packers per interval.
size_t kMinimumPacketsPerInterval = 10;
// Number of gradients to average.
size_t kAvgGradientSampleSize = 1;
// The factor that converts average utility gradient to a rate change (in Mbps).
float kUtilityGradientToRateChangeFactor = 1.0f * kMegabit;
// The initial maximum proportional rate change.
float kInitialMaximumProportionalChange = 0.05f;
// The additional maximum proportional change each time it is incremented.
float kMaximumProportionalChangeStepSize = 0.06f;
// The duration of a monitor interval with respect to RTT
float kMonitorIntervalDuration = 1.5f;
}  // namespace

bool PccSender::ReadRateControlParams() {
    const char* arg_dur = Options::Get("-mdur=");
    if (arg_dur != NULL) {
        kMonitorIntervalDuration = atof(arg_dur);
    }
    const char* arg_grad_samples = Options::Get("-gsample=");
    if (arg_grad_samples != NULL) {
        kAvgGradientSampleSize = atoi(arg_grad_samples);
    }
    const char* arg_prop_change = Options::Get("-propchange=");
    if (arg_prop_change != NULL) {
        kInitialMaximumProportionalChange = atof(arg_prop_change);
        kMaximumProportionalChangeStepSize = 1.2 * kInitialMaximumProportionalChange;
    }
    const char* arg_ugrcf = Options::Get("-ugrcf=");
    if (arg_ugrcf != NULL) {
        kUtilityGradientToRateChangeFactor = atof(arg_ugrcf);
    }
    const char* arg_probe_step = Options::Get("-probestep=");
    if (arg_probe_step != NULL) {
        kProbingStepSize = atof(arg_probe_step);
    }
    const char* arg_min_chg = Options::Get("-minchg=");
    if (arg_min_chg != NULL) {
        kMinimumRateChange = atof(arg_min_chg);
    }
    const char* arg_scale_all = Options::Get("-scaleall=");
    if (arg_scale_all != NULL) {
        float scale = atof(arg_scale_all);
        kInitialMaximumProportionalChange *= scale;
        kMaximumProportionalChangeStepSize = 1.2 * kInitialMaximumProportionalChange;
        kUtilityGradientToRateChangeFactor *= scale;
        kProbingStepSize *= scale;
        kMinimumRateChange *= scale; 
    }
    const char* arg_intervals = Options::Get("-intervals=");
    if (arg_intervals != NULL) {
        kNumIntervalGroupsInProbing = atoi(arg_intervals);
    }
    const char* arg_min_pkts = Options::Get("-minpkt=");
    if (arg_min_pkts != NULL) {
        kMinimumPacketsPerInterval = atoi(arg_min_pkts);
    }
    const char* arg_min_rate = Options::Get("-minrate=");
    if (arg_min_rate != NULL) {
        kMinSendingRate = atoi(arg_min_rate);
    }
    const char* arg_p_rand_rate = Options::Get("--p-rand-rate=");
    if (arg_p_rand_rate != NULL) {
        kRandomChoice = atof(arg_p_rand_rate);
    }

    PccLoggableEvent event("Rate Control Parameters", "-LOG_RATE_CONTROL_PARAMS");
    event.AddValue("Monitor Interval Duration", kMonitorIntervalDuration);
    event.AddValue("Average Gradient Sample Size", kAvgGradientSampleSize);
    event.AddValue("Initial Maximum Rate Change Proportion", kInitialMaximumProportionalChange);
    event.AddValue("Maximum Rate Change Proportion Step Size", kMaximumProportionalChangeStepSize);
    event.AddValue("Utility Gradient To Rate Change Factor", kUtilityGradientToRateChangeFactor);
    event.AddValue("Probing Step Size", kProbingStepSize);
    event.AddValue("Minimum Rate Change", kMinimumRateChange);
    event.AddValue("Number Of Interval Groups In Probing", kNumIntervalGroupsInProbing);
    event.AddValue("Minimum Packets Per Interval", kMinimumPacketsPerInterval);
    event.AddValue("Minimum Sending Rate", kMinSendingRate);
    event.AddValue("Probability of Random Rate", kRandomChoice);
    this->log->LogEvent(event);
    
    return true;
}

#ifdef QUIC_PORT
QuicTime::Delta PccSender::ComputeMonitorDuration(
    QuicBandwidth sending_rate, 
    QuicTime::Delta rtt) {

  return QuicTime::Delta::FromMicroseconds(
      std::max(1.5 * rtt.ToMicroseconds(), 
               kNumMicrosPerSecond * kMinimumPacketsPerInterval * kBitsPerByte * 
                   kDefaultTCPMSS / static_cast<float>(
                       sending_rate.ToBitsPerSecond())));
}
#else
QuicTime PccSender::ComputeMonitorDuration(
    QuicBandwidth sending_rate, 
    QuicTime rtt) {
  
  return
      std::max(kMonitorIntervalDuration * rtt, 
               kNumMicrosPerSecond * kMinimumPacketsPerInterval * kBitsPerByte * 
                   kDefaultTCPMSS / (float)sending_rate);
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
                     UDT_UNUSED QuicPacketCount max_congestion_window)
#endif
    :
#ifdef QUIC_PORT
      sending_rate_(QuicBandwidth::FromBitsPerSecond(
          initial_congestion_window * kDefaultTCPMSS * kBitsPerByte *
          kNumMicrosPerSecond / rtt_stats->initial_rtt_us()))
#else
      sending_rate_(
          initial_congestion_window * kDefaultTCPMSS * kBitsPerByte *
          kNumMicrosPerSecond / initial_rtt_us)
#endif
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
  if (Options::Get("-log=") == NULL) {
    log = new PccEventLogger("pcc_log.txt");
  } else {
    log = new PccEventLogger(Options::Get("-log="));    
  }
  latest_utility_info_.sending_rate = 0;
  ReadRateControlParams();
  py_helper = NULL;
  const char* py_helper_name = Options::Get("-pyhelper=");
  if (py_helper_name != NULL) {
    py_helper = new PccPythonHelper(py_helper_name);
  }
  #endif
}

#ifndef QUIC_PORT
PccSender::~PccSender() {
    delete log;
}

#endif
#if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
PccSender::~PccSender() {}

#endif
bool PccSender::ShouldCreateNewMonitorInterval(QuicTime sent_time) {
    return interval_queue_.empty() ||
        interval_queue_.Current().AllPacketsSent(sent_time);
}

void PccSender::UpdateCurrentRttEstimate(QuicTime rtt) {
    avg_rtt_ = rtt;
}

QuicTime PccSender::GetCurrentRttEstimate(QuicTime sent_time) {
    #ifdef QUIC_PORT
    return rtt_stats_->smoothed_rtt();
    #else
    return avg_rtt_;
    #endif
}

QuicBandwidth PccSender::UpdateSendingRate(QuicTime event_time) {
  sending_rate_ += step_size_ * interval_analyis_group_.ComputeUtilityGradient();
  return sending_rate_;
}

void PccSender::OnPacketSent(QuicTime sent_time,
                             UDT_UNUSED QuicByteCount bytes_in_flight,
                             QuicPacketNumber packet_number,
                             QuicByteCount bytes,
                             UDT_UNUSED HasRetransmittableData is_retransmittable) {

  if (ShouldCreateNewMonitorInterval(sent_time)) {
    // Set the monitor duration to 1.5 of smoothed rtt.
    QuicTime rtt_estimte = GetCurrentRttEstimate(sent_time);
    QuicTime monitor_duration = ComputeMonitorDuration(sending_rate,
        rtt_estimate); 
    float sending_rate = UpdateSendingRate(sent_time);
    interval_queue_.EnqueueMonitorInterval(
        MonitorInterval(sending_rate, sent_time + monitor_duration));
    
    #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
    printf("S %d | st=%d r=%6.3lf rtt=%7ld\n",
           is_useful, mode_,
           interval_queue_.current().sending_rate.ToKBitsPerSecond() / 1000.0,
           rtt_stats_->smoothed_rtt().ToMicroseconds());
    #endif
  }
  interval_queue_.OnPacketSent(sent_time, packet_number, bytes);
}

void PccSender::OnCongestionEvent(UDT_UNUSED bool rtt_updated,
                                  UDT_UNUSED QuicByteCount bytes_in_flight,
                                  QuicTime event_time,
  #ifndef QUIC_PORT
                                  QuicTime rtt,
  #endif
                                  const AckedPacketVector& acked_packets,
                                  const LostPacketVector& lost_packets) {
  
  #ifndef QUIC_PORT
  UpdateCurrentRttEstimate(rtt);
  #endif
  int64_t rtt_estimate = GetCurrentRttEstimate(event_time); 
  
  interval_queue_.OnCongestionEvent(acked_packets, 
                                    lost_packets,
                                    rtt_estimate, 
                                    event_time);

  while (interval_queue_.HasFinishedInterval()) {
    if (interval_analysis_group_.Full()) {
      interval_analysis_group_.RemoveOldestInterval();
      interval_analysis_group_.AddNewInterval(interval_queue_.Pop());
    }
  }
}

#ifdef QUIC_PORT
bool PccSender::CanSend(QuicByteCount bytes_in_flight) {
  return true;
}
#endif

QuicBandwidth PccSender::PacingRate(UDT_UNUSED QuicByteCount bytes_in_flight) const {
  QuicBandwidth result = interval_queue_.empty() ? sending_rate_
                                 : interval_queue_.Current().GetTargetSendingRate();
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

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

