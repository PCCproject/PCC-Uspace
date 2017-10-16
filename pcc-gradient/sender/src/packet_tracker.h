#ifndef PACKET_TRACKER_H_
#define PACKET_TRACKER_H_

#include <pthread.h>
#include <iostream>
#include <unordered_map>
#include <time.h>
#include <string.h>
#include <mutex>

namespace {
    int kArbitraryPacketLimit = 100000;
} // namespace

enum PacketState { PACKET_STATE_QUEUED, PACKET_STATE_SENT, PACKET_STATE_ACKED, PACKET_STATE_LOST };

template<typename SeqNoType, typename IdType>
class PacketRecord {
  public:
    PacketRecord(CPacket& packet, IdType packet_id, PacketState initial_packet_state);
    ~PacketRecord();
    void UpdateRecord(PacketState new_packet_state);
    char* GetPacketPayloadPointer() { return payload_pointer_; }
    int32_t GetPacketSize() { return packet_size_; }
    int32_t GetPacketState() { return packet_state_; }
    IdType GetPacketId() { return packet_id_; }
    uint64_t GetPacketRtt() { return rtt_us_; }
    struct timespec GetPacketSentTime() { return sent_time_; }
    void SetPacketId(IdType new_id) { packet_id_ = new_id; }
  private:
    PacketState packet_state_;
    char* payload_pointer_;
    int32_t packet_size_;
    IdType packet_id_;
    SeqNoType seq_no_;
    uint64_t rtt_us_;
    struct timespec sent_time_;
};

template<typename SeqNoType, typename IdType>
PacketRecord<SeqNoType, IdType>::PacketRecord(CPacket& packet, IdType packet_id, PacketState initial_packet_state) {
    packet_size_ = packet.getLength();
    seq_no_ = packet.m_iSeqNo;
    packet_id_ = packet_id;
    payload_pointer_ = new char[packet_size_];
    memcpy(payload_pointer_, packet.m_pcData, packet_size_);
    packet_state_ = initial_packet_state;
    rtt_us_ = 0;
}

template<typename SeqNoType, typename IdType>
PacketRecord<SeqNoType, IdType>::~PacketRecord() {
    delete [] payload_pointer_;
}

template<typename SeqNoType, typename IdType>
void PacketRecord<SeqNoType, IdType>::UpdateRecord(PacketState new_packet_state) {
    if (new_packet_state == PACKET_STATE_SENT) {
        clock_gettime(CLOCK_MONOTONIC, &sent_time_);
    } else if (new_packet_state == PACKET_STATE_ACKED) {
        struct timespec ack_time;
        clock_gettime(CLOCK_MONOTONIC, &ack_time);
        rtt_us_ = 1000000 * (ack_time.tv_sec - sent_time_.tv_sec) + (ack_time.tv_nsec - sent_time_.tv_nsec) / 1000;
    }
    packet_state_ = new_packet_state;
}

class TimespecLessThan {
  public:
    int operator() (const struct timespec ts_1, const struct timespec ts_2) {
        if (ts_1.tv_sec != ts_2.tv_sec) {
            return ts_1.tv_sec < ts_2.tv_sec;
        }
        return ts_1.tv_nsec < ts_2.tv_nsec;
    }
};

template<typename SeqNoType>
class TrackerLessThan {
  public:
    int operator() (const SeqNoType seq_no_1, const SeqNoType seq_no_2) {
        return seq_no_2 < seq_no_1;
    }
};

namespace std {
    template <> struct hash<struct timespec> {
        size_t operator()(const struct timespec& ts) const {
            return hash<int>()(ts.tv_nsec);
        }
    };
    int operator==(const struct timespec& ts1, const struct timespec& ts2);
} // namespace std

template<typename SeqNoType, typename IdType>
class PacketTracker {
  public:
    PacketTracker(pthread_cond_t* send_cond);
    ~PacketTracker();
    bool CanEnqueuePacket();
    void EnqueuePacket(CPacket& packet);
    void OnPacketSent(CPacket& packet);
    void OnPacketAck(SeqNoType seq_no);
    void OnPacketLoss(SeqNoType seq_no);
    void DeletePacketRecord(SeqNoType seq_no);
    IdType GetPacketId(SeqNoType seq_no);
    int32_t GetPacketSize(SeqNoType seq_no);
    uint64_t GetPacketRtt(SeqNoType seq_no);
    struct timespec GetPacketSentTime(SeqNoType seq_no);
    bool HasSentPackets();
    bool HasRetransmittablePackets();
    bool HasSendablePackets();
    SeqNoType GetLowestSendableSeqNo();
    SeqNoType GetLowestRetransmittableSeqNo();
    SeqNoType GetOldestSentSeqNo();
    char* GetPacketPayloadPointer(SeqNoType seq_no);
  private:
    IdType MakeNewPacketId(CPacket& packet);
    std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*> packet_record_map_;
    std::priority_queue<SeqNoType, std::vector<SeqNoType>, TrackerLessThan<SeqNoType> > retransmittable_queue_;
    std::priority_queue<SeqNoType, std::vector<SeqNoType>, TrackerLessThan<SeqNoType> > send_queue_;
    std::priority_queue<struct timespec, std::vector<struct timespec>, TimespecLessThan> sent_queue_;
    std::unordered_map<struct timespec, SeqNoType> sent_time_map_;
    IdType prev_packet_id_;
    int cur_num_packets_;
    int arbitrary_packet_limit_;
    std::mutex lock_;
    pthread_cond_t* send_cond_;
};

template <typename SeqNoType, typename IdType>
PacketTracker<SeqNoType, IdType>::~PacketTracker() {
}

template <typename SeqNoType, typename IdType>
PacketTracker<SeqNoType, IdType>::PacketTracker(pthread_cond_t* send_cond) {
    prev_packet_id_ = 0;
    arbitrary_packet_limit_ = kArbitraryPacketLimit;
    send_cond_ = send_cond;
    cur_num_packets_ = 0;
}

template <typename SeqNoType, typename IdType>
IdType PacketTracker<SeqNoType, IdType>::MakeNewPacketId(CPacket& packet) {
    IdType result = ++prev_packet_id_;
    return result;
}

template <typename SeqNoType, typename IdType>
bool PacketTracker<SeqNoType, IdType>::CanEnqueuePacket() {
    return cur_num_packets_ < arbitrary_packet_limit_;
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::EnqueuePacket(CPacket& packet) {
    SeqNoType seq_no = packet.m_iSeqNo;
    //std::cout << "Enqueueing packet: " << seq_no << std::endl;
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        
        // Packet Id is not currently recorded, so we should record a new ID for it.
        IdType packet_id = MakeNewPacketId(packet);
        packet_record_map_.insert(std::make_pair(seq_no, new PacketRecord<SeqNoType, IdType>(packet, packet_id, PACKET_STATE_QUEUED)));
        send_queue_.push(seq_no);
    } else {
        std::cerr << "ERROR: Attempted to enqueue packet that already has a record!" << std::endl;
        std::cerr << "\t seq_no = " << seq_no << std::endl;
        exit(-1);
    }
    ++cur_num_packets_;
    lock_.unlock();
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketSent(CPacket& packet) {
    SeqNoType seq_no = packet.m_iSeqNo;
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        std::cerr << "ERROR: Attempted to send packet not already queued!" << std::endl;
        std::cerr << "\t seq_no = " << seq_no << std::endl;
        exit(-1);
    } else {
        if (packet_record_iter->second->GetPacketState() == PACKET_STATE_QUEUED) {
            SeqNoType sendable_seq_no = send_queue_.top();
            if (seq_no != sendable_seq_no) {
                std::cerr << "ERROR: Attempted to send out of order!" << std::endl;
                std::cerr << "\t seq_no = " << seq_no << std::endl;
                std::cerr << "\t sendable_seq_no = " << sendable_seq_no << std::endl;
                exit(-1);
            } else {
                send_queue_.pop();
            }
        } else {
            SeqNoType retransmittable_seq_no = retransmittable_queue_.top();
            if (seq_no != retransmittable_seq_no) {
                std::cerr << "ERROR: Attempted to retransmit out of order!" << std::endl;
                std::cerr << "\t seq_no = " << seq_no << std::endl;
                std::cerr << "\t retransmittable_seq_no = " << retransmittable_seq_no << std::endl;
                exit(-1);
            } else {
                retransmittable_queue_.pop();
            }
        }
        PacketRecord<SeqNoType, IdType>* packet_record = packet_record_iter->second;
        packet_record->UpdateRecord(PACKET_STATE_SENT);
        packet_record->SetPacketId(MakeNewPacketId(packet));
        sent_queue_.push(packet_record->GetPacketSentTime());
        sent_time_map_.insert(std::make_pair(packet_record->GetPacketSentTime(), seq_no)); 
    }
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketAck(SeqNoType seq_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter != packet_record_map_.end()) {
        PacketRecord<SeqNoType, IdType>* packet_record = packet_record_iter->second;
        sent_time_map_.erase(packet_record->GetPacketSentTime());
        packet_record->UpdateRecord(PACKET_STATE_ACKED);
    } else {
        //std::cerr << "ERROR: Packet was acked but never recorded as sent!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
    }
    lock_.unlock();
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketLoss(SeqNoType seq_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter != packet_record_map_.end()) {
        PacketRecord<SeqNoType, IdType>* packet_record = packet_record_iter->second;
        sent_time_map_.erase(packet_record->GetPacketSentTime());
        packet_record->UpdateRecord(PACKET_STATE_LOST);
        packet_record->SetPacketId(0);
        retransmittable_queue_.push(seq_no);
    } else {
        //std::cerr << "ERROR: Packet was lost but never recorded as sent!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
    }
    lock_.unlock();
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::DeletePacketRecord(SeqNoType seq_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    //std::cout << "Deleting packet record: " << std::endl;
    //std::cout << "\t seq_no = " << seq_no << std::endl;
    if (packet_record_iter == packet_record_map_.end()) {
        std::cerr << "ERROR: Attempted to delete unrecorded packet!" << std::endl;
        std::cerr << "\t seq_no = " << seq_no << std::endl;
        exit(-1);
    } else {
        delete packet_record_iter->second;
        packet_record_map_.erase(packet_record_iter);
    }
    --cur_num_packets_;
    if (cur_num_packets_ < arbitrary_packet_limit_) {
        pthread_cond_signal(send_cond_);
    }
    //std::cout << "\t num packets = " << cur_num_packets_ << std::endl;
    lock_.unlock();
}

template<typename SeqNoType, typename IdType>
struct timespec PacketTracker<SeqNoType, IdType>::GetPacketSentTime(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 0;
        return ts;
        //std::cerr << "ERROR: Attempted to get the sending time of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
    } else {
        struct timespec result = packet_record_iter->second->GetPacketSentTime();
        return result;
    }
}

template<typename SeqNoType, typename IdType>
IdType PacketTracker<SeqNoType, IdType>::GetPacketId(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the id of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        IdType result = packet_record_iter->second->GetPacketId();
        return result;
    }
}

template<typename SeqNoType, typename IdType>
uint64_t PacketTracker<SeqNoType, IdType>::GetPacketRtt(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the rtt of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        return packet_record_iter->second->GetPacketRtt();
    }
}

template<typename SeqNoType, typename IdType>
int32_t PacketTracker<SeqNoType, IdType>::GetPacketSize(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the size of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        return packet_record_iter->second->GetPacketSize();
    }
}

template<typename SeqNoType, typename IdType>
bool PacketTracker<SeqNoType, IdType>::HasSentPackets() {
    return !sent_queue_.empty();
}

template<typename SeqNoType, typename IdType>
SeqNoType PacketTracker<SeqNoType, IdType>::GetOldestSentSeqNo() {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<struct timespec, SeqNoType>::iterator sent_iterator =
        sent_time_map_.find(sent_queue_.top());
    while (!sent_queue_.empty() && sent_iterator == sent_time_map_.end()) {
        sent_queue_.pop();
        if (!sent_queue_.empty()) {
            sent_iterator = sent_time_map_.find(sent_queue_.top());
        }
    }
    if (sent_queue_.empty()) {
        return 0;
    }
    if (sent_iterator == sent_time_map_.end()) {
        std::cout << "COULD NOT FIND IN SENT MAP" << std::endl;
    }
    return sent_iterator->second;
}

template<typename SeqNoType, typename IdType>
bool PacketTracker<SeqNoType, IdType>::HasSendablePackets() {
    std::lock_guard<std::mutex> guard(lock_);
    while (!send_queue_.empty() && packet_record_map_.find(send_queue_.top()) ==
        packet_record_map_.end()) {
        
        send_queue_.pop();
    }
    return !send_queue_.empty();
}

template<typename SeqNoType, typename IdType>
SeqNoType PacketTracker<SeqNoType, IdType>::GetLowestSendableSeqNo() {
    std::lock_guard<std::mutex> guard(lock_);
    while (!send_queue_.empty() && packet_record_map_.find(send_queue_.top()) ==
        packet_record_map_.end()) {
        
        send_queue_.pop();
    }
    if (send_queue_.empty()) {
        return 0;
    }
    return send_queue_.top();
}

template<typename SeqNoType, typename IdType>
bool PacketTracker<SeqNoType, IdType>::HasRetransmittablePackets() {
    std::lock_guard<std::mutex> guard(lock_);
    while (!retransmittable_queue_.empty() && packet_record_map_.find(retransmittable_queue_.top()) ==
        packet_record_map_.end()) {
        
        retransmittable_queue_.pop();
    }
    return !retransmittable_queue_.empty();
}

template<typename SeqNoType, typename IdType>
SeqNoType PacketTracker<SeqNoType, IdType>::GetLowestRetransmittableSeqNo() {
    std::lock_guard<std::mutex> guard(lock_);
    while (!retransmittable_queue_.empty() && packet_record_map_.find(retransmittable_queue_.top()) ==
        packet_record_map_.end()) {
        
        retransmittable_queue_.pop();
    }
    if (retransmittable_queue_.empty()) {
        return 0;
    }
    return retransmittable_queue_.top();
}

template<typename SeqNoType, typename IdType>
char* PacketTracker<SeqNoType, IdType>::GetPacketPayloadPointer(SeqNoType seq_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        std::cerr << "ERROR: Attempted to get the payload of an unrecorded packet!" << std::endl;
        std::cerr << "\t seq_no = " << seq_no << std::endl;
        exit(-1);
    } else {
        char* result = packet_record_iter->second->GetPacketPayloadPointer();
        lock_.unlock();
        return result;
    }
}


#endif // PACKET_TRACKER_H_
