// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_SENDER_H_

#include "pcc_monitor_interval_queue.h"
#include <vector>
#include <queue>

class CUDT;

// PccSender implements the PCC congestion control algorithm. PccSender
// evaluates the benefits of different sending rates by comparing their
// utilities, and adjusts the sending rate towards the direction of
// higher utility.
class PccSender {
 public:

 void DumpIntervalPacketStates() { /*interval_queue_.DumpIntervalPacketStates();*/ }
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

  PccSender(CUDT* cudt, 
            int32_t initial_congestion_window,
            int32_t max_congestion_window);
  PccSender(const PccSender&) = delete;
  PccSender& operator=(const PccSender&) = delete;
  PccSender(PccSender&&) = delete;
  PccSender& operator=(PccSender&&) = delete;
  ~PccSender()  {}

  void OnCongestionEvent(uint64_t event_time,
                         uint64_t rtt,
                         const CongestionVector& acked_packets,
                         const CongestionVector& lost_packets) ;
  bool OnPacketSent(uint64_t sent_time,
                    int32_t packet_number,
                    int32_t bytes) ;
  // End implementation of SendAlgorithmInterface.

  void SetRate(double rate_mbps);

  float ComputeRateChange(float low_rate_utility,
                          float high_rate_utility,
                          float low_rate,
                          float high_rate);

  void UpdateAverageGradient(float new_gradient);

  // Implementation of PccMonitorIntervalQueueDelegate.
  // Called when all useful intervals' utilities are available,
  // so the sender can make a decision.
  void OnUtilityAvailable(
      const std::vector<UtilityInfo>& utility_info) ;

 private:
  // Returns true if next created monitor interval is useful,
  // i.e., its utility will be used when a decision can be made.
  bool CreateUsefulInterval() const;
  // Maybe set sending_rate_mbps_ for next created monitor interval.
  void MaybeSetSendingRate();

  // Returns true if the sender can enter DECISION_MADE from PROBING mode.
  bool CanMakeDecision(const std::vector<UtilityInfo>& utility_info) const;
  // Set the sending rate to the central rate used in PROBING mode.
  void EnterProbing();
  // Set the sending rate when entering DECISION_MADE from PROBING mode.
  void EnterDecisionMade(float new_rate);

  // Current mode of PccSender.
  SenderMode mode_;
  // Sending rate in Mbit/s for the next monitor intervals.
  float sending_rate_mbps_;
  // Most recent utility used when making the last rate change decision.
  float latest_utility_;
  // Duration of the current monitor interval.
  int32_t monitor_duration_;
  // Current direction of rate changes.
  RateChangeDirection direction_;
  // Number of rounds sender remains in current mode.
  size_t rounds_;
  // Queue of monitor intervals with pending utilities.
  PccMonitorIntervalQueue interval_queue_;

  // Moving average of rtt.
  float avg_rtt_;
  // The last rtt sample.
  float last_rtt_;
  // Timestamp when the last rtt sample is received.
  uint64_t time_last_rtt_received_;

  // Maximum congestion window in bits, used to cap sending rate.
  uint32_t max_cwnd_bits_;

  // The current average of several gradients.
  float avg_gradient_;

  // The gradient samples that have been averaged.
  std::queue<float> gradient_samples_;

  uint32_t swing_buffer_;

  uint32_t rate_change_amplifier_;
  
  uint32_t rate_change_proportion_allowance_;

  float previous_change_;

    CUDT* cudt;

};

#endif
