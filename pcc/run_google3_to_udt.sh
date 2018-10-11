#!/bin/bash

dir_google3="to-be-added"
dir_udt="to-be-added"

if [ \( ! -d "${dir_google3}" \) -o \( ! -d "${dir_udt}" \) ]
then
  echo "folder does not exist"
  exit 1
fi

func_comment_segments() {
  line="$(grep -n "${1}" ${3} | head -n 1 | cut -d: -f1)"
  sed -i "${line}i\/*" ${3}
  lines="$(grep -n "${2}" ${3} | cut -d: -f1)"
  for lno in ${lines}
  do
    if [[ ${lno} -gt ${line} ]]; then
      lno=$((${lno} + 1))
      sed -i "${lno}i*\/" ${file}
      break
    fi
  done
}

declare -a files_prefix=("pcc_monitor_interval_queue"
                         "pcc_sender" "pcc_utility_manager"
                        )

for prefix in "${files_prefix[@]}"
do
   cp ${dir_google3}/${prefix}.h ${dir_udt}/${prefix}.h
   cp ${dir_google3}/${prefix}.cc ${dir_udt}/${prefix}.cpp
done
cd ${dir_udt}

file="pcc_monitor_interval_queue.h"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}
sed -i '4i#include \"quic_types\/quic_bandwidth.h\"\n' ${file}
sed -i "s/\ QUIC_EXPORT_PRIVATE\ /\ /g" ${file}

file="pcc_monitor_interval_queue.cpp"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i '1i#include \"pcc_monitor_interval_queue.h\"\n' ${file}
sed -i '3i#include <assert.h>\n' ${file}
sed -i '4i#include <iostream>\n' ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/QUIC_BUG\ /std::cerr\ /g" ${file}
sed -i "s/QUIC_BUG_IF(num_available_intervals_ > num_useful_intervals_)/assert(num_available_intervals_ <= num_useful_intervals_)/g" ${file}
sed -i "s/DCHECK_GT(num_useful_intervals_, 0u)/assert(num_useful_intervals_ > 0u)/g" ${file}
sed -i "s/DCHECK_EQ(num_available_intervals_, useful_intervals.size())/assert(num_available_intervals_ == useful_intervals.size())/g" ${file}
sed -i "s/DCHECK(/assert(/g" ${file}

file="pcc_utility_manager.h"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i "4i#include \"pcc_monitor_interval_queue.h\"\n" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}

file="pcc_utility_manager.cpp"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i "1i#include \"pcc_utility_manager.h\"" ${file}
sed -i "2i#include <assert.h>\n" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}
line="$(grep -n "namespace {" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 1))
sed -i "${line}iconst QuicByteCount kMaxPacketSize = 1500;" ${file}
sed -i "s/QUIC_BUG_IF(interval->first_packet_sent_time ==/assert(interval->first_packet_sent_time !=/g" ${file}

file="pcc_sender.h"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i "s/#include \"base\/macros.h\"/\/\/#include \"base\/macros.h\"/g" ${file}
sed -i "6i#include \"pcc_monitor_interval_queue.h\"" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}
sed -i "s/namespace test {/\/\/ namespace test {/g" ${file}
sed -i "s/}  \/\/ namespace test/\/\/ }  \/\/ namespace test/g" ${file}
sed -i "s/class RttStats;/\/\/ class RttStats;/g" ${file}
sed -i "s/class PccSenderPeer;/\/\/ class PccSenderPeer;/g" ${file}
sed -i "s/class QUIC_EXPORT_PRIVATE/class/g" ${file}
sed -i "s/public SendAlgorithmInterface,/\/\/ public SendAlgorithmInterface,/g" ${file}
func_comment_segments "struct DebugState" "};" ${file}
sed -i "s/const RttStats\* rtt_stats,/\/\/const RttStats\* rtt_stats,/g" ${file}
sed -i "s/const QuicUnackedPacketMap\* unacked_packets,/\/\/const QuicUnackedPacketMap\* unacked_packets,/g" ${file}
sed -i "s/, QuicRandom\* random);/); \/\/, QuicRandom\* random);/g" ${file}
sed -i "s/~PccSender() override {}/~PccSender() \/\*override\*\/ {}/g" ${file}
sed -i "s/const LostPacketVector\& lost_packets) override;/const LostPacketVector\& lost_packets) \/\*override\*\/;/g" ${file}
sed -i "s/HasRetransmittableData is_retransmittable) override;/HasRetransmittableData is_retransmittable) \/\*override\*\/;/g" ${file}
sed -i "s/bytes_in_flight) const override/bytes_in_flight) const \/\*override\*\//g" ${file}
sed -i "s/bool CanSend(QuicByteCount bytes_in_flight) override;/bool CanSend(QuicByteCount bytes_in_flight) \/\*override\*\/;/g" ${file}
sed -i "s/GetCongestionWindow() const override;/GetCongestionWindow() const \/\*override\*\/;/g" ${file}
declare -a useless_func=("InSlowStart" "InRecovery" "ShouldSendProbingPacket"
                         "SetFromConfig" "SetInitialCongestionWindowInPackets"
                         "AdjustNetworkParameters" "SetNumEmulatedConnections"
                         "OnRetransmissionTimeout" "OnConnectionMigration"
                         "BandwidthEstimate" "GetSlowStartThreshold"
                         "GetCongestionControlType" "GetDebugState"
                         "OnApplicationLimited" "ExportDebugState"
                         "UpdateBandwidthSampler"
                        )
for func in "${useless_func[@]}"
do
  func_comment_segments ${func} ";\|}" ${file}
done
sed -i "s/bool rtt_updated,/bool rtt_updated, QuicTime::Delta rtt,/g" ${file}
line="$(grep -n "private:" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 1))
sed -i "${line}i\  void UpdateRtt(QuicTime event_time, QuicTime::Delta rtt);" ${file}
sed -i "s/friend class test::PccSenderPeer;/\/\/ friend class test::PccSenderPeer;/g" ${file}
sed -i "s/typedef WindowedFilter/\/\*typedef WindowedFilter/g" ${file}
sed -i "s/MaxBandwidthFilter;/MaxBandwidthFilter;\*\//g" ${file}
sed -i "s/const RttStats\* rtt_stats_;/QuicTime::Delta avg_rtt_;/g" ${file}
line="$(grep -n "QuicTime::Delta avg_rtt_;" ${file} | head -n 1 | cut -d: -f1)"
sed -i "${line}i\  QuicTime::Delta min_rtt_;" ${file}
sed -i "${line}i\  QuicTime::Delta latest_rtt_;" ${file}
sed -i "s/const QuicUnackedPacketMap\* unacked_packets_;/\/\/ const QuicUnackedPacketMap\* unacked_packets_;/g" ${file}
sed -i "s/QuicRandom\* random_;/\/\/ QuicRandom\* random_;/g" ${file}
sed -i "s/BandwidthSampler sampler_;/\/\/ BandwidthSampler sampler_;/g" ${file}
sed -i "s/MaxBandwidthFilter max_bandwidth_;/\/\/ MaxBandwidthFilter max_bandwidth_;/g" ${file}
sed -i "s/QuicPacketNumber last_sent_packet_;/\/\/ QuicPacketNumber last_sent_packet_;/g" ${file}
sed -i "s/QuicPacketNumber current_round_trip_end_;/\/\/ QuicPacketNumber current_round_trip_end_;/g" ${file}
sed -i "s/QuicRoundTripCount round_trip_count_;/\/\/ QuicRoundTripCount round_trip_count_;/g" ${file}
func_comment_segments "std::ostream\& operator<<" ");" ${file}

file="pcc_sender.cpp"
sed -i "s/#include \"third_party/\/\/#include \"third_party/g" ${file}
sed -i "1i#include \"pcc_sender.h\"\n" ${file}
sed -i "2i#include \"pcc_utility_manager.h\"" ${file}
sed -i "4i#include <assert.h>" ${file}
sed -i "5i#include <iostream>\n" ${file}
sed -i "s/namespace quic {/\/\/ namespace quic {/g" ${file}
sed -i "s/}  \/\/ namespace quic/\/\/ }  \/\/ namespace quic/g" ${file}
sed -i "s/#include \"base\/commandlineflags.h\"/\/\/#include \"base\/commandlineflags.h\"/g" ${file}
line="$(grep -n "DEFINE_" ${file} | head -n 1 | cut -d: -f1)"
sed -i "${line}i\/*" ${file}
line="$(grep -n "DEFINE_" ${file} | tail -n 1 | cut -d: -f1)"
lines="$(grep -n ");" ${file} | cut -d: -f1)"
for lno in ${lines}
do
  if [[ ${lno} > ${line} ]]; then
    lno=$((${lno} + 1))
    sed -i "${lno}i*\/" ${file}
    break
  fi
done
line="$(grep -n "#include" ${file} | tail -n 1 | cut -d: -f1)"
line=$((${line} + 2))
lineflag="$(grep -n "DEFINE_" ${file} | head -n 1 | cut -d: -f1)"
while [[ true ]]; do
  l=$(sed "${lineflag}!d" ${file})
  type=$(echo ${l}| cut -d'_' -f 2)
  type=$(echo ${type}| cut -d'(' -f 1)
  name=$(echo ${l}| cut -d'(' -f 2)
  name=$(echo ${name}| cut -d',' -f 1)
  value=$(echo ${l}| cut -d',' -f 2)
  sed -i "${line}istatic ${type} FLAGS_${name} = ${value};" ${file}
  line=$((${line} + 1))
  lineflag=$((${lineflag} + 1))
  linesflag="$(grep -n "DEFINE_" ${file} | cut -d: -f1)"
  for lno in ${linesflag}
  do
    if [[ ${lno} -gt ${lineflag} ]]; then
      break
    fi
  done
  if [[ ${lno} -gt ${lineflag} ]]; then
    lineflag=$((${lno}))
  else
    break
  fi
done
line="$(grep -n "namespace {" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 1))
sed -i "${line}iconst QuicByteCount kDefaultTCPMSS = 1400;" ${file}
sed -i "${line}iconst QuicTime::Delta kInitialRtt = QuicTime::Delta::FromMicroseconds(100000);" ${file}
func_comment_segments "DebugState::DebugState" "}" ${file}
sed -i "s/const RttStats\* rtt_stats,/\/\/const RttStats\* rtt_stats,/g" ${file}
sed -i "s/const QuicUnackedPacketMap\* unacked_packets,/\/\/const QuicUnackedPacketMap\* unacked_packets,/g" ${file}
sed -i "s/QuicPacketCount max_congestion_window,/QuicPacketCount max_congestion_window)/g" ${file}
sed -i "s/QuicRandom\* random)/\/\/ QuicRandom* random)/g" ${file}
sed -i "s/rtt_stats->initial_rtt()/kInitialRtt/g" ${file}
sed -i "s/rtt_stats_(rtt_stats),/avg_rtt_(QuicTime::Delta::Zero()),/g" ${file}
line="$(grep -n "avg_rtt_(QuicTime::Delta::Zero())," ${file} | head -n 1 | cut -d: -f1)"
sed -i "${line}i\      min_rtt_(QuicTime::Delta::Zero())," ${file}
sed -i "${line}i\      latest_rtt_(QuicTime::Delta::Zero())," ${file}
sed -i "s/unacked_packets_(unacked_packets),/\/\/ unacked_packets_(unacked_packets),/g" ${file}
sed -i "s/random_(random),/\/\/ random_(random),/g" ${file}
sed -i "s/max_bandwidth_(/\/\/ max_bandwidth_(/g" ${file}
sed -i "s/last_sent_packet_(0),/\/\/ last_sent_packet_(0),/g" ${file}
sed -i "s/current_round_trip_end_(0),/\/\/ current_round_trip_end_(0),/g" ${file}
sed -i "s/round_trip_count_(0),/\/\/ round_trip_count_(0),/g" ${file}
sed -i "s/last_sent_packet_ = packet_number;/\/\/ last_sent_packet_ = packet_number;/g" ${file}
sed -i "s/is_retransmittable != HAS_RETRANSMITTABLE_DATA/false \&\& is_retransmittable != HAS_RETRANSMITTABLE_DATA/g" ${file}
sed -i "s/rtt_stats_->latest_rtt()/latest_rtt_/g" ${file}
sed -i "s/rtt_stats_->min_rtt()/min_rtt_/g" ${file}
sed -i "s/rtt_stats_->smoothed_rtt()/avg_rtt_/g" ${file}
sed -i "s/rtt_stats_->initial_rtt()/kInitialRtt/g" ${file}
sed -i "s/QUIC_BUG_IF(mode_ != STARTING);/assert(mode_ == STARTING);/g" ${file}
func_comment_segments "if (FLAGS_enable_rtt_deviation_based_early_termination)" "}" ${file} #TODO: rtt deviation based early termination
func_comment_segments "sampler_.OnPacketSent" ");" ${file}
sed -i "s/bool rtt_updated,/bool rtt_updated, QuicTime::Delta rtt,/g" ${file}
sed -i "s/UpdateBandwidthSampler(event_time, acked_packets, lost_packets);/\/\/ UpdateBandwidthSampler(event_time, acked_packets, lost_packets);/g" ${file}
line="$(grep -n "QuicTime::Delta avg_rtt = avg_rtt_;" ${file} | head -n 1 | cut -d: -f1)"
sed -i "${line}i\  if (rtt_updated) {" ${file}
line=$((${line} + 1))
sed -i "${line}i\    UpdateRtt(event_time, rtt);" ${file}
line=$((${line} + 1))
sed -i "${line}i\  }" ${file}
sed -i "s/QUIC_BUG_IF(avg_rtt.IsZero());/\/\/ QUIC_BUG_IF(avg_rtt.IsZero());/g" ${file}
func_comment_segments "if (min_rtt_ < rtt_stats_->mean_deviation()) {" "}" ${file}
sed -i "s/void PccSender::OnApplicationLimited/\/\* void PccSender::OnApplicationLimited/g" ${file}
line="$(grep -n "sampler_.OnAppLimited();" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 2))
sed -i "${line}i\*\/" ${file}
sed -i "s/QuicString PccSender::GetDebugState/\/\* QuicString PccSender::GetDebugState/g" ${file}
line="$(grep -n "return stream.str();" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 2))
sed -i "${line}i\*\/" ${file}
sed -i "s/void PccSender::UpdateBandwidthSampler/\/\* void PccSender::UpdateBandwidthSampler/g" ${file}
line="$(grep -n "sampler_.RemoveObsoletePackets" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 2))
sed -i "${line}i\*\/" ${file}
sed -i "s/static QuicString PccSenderModeToString/\/\* static QuicString PccSenderModeToString/g" ${file}
line="$(grep -n "return \"???\";" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 2))
sed -i "${line}i\*\/" ${file}
declare -a useless_func=("::BandwidthEstimate" "::InSlowStart" "::InRecovery"
                         "::ShouldSendProbingPacket" "::GetSlowStartThreshold"
                         "::GetCongestionControlType" "::ExportDebugState"
                         "operator<<"
                        )
for func in "${useless_func[@]}"
do
  func_comment_segments ${func} "}" ${file}
done
sed -i "s/DCHECK_EQ(1u, utility_info.size());/assert(utility_info.size() == 1u);/g" ${file}
sed -i "s/DCHECK_EQ(2 \* kNumIntervalGroupsInProbingLong, utility_info.size());/assert(utility_info.size() == 2 \* kNumIntervalGroupsInProbingLong);/g" ${file}
sed -i "s/DCHECK_NE(STARTING, mode_);/assert(STARTING != mode_);/g" ${file}
sed -i "s/DCHECK_NE(DECISION_MADE, mode_);/assert(DECISION_MADE != mode_);/g" ${file}
func_comment_segments "!BandwidthEstimate().IsZero()" "}" ${file}
sed -i "s/random_->RandUint64()/rand()/g" ${file}
sed -i "s/FALLTHROUGH_INTENDED;/\/\/ FALLTHROUGH_INTENDED;/g" ${file}
sed -i "s/QUIC_BUG <</std::cerr <</g" ${file}
line="$(grep -n "PccSender::OnCongestionEvent" ${file} | head -n 1 | cut -d: -f1)"
sed -i "${line}ivoid PccSender::UpdateRtt(QuicTime event_time, QuicTime::Delta rtt) {" ${file}
line=$((${line} + 1))
sed -i "${line}i\  latest_rtt_ = rtt;" ${file}
line=$((${line} + 1))
sed -i "${line}i\  avg_rtt_ = avg_rtt_.IsZero() ? rtt : avg_rtt_ * 0.875 + rtt * 0.125;" ${file}
line=$((${line} + 1))
sed -i "${line}i\  if (min_rtt_.IsZero() || rtt < min_rtt_) {" ${file}
line=$((${line} + 1))
sed -i "${line}i\    min_rtt_ = rtt;" ${file}
line=$((${line} + 1))
sed -i "${line}i\  }" ${file}
line=$((${line} + 1))
sed -i "${line}i\  std::cerr << (event_time - QuicTime::Zero()).ToMicroseconds() << \" New RTT \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\            << rtt.ToMicroseconds() << std::endl;" ${file}
line=$((${line} + 1))
sed -i "${line}i\}\n" ${file}
line="$(grep -n "interval_queue_.OnPacketSent" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} - 1))
sed -i "${line}i\    std::cerr << (sent_time - QuicTime::Zero()).ToMicroseconds() << \" \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \"Create MI (useful: \" << interval_queue_.current().is_useful" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \") with rate \" << interval_queue_.current().sending_rate" ${file}
line=$((${line} + 1))
sed -i "${line}i\                                                            .ToKBitsPerSecond()" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \", duration \" << monitor_duration_.ToMicroseconds()" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << std::endl;" ${file}
line="$(grep -n "CalculateUtility" ${file} | head -n 1 | cut -d: -f1)"
line=$((${line} + 1))
sed -i "${line}i\    std::cerr << \"End MI (rate: \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << useful_intervals[i]->sending_rate.ToKBitsPerSecond()" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \", rtt \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << useful_intervals[i]->rtt_on_monitor_start.ToMicroseconds()" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \"->\"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << useful_intervals[i]->rtt_on_monitor_end.ToMicroseconds()" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << \", \" << useful_intervals[i]->bytes_acked << \"\/\"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << useful_intervals[i]->bytes_sent << \") with utility \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << CalculateUtility(useful_intervals[i]) << \"(latest \"" ${file}
line=$((${line} + 1))
sed -i "${line}i\              << latest_utility_ << \")\" << std::endl;" ${file}
