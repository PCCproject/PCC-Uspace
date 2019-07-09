#ifndef THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_H_
#define THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_H_

#include <utility>
#include <vector>
#include <iostream>

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
#include "net/quic/core/congestion_control/send_algorithm_interface.h"
#include "net/quic/core/quic_time.h"
#include "net/quic/core/quic_types.h"
#else
#include "gfe/quic/core/congestion_control/send_algorithm_interface.h"
#include "gfe/quic/core/quic_time.h"
#include "gfe/quic/core/quic_types.h"
#endif
#else
#include <cstdint>
#include <cstdlib>
#include <cmath>
#endif

#ifndef QUIC_PORT
typedef int32_t QuicPacketCount;
typedef int32_t QuicPacketNumber;
typedef int64_t QuicByteCount;
typedef int64_t QuicTime;
typedef double  QuicBandwidth;
#endif

#ifdef QUIC_PORT
#ifdef QUIC_PORT_LOCAL
namespace net {
namespace {
bool FLAGS_use_utility_version_2 = true;
}
#else
namespace gfe_quic {
DECLARE_bool(use_utility_version_2);
#endif
using namespace net;
#endif

// PacketRttSample, stores the packet number and its corresponding RTT
struct PacketRttSample {
  PacketRttSample();
  #ifdef QUIC_PORT
  PacketRttSample(QuicPacketNumber packet_number, QuicTime::Delta rtt);
  #else
  PacketRttSample(QuicPacketNumber packet_number, QuicTime rtt);
  #endif
  ~PacketRttSample() {}

  // Packet number of the sampled packet.
  QuicPacketNumber packet_number;
  // RTT corresponding to the sampled packet.
  #ifdef QUIC_PORT
  QuicTime::Delta rtt;
  #else
  QuicTime rtt;
  #endif
};

// MonitorInterval, as the queue's entry struct, stores the information
// of a PCC monitor interval (MonitorInterval) that can be used to
// - pinpoint a acked/lost packet to the corresponding MonitorInterval,
// - calculate the MonitorInterval's utility value.
struct MonitorInterval {
 //friend class MonitorIntervalMetric;
 //public:
  MonitorInterval(QuicBandwidth sending_rate, QuicTime end_time);
  #if defined(QUIC_PORT) && defined(QUIC_PORT_LOCAL)
  explicit MonitorInterval(const MonitorInterval&);
  #endif
  #ifdef QUIC_PORT_LOCAL
  ~MonitorInterval();
  #else
  ~MonitorInterval() {}
  #endif

  void OnPacketSent(QuicTime cur_time, QuicPacketNumber packet_num, QuicByteCount packet_size);
  void OnPacketAcked(QuicTime cur_time, QuicPacketNumber packet_num, QuicByteCount packet_size, QuicTime rtt);
  void OnPacketLost(QuicTime cur_time, QuicPacketNumber packet_num, QuicByteCount packet_size);

  bool AllPacketsSent(QuicTime cur_time) const;
  bool AllPacketsAccountedFor(QuicTime cur_time);

  QuicBandwidth GetTargetSendingRate() const;
  QuicTime GetStartTime() const;

  void SetUtility(float utility);
  
  QuicBandwidth GetObsThroughput() const;
  QuicBandwidth GetObsSendingRate() const;
  float GetObsSendDur() const;
  float GetObsRecvDur() const;
  float GetObsRtt() const;
  float GetObsRttInflation() const;
  float GetObsLossRate() const;
  float GetObsUtility() const;

  int GetId() const { return id; }

  int GetBytesSent() const { return bytes_sent; } 
  int GetBytesAcked() const { return bytes_acked; } 
  int GetBytesLost() const { return bytes_lost; } 

  uint64_t GetSendStartTime() const { return first_packet_sent_time; }
  uint64_t GetSendEndTime() const { return last_packet_sent_time; }
  uint64_t GetRecvStartTime() const { return first_packet_ack_time; }
  uint64_t GetRecvEndTime() const { return last_packet_ack_time; }

  uint64_t GetFirstAckLatency() const;
  uint64_t GetLastAckLatency() const;

  int GetAveragePacketSize() const { return bytes_sent / n_packets_sent; }
  double GetUtility() const { return utility; }

 private:
  static int next_id;

  bool ContainsPacket(QuicPacketNumber packet_num);

  int id;

  // Sending rate.
  QuicBandwidth target_sending_rate;
  // The end time for this monitor interval in microseconds.
  QuicTime end_time;

  // Sent time of the first packet.
  QuicTime first_packet_sent_time;
  // Sent time of the last packet.
  QuicTime last_packet_sent_time;

  // Sent time of the first packet.
  QuicTime first_packet_ack_time;
  // Sent time of the last packet.
  QuicTime last_packet_ack_time;

  // PacketNumber of the first sent packet.
  QuicPacketNumber first_packet_number;
  // PacketNumber of the last sent packet.
  QuicPacketNumber last_packet_number;
  // PacketNumber of the last packet whose status is known (acked/lost).
  QuicPacketNumber last_packet_number_accounted_for;

  // Number of bytes which are sent in total.
  QuicByteCount bytes_sent;
  // Number of bytes which have been acked.
  QuicByteCount bytes_acked;
  // Number of bytes which are considered as lost.
  QuicByteCount bytes_lost;

  // Utility value of this MonitorInterval, which is calculated
  // when all sent packets are accounted for.
  float utility;

  // The number of packets in this monitor interval.
  int n_packets_sent;

  // The number of packets whose return status is known.
  int n_packets_accounted_for;

  // A sample of the RTT for each packet.
  std::vector<PacketRttSample> packet_rtt_samples;
};

#ifdef QUIC_PORT
} // namespace gfe_quic
#endif

#endif  // THIRD_PARTY_PCC_QUIC_PCC_MONITOR_INTERVAL_H_
