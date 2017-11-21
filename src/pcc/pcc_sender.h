// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_

#include <vector>
#include <queue>

#ifdef QUIC_PORT
#include "base/macros.h"
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/pcc_monitor_interval_queue.h"

#include "net/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/quic/core/quic_bandwidth.h"
#include "net/quic/core/quic_connection_stats.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "third_party/pcc_quic/pcc_monitor_interval_queue.h"

#include "gfe/quic/core/congestion_control/send_algorithm_interface.h"
#include "gfe/quic/core/quic_bandwidth.h"
#include "gfe/quic/core/quic_connection_stats.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include "pcc_monitor_interval_queue.h"
#include <iostream>
#define QUIC_EXPORT_PRIVATE

typedef bool HasRetransmittableData;
//namespace {
//double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_starting = 0.3;
//double FLAGS_max_rtt_fluctuation_tolerance_ratio_in_decision_made = 0.05;
//}
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
DECLARE_double(max_rtt_fluctuation_tolerance_ratio_in_starting);
DECLARE_double(max_rtt_fluctuation_tolerance_ratio_in_decision_made);
#endif

namespace test {
class PccSenderPeer;
} // namespace test
#endif

// PccSender implements the PCC congestion control algorithm. PccSender
// evaluates the benefits of different sending rates by comparing their
// utilities, and adjusts the sending rate towards the direction of
// higher utility.
class QUIC_EXPORT_PRIVATE PccSender
#ifdef QUIC_PORT
    : public SendAlgorithmInterface,
      public PccMonitorIntervalQueueDelegateInterface {
#else
          {
#endif
 public:
  // Sender's mode during a connection.
  enum SenderMode {
    // Initial phase of the connection. Sending rate gets doubled as
    // long as utility keeps increasing, and the sender enters
    // PROBING mode when utility decreases.
    STARTING,
    // Sender tries different sending rates to decide whether higher
    // or lower sending rate has greater utility. Sender enters
    // DECISION_MADE mode once a decision is made.
    PROBING,
    // Sender keeps increasing or decreasing sending rate until
    // utility decreases, then sender returns to PROBING mode.
    // TODO(tongmeng): a better name?
    DECISION_MADE
  };

  // Indicates whether sender should increase or decrease sending rate.
  enum RateChangeDirection { INCREASE, DECREASE };

  #ifdef QUIC_PORT
  PccSender(const RttStats* rtt_stats,
            QuicPacketCount initial_congestion_window,
            QuicPacketCount max_congestion_window, QuicRandom* random);
  #else
  PccSender(QuicTime initial_rtt_us,
            QuicPacketCount initial_congestion_window,
            QuicPacketCount max_congestion_window);
  #endif
  PccSender(const PccSender&) = delete;
  PccSender& operator=(const PccSender&) = delete;
  PccSender(PccSender&&) = delete;
  PccSender& operator=(PccSender&&) = delete;
  #ifdef QUIC_PORT
  #ifdef QUIC_PORT_LOCAL
  ~PccSender() override;
  #else
  ~PccSender() override {}
  #endif
  #endif

  #ifdef QUIC_PORT
  // Start implementation of SendAlgorithmInterface.
  bool InSlowStart() const override;
  bool InRecovery() const override;
  bool IsProbingForMoreBandwidth() const override;

  void SetFromConfig(const QuicConfig& config,
                     Perspective perspective) override {}

  void AdjustNetworkParameters(QuicBandwidth bandwidth,
                               QuicTime::Delta rtt) override {}
  void SetNumEmulatedConnections(int num_connections) override {}
  #endif
  void OnCongestionEvent(bool rtt_updated,
                         QuicByteCount bytes_in_flight,
                         QuicTime event_time,
  #ifndef QUIC_PORT
                         QuicTime rtt,
  #endif
                         const AckedPacketVector& acked_packets,
  #ifdef QUIC_PORT
                         const LostPacketVector& lost_packets) override;
  #else
                         const LostPacketVector& lost_packets);
  #endif
  void OnPacketSent(QuicTime sent_time,
                    QuicByteCount bytes_in_flight,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes,
  #ifdef QUIC_PORT
                    HasRetransmittableData is_retransmittable) override;
  void OnRetransmissionTimeout(bool packets_retransmitted) override {}
  void OnConnectionMigration() override {}
  bool CanSend(QuicByteCount bytes_in_flight) override;
  #else
                    HasRetransmittableData is_retransmittable);
  #endif
  #ifdef QUIC_PORT
  QuicBandwidth PacingRate(QuicByteCount bytes_in_flight) const override;
  #else
  QuicBandwidth PacingRate(QuicByteCount bytes_in_flight) const;
  #endif
  #ifdef QUIC_PORT
  QuicBandwidth BandwidthEstimate() const override;
  QuicByteCount GetCongestionWindow() const override;
  QuicByteCount GetSlowStartThreshold() const override;
  CongestionControlType GetCongestionControlType() const override;
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  std::string GetDebugState() const override;
  #else
  string GetDebugState() const override;
  #endif
  void OnApplicationLimited(QuicByteCount bytes_in_flight) override {}

  // End implementation of SendAlgorithmInterface.

  QuicTime::Delta ComputeMonitorDuration(
      QuicBandwidth sending_rate,
      QuicTime::Delta rtt);
  #else
  QuicTime ComputeMonitorDuration(
      QuicBandwidth sending_rate,
      QuicTime rtt);
  #endif

  QuicBandwidth ComputeRateChange(const UtilityInfo& utility_sample_1,
                                  const UtilityInfo& utility_sample_2);

  void UpdateAverageGradient(float new_gradient);

  // Implementation of PccMonitorIntervalQueueDelegate.
  // Called when all useful intervals' utilities are available,
  // so the sender can make a decision.
  void OnUtilityAvailable(
  #ifdef QUIC_PORT_LOCAL
      const std::vector<UtilityInfo>& utility_info) override;
  #else
      const std::vector<UtilityInfo>& utility_info);
  #endif
  
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  void SetFlag(double val);
  #endif
 private:
  #ifdef QUIC_PORT
  friend class test::PccSenderPeer;
  #endif
  // Returns true if next created monitor interval is useful,
  // i.e., its utility will be used when a decision can be made.
  bool CreateUsefulInterval() const;
  // Maybe set sending_rate_ for next created monitor interval.
  void MaybeSetSendingRate();

  // Returns true if the sender can enter DECISION_MADE from PROBING mode.
  bool CanMakeDecision(const std::vector<UtilityInfo>& utility_info) const;
  // Set the sending rate to the central rate used in PROBING mode.
  void EnterProbing();
  // Set the sending rate when entering DECISION_MADE from PROBING mode.
  void EnterDecisionMade(QuicBandwidth new_rate);

  // Current mode of PccSender.
  SenderMode mode_;
  // Sending rate in Mbit/s for the next monitor intervals.
  QuicBandwidth sending_rate_;
  // Most recent utility used when making the last rate change decision.
  UtilityInfo latest_utility_info_;
  // Duration of the current monitor interval.
  #ifdef QUIC_PORT
  QuicTime::Delta monitor_duration_;
  #else
  QuicTime monitor_duration_;
  #endif
  // Current direction of rate changes.
  RateChangeDirection direction_;
  // Number of rounds sender remains in current mode.
  size_t rounds_;
  // Queue of monitor intervals with pending utilities.
  PccMonitorIntervalQueue interval_queue_;
  // Maximum congestion window in bits, used to cap sending rate.
  uint32_t max_cwnd_bits_;
  // The current average of several utility gradients.
  float avg_gradient_;
  // The gradient samples that have been averaged.
  std::queue<float> gradient_samples_;

  #ifdef QUIC_PORT
  const RttStats* rtt_stats_;
  QuicRandom* random_;
  #else
  QuicTime avg_rtt_;
  #endif

  // The number of consecutive rate changes in a single direction
  // before we accelerate the rate of change.
  size_t swing_buffer_;
  // An acceleration factor for the rate of change.
  float rate_change_amplifier_;
  // The maximum rate change as a proportion of the current rate.
  size_t rate_change_proportion_allowance_;
  // The most recent change made to the sending rate.
  QuicBandwidth previous_change_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
