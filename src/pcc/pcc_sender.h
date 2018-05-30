// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_

#include <vector>
#include <queue>
#include <mutex>

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
#include "../core/options.h"
#include "rate_control/pcc_rc.h"
#include "rate_control/pcc_rc_factory.h"
#include "utility/pcc_ucalc.h"
#include "utility/pcc_ucalc_factory.h"
#include "monitor_interval/pcc_mi_analysis_group.h"
#include "monitor_interval/pcc_mi_queue.h"
#include "pcc_logger.h"
#include <iostream>
#define QUIC_EXPORT_PRIVATE

typedef bool HasRetransmittableData;
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
#else
namespace gfe_quic {
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
    : public SendAlgorithmInterface {
#else
          {
#endif
 public:

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
  #ifdef QUIC_PORT_LOCAL
  ~PccSender() override;
  #else
  ~PccSender();
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
  
  #ifndef QUIC_PORT
  PccEventLogger* log;
  void Reset();
  #endif

  QuicTime GetCurrentRttEstimate(QuicTime cur_time);

 private:
  #ifdef QUIC_PORT
  friend class test::PccSenderPeer;
  #endif
  
  void UpdateCurrentRttEstimate(QuicTime rtt);

  bool ShouldCreateNewMonitorInterval(QuicTime cur_time);
  QuicBandwidth UpdateSendingRate(QuicTime cur_time);

  // Sending rate in Mbit/s for the next monitor intervals.
  QuicBandwidth sending_rate_;
  
  // Queue of monitor intervals with pending utilities.
  PccMonitorIntervalQueue interval_queue_;
  // Group of previously measured monitor intervals.
  PccMonitorIntervalAnalysisGroup interval_analysis_group_;

  #ifdef QUIC_PORT
  const RttStats* rtt_stats_;
  QuicRandom* random_;
  #else
  QuicTime avg_rtt_;
  #endif
  
  PccUtilityCalculator* utility_calculator_;
  PccRateController* rate_controller_;
  std::mutex* rate_control_lock_;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif
