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

enum PacketState { PACKET_STATE_NONE, PACKET_STATE_QUEUED, PACKET_STATE_SENT, PACKET_STATE_ACKED, PACKET_STATE_LOST };

template<typename SeqNoType, typename IdType>
struct MessageRecord{
    uint64_t rtt_us;
    SeqNoType msg_no;
    IdType packet_id;
    struct timespec sent_time;
};

template<typename SeqNoType, typename IdType>
class PacketRecord {
  public:
    PacketRecord(CPacket& packet, PacketState initial_packet_state);
    ~PacketRecord();
    void UpdateRecord(PacketState new_packet_state, SeqNoType msg_no, IdType packet_id);
    char* GetPacketPayloadPointer() { return payload_pointer_; }
    int32_t GetPacketSize() { return packet_size_; }
    SeqNoType GetPacketMostRecentMsgNo();
    PacketState GetPacketState() { return packet_state_; }
    IdType GetPacketId(SeqNoType seq_no);
    uint64_t GetPacketRtt(SeqNoType msg_no);
    struct timespec GetPacketSentTime(SeqNoType msg_no);
    //void SetPacketId(IdType new_id) { packet_id_ = new_id; }
  private:
    MessageRecord<SeqNoType, IdType>* SafeGetMessageRecord(SeqNoType msg_no);
    
    PacketState packet_state_;
    char* payload_pointer_;
    int32_t packet_size_;
    SeqNoType seq_no_;
    std::vector<MessageRecord<SeqNoType, IdType> > msg_records_;
};

template<typename SeqNoType, typename IdType>
SeqNoType PacketRecord<SeqNoType, IdType>::GetPacketMostRecentMsgNo() { 
    if (msg_records_.empty()) {
        return 0;
    }
    return msg_records_.back().msg_no;
}

template<typename SeqNoType, typename IdType>
uint64_t PacketRecord<SeqNoType, IdType>::GetPacketRtt(SeqNoType msg_no) {
    MessageRecord<SeqNoType, IdType>* msg_record = SafeGetMessageRecord(msg_no);
    return msg_record->rtt_us;
}

template<typename SeqNoType, typename IdType>
IdType PacketRecord<SeqNoType, IdType>::GetPacketId(SeqNoType msg_no) {
    MessageRecord<SeqNoType, IdType>* msg_record = SafeGetMessageRecord(msg_no);
    return msg_record->packet_id;
}


template<typename SeqNoType, typename IdType>
struct timespec PacketRecord<SeqNoType, IdType>::GetPacketSentTime(SeqNoType msg_no) {
    MessageRecord<SeqNoType, IdType>* msg_record = SafeGetMessageRecord(msg_no);
    return msg_record->sent_time;
}

template<typename SeqNoType, typename IdType>
MessageRecord<SeqNoType, IdType>* PacketRecord<SeqNoType, IdType>::SafeGetMessageRecord(SeqNoType msg_no) {
    typename std::vector<MessageRecord<SeqNoType, IdType> >::iterator msg_record_iterator = 
        msg_records_.begin();

    while (msg_record_iterator != msg_records_.end() &&
            msg_record_iterator->msg_no != msg_no) {
        ++msg_record_iterator;
    }

    if (msg_record_iterator == msg_records_.end()) {
        std::cerr << "ERROR: could not find msg no " << msg_no << " for packet " << seq_no_ << std::endl;
        exit(-1);
    }

    return &(*msg_record_iterator);
}

template<typename SeqNoType, typename IdType>
PacketRecord<SeqNoType, IdType>::PacketRecord(CPacket& packet, PacketState initial_packet_state) {
    packet_size_ = packet.getLength();
    seq_no_ = packet.m_iSeqNo;
    payload_pointer_ = new char[packet_size_];
    memcpy(payload_pointer_, packet.m_pcData, packet_size_);
    packet_state_ = initial_packet_state;
}

template<typename SeqNoType, typename IdType>
PacketRecord<SeqNoType, IdType>::~PacketRecord() {
    delete [] payload_pointer_;
}

template<typename SeqNoType, typename IdType>
void PacketRecord<SeqNoType, IdType>::UpdateRecord(PacketState new_packet_state, SeqNoType msg_no, IdType packet_id) {
    if (new_packet_state == PACKET_STATE_SENT) {
        MessageRecord<SeqNoType, IdType> msg_record;
        msg_record.rtt_us = 0;
        clock_gettime(CLOCK_MONOTONIC, &msg_record.sent_time);
        msg_record.msg_no = msg_no;
        msg_record.packet_id = packet_id;
        msg_records_.push_back(msg_record);
    } else if (new_packet_state == PACKET_STATE_ACKED) {
        struct timespec ack_time;
        clock_gettime(CLOCK_MONOTONIC, &ack_time);
        MessageRecord<SeqNoType, IdType>* msg_record = SafeGetMessageRecord(msg_no);
        msg_record->rtt_us = 1000000 * (ack_time.tv_sec - msg_record->sent_time.tv_sec) + (ack_time.tv_nsec - msg_record->sent_time.tv_nsec) / 1000;
    }
    if (msg_no == msg_records_.back().msg_no) {
        packet_state_ = new_packet_state;
    }
}

class TimespecLessThan {
  public:
    int operator() (const struct timespec ts_1, const struct timespec ts_2) {
        if (ts_1.tv_sec != ts_2.tv_sec) {
            return ts_1.tv_sec > ts_2.tv_sec;
        }
        return ts_1.tv_nsec > ts_2.tv_nsec;
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
    void OnPacketAck(SeqNoType seq_no, SeqNoType msg_no);
    void OnPacketLoss(SeqNoType seq_no, SeqNoType msg_no);
    void DeletePacketRecord(SeqNoType seq_no);
    IdType GetPacketId(SeqNoType seq_no, SeqNoType msg_no);
    int32_t GetPacketSize(SeqNoType seq_no);
    PacketState GetPacketState(SeqNoType seq_no);
    SeqNoType GetPacketLastMsgNo(SeqNoType seq_no);
    uint64_t GetPacketRtt(SeqNoType seq_no, SeqNoType msg_no);
    struct timespec GetPacketSentTime(SeqNoType seq_no, SeqNoType msg_no);
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
    //std::cout << "Making new id: " << result << std::endl;
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
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        
        // Packet Id is not currently recorded, so we should record a new ID for it.
        //IdType packet_id = MakeNewPacketId(packet);
        //std::cout << "Enqueued packet: " << packet_id << ":" << seq_no << std::endl;
        packet_record_map_.insert(std::make_pair(seq_no, new PacketRecord<SeqNoType, IdType>(packet, PACKET_STATE_QUEUED)));
        send_queue_.push(seq_no);
    } else {
        std::cerr << "ERROR: Attempted to enqueue packet that already has a record!" << std::endl;
        std::cerr << "\t seq_no = " << seq_no << std::endl;
        exit(-1);
    }
    ++cur_num_packets_;
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketSent(CPacket& packet) {
    SeqNoType seq_no = packet.m_iSeqNo;
    //std::cerr << "Sending packet seq_no = " << seq_no << ", msg_no " << packet.m_iMsgNo << std::endl;
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
        packet_record->UpdateRecord(PACKET_STATE_SENT, packet.m_iMsgNo, MakeNewPacketId(packet));
        sent_queue_.push(packet_record->GetPacketSentTime(packet.m_iMsgNo));
        sent_time_map_.insert(std::make_pair(packet_record->GetPacketSentTime(packet.m_iMsgNo), seq_no)); 
    }
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketAck(SeqNoType seq_no, SeqNoType msg_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter != packet_record_map_.end()) {
        PacketRecord<SeqNoType, IdType>* packet_record = packet_record_iter->second;
        sent_time_map_.erase(packet_record->GetPacketSentTime(msg_no));
        packet_record->UpdateRecord(PACKET_STATE_ACKED, msg_no, 0);
    } else {
        //std::cerr << "ERROR: Packet was acked but never recorded as sent!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
    }
    lock_.unlock();
}

template <typename SeqNoType, typename IdType>
void PacketTracker<SeqNoType, IdType>::OnPacketLoss(SeqNoType seq_no, SeqNoType msg_no) {
    lock_.lock();
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter != packet_record_map_.end()) {
        PacketRecord<SeqNoType, IdType>* packet_record = packet_record_iter->second;
        sent_time_map_.erase(packet_record->GetPacketSentTime(msg_no));
        packet_record->UpdateRecord(PACKET_STATE_LOST, msg_no, 0);
        //packet_record->SetPacketId(0);
        retransmittable_queue_.push(seq_no);
        //std::cout << "Added " << seq_no << " to retransmittable queue" << std::endl;
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
struct timespec PacketTracker<SeqNoType, IdType>::GetPacketSentTime(SeqNoType seq_no, SeqNoType msg_no) {
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
        struct timespec result = packet_record_iter->second->GetPacketSentTime(msg_no);
        return result;
    }
}

template<typename SeqNoType, typename IdType>
IdType PacketTracker<SeqNoType, IdType>::GetPacketId(SeqNoType seq_no, SeqNoType msg_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the id of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        return packet_record_iter->second->GetPacketId(msg_no);
    }
}

template<typename SeqNoType, typename IdType>
uint64_t PacketTracker<SeqNoType, IdType>::GetPacketRtt(SeqNoType seq_no, SeqNoType msg_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the rtt of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        return packet_record_iter->second->GetPacketRtt(msg_no);
    }
}

template<typename SeqNoType, typename IdType>
PacketState PacketTracker<SeqNoType, IdType>::GetPacketState(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the size of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return PACKET_STATE_NONE;
    } else {
        return packet_record_iter->second->GetPacketState();
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
SeqNoType PacketTracker<SeqNoType, IdType>::GetPacketLastMsgNo(SeqNoType seq_no) {
    std::lock_guard<std::mutex> guard(lock_);
    typename std::unordered_map<SeqNoType, PacketRecord<SeqNoType, IdType>*>::iterator packet_record_iter =
        packet_record_map_.find(seq_no);
    if (packet_record_iter == packet_record_map_.end()) {
        //std::cerr << "ERROR: Attempted to get the size of an unrecorded packet!" << std::endl;
        //std::cerr << "\t seq_no = " << seq_no << std::endl;
        //exit(-1);
        return 0;
    } else {
        return packet_record_iter->second->GetPacketMostRecentMsgNo();
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
