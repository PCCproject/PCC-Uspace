#include "RttTracker.h"

namespace {
    uint64_t kInitialRtt = 1000; // 1ms
} // namespace

template <typename IdType>
RttTracker<IdType>::RttTracker(double sample_rate) {
    sample_rate_ = sample_rate;
    wait_until_sample_ = 0;
    latest_rtt_ = kInitialRtt;
}

template <typename IdType>
void RttTracker<IdType>::OnPacketSent(IdType id) {
    wait_until_sample_ -= sample_rate_;
    if (wait_until_sample_ <= 0) {
        wait_until_sample_ = 1.0;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        outgoing_times_.insert(std::make_pair(id, now));
    }
}

template <typename IdType>
void RttTracker<IdType>::OnPacketAck(IdType id) {
    typename std::unordered_map<IdType, struct timespec>::iterator element = outgoing_times_.lookup(id);
    if (element != outgoing_times_.end) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        struct timespec outgoing_time = element->second;
        uint64_t new_rtt = 1000000000 * (now.tv_sec - outgoing_time.tv_sec) + (now.tv_nsec - outgoing_time.tv_nsec);
        latest_rtt_ = new_rtt / 1000;
        outgoing_times_.erase(element);
    }
}

template <typename IdType>
void RttTracker<IdType>::OnPacketLoss(IdType id) {
    typename std::unordered_map<IdType, struct timespec>::iterator element = outgoing_times_.lookup(id);
    if (element != outgoing_times_.end) {
        outgoing_times_.erase(element);
    }
}

template <typename IdType>
uint64_t RttTracker<IdType>::GetLatestRtt() {
    return latest_rtt_;
}
