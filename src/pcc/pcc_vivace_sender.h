#ifndef PCC_VIVACE_SENDER_H_
#define PCC_VIVACE_SENDER_H_

#include "pcc_sender.h"

class PccVivaceSender : public PccSender {
 public:
  PccVivaceSender(//const RttStats* rtt_stats,
                  //const QuicUnackedPacketMap* unacked_packets,
                  QuicPacketCount initial_congestion_window,
                  QuicPacketCount max_congestion_window); //, QuicRandom* random);
  PccVivaceSender(const PccVivaceSender&) = delete;
  PccVivaceSender& operator=(const PccVivaceSender&) = delete;
  PccVivaceSender(PccVivaceSender&&) = delete;
  PccVivaceSender& operator=(PccVivaceSender&&) = delete;
  ~PccVivaceSender() /*override*/ {}

  QuicBandwidth GetSendingRateForNonUsefulInterval() const override;

  void OnUtilityAvailable(
      const std::vector<const MonitorInterval *>& useful_intervals,
      QuicTime event_time) override;

 private:
  bool CanMakeDecision(
      const std::vector<UtilityInfo>& utility_info) const override;
  // Determine rate change direction in PROBING mode based on the utilities of
  // a majority of interval groups.
  void SetRateChangeDirection(const std::vector<UtilityInfo>& utility_info);

  void EnterProbing(const std::vector<UtilityInfo>& utility_info);
  void EnterDecisionMade(const std::vector<UtilityInfo>& utility_info);
  QuicBandwidth ComputeRateChange(const std::vector<UtilityInfo>& utility_info);

  // Most recent utility info used when making the last rate change decision.
  UtilityInfo latest_utility_info_;

  // Number of incremental rate change step size allowed on basis of initial
  // maximum rate change step size.
  size_t incremental_rate_change_step_allowance_;
};

// }  // namespace quic

#endif
