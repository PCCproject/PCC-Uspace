#include "channel_simulator.h"

#include <unistd.h>

const int kPacketSize = 1500 * 8;
const double kTimeDelta = 0.000000001;

static double rand0_1() {
    return (double)rand() / (double)RAND_MAX;
}

PacketInfo::PacketInfo() : sender_id(0), seq_no(0), sent_timestamp(0.0) {}

PacketInfo::PacketInfo(int sender_id, long long seq, double event_time) :
    sender_id(sender_id), seq_no(seq), sent_timestamp(event_time) {}

EventInfo::EventInfo(EventType event_type, double event_time, int sender_id, long long seq, double rtt) :
    event_type(event_type), time(event_time), sender_id(sender_id), seq_no(seq), rtt(rtt) {}

Simulator::Simulator(FILE* sender_config,
                     double bw,
                     double dl,
                     double bf,
                     double plr,
                     int duration) : bandwidth_(bw),
                                     base_rtt_(dl),
                                     full_queue_delay_((bf - 1) * 1500 / (bw * 1024 * 1024)),
                                     duration_(duration),
                                     plr_(plr),
                                     last_event_time_(0.0) {
#ifdef DEBUG_
  flog_ = fopen("log.txt", "w");
#endif

  packet_size_ = 1500;
  cur_queue_delay_ = 0;
  last_queue_update_time_ = 0;

#ifdef DEBUG_
  fprintf(flog_, "Simulation Initialized: recv interval %.10lf sec\n",
      recv_interval_);
  fflush(flog_);
#endif

  double start_time = -1.0;
  
  int sender_id = 0;
  char sender_type[15];
  char utility_tag[15];
  char tmp;
  while(fscanf(sender_config, "%s %lf", sender_type, &start_time) != EOF) {
    printf("%s Sender %d: start at %4.0lf sec\n",
        sender_type, sender_id, start_time);
    fscanf(sender_config, "%c", &tmp);
    if (tmp == ' ') {
      fscanf(sender_config, "%s\n", utility_tag);
      printf("  (Using %s utility function)\n", utility_tag);
    } else {
      utility_tag[0] = '\0';
    }

    EnqueueSendEvent(start_time, sender_id);
    if (std::string(sender_type).compare("PCC") == 0) {
      senders_.push_back(new PccSender(10000, 10, 0));
      seq_numbers_.push_back(0);
    }

    sender_id += 1;
  }
}

Simulator::~Simulator() {
  for (int i = 0; i < senders_.size(); ++i) {
    delete senders_[i];
  }
  senders_.clear();
}

void Simulator::ChangeLink(double bw, double dl, double bf, double plr, bool reset_queue) {
    bandwidth_ = bw;
    base_rtt_ = dl;
    full_queue_delay_ = (bf - 1) * 1500 / (bw * 1024 * 1024);
    if (reset_queue) {
        cur_queue_delay_ = 0;
    }
    plr_ = plr;
   // std::cout << "bw = " << bw << ", base_rtt_ = " << base_rtt_ << ", full_queue_delay_" << full_queue_delay_ << std::endl;
}

void Simulator::ProcessLinkChangeEvent(EventInfo& event_info) {
    //std::cout << "Processing link change event" << std::endl;
    LinkChangeEventData* data = (LinkChangeEventData*)event_info.data;
    double new_bw = data->bw + (rand0_1() - 0.5) * (data->bw_range);
    double new_dl = data->dl + (rand0_1() - 0.5) * (data->dl_range);
    double new_bf = data->bf + (rand0_1() - 0.5) * (data->bf_range);
    double new_plr = data->plr + (rand0_1() - 0.5) * (data->plr_range);
    //std::cout << "bw = " << new_bw << ", bw_range = " << data->bw_range << std::endl;
    if (new_bw < 0) {
        new_bw = 0;
    }
    if (new_dl < 0) {
        new_dl = 0;
    }
    if (new_bf < 1) {
        new_bf = 1;
    }
    if (new_plr < 0) {
        new_plr = 0;
    }
    ChangeLink(new_bw, new_dl, new_bf, new_plr, data->reset_queue);
    event_info.time += data->change_interval;
    event_queue_.push(event_info);
}

void Simulator::EnqueueEvent(EventInfo& event) {
  event_queue_.push(event);
}

void Simulator::EnqueueSendEvent(double event_time, int sender_id) {
  event_queue_.push(EventInfo(SEND, event_time, sender_id, 0, 0.0));
}

void Simulator::EnqueueLossEvent(double event_time,
                             int sender_id,
                             long long seq) {
  event_queue_.push(EventInfo(LOST, event_time, sender_id, seq, 0.0));
}

void Simulator::EnqueueAckEvent(double event_time,
                             int sender_id,
                             long long seq,
                             double rtt) {
  event_queue_.push(EventInfo(ACKED, event_time, sender_id, seq, rtt));
}

void Simulator::Run() {
  EventType event_type;
  int sender_id;
  long long seq;
  double rtt;
  double last_log_time = 0;
  printf("Simulation Progress:\n");
  while(last_event_time_ < duration_) {
    //std::cout << " --- STEP --- " << std::endl;
    if (last_event_time_ - last_log_time > 50) {
      printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
      printf("====> %5.0lf sec (%5.1lf%%)",
          last_event_time_, 100.0 * last_event_time_ / duration_);
      fflush(stdout);
      last_log_time = last_event_time_;
    }

    //std::cout << "Getting first event" << std::endl;
    EventInfo event = event_queue_.top();
    event_queue_.pop();
    last_event_time_ = event.time;
    event_type = event.event_type;
    sender_id = event.sender_id;
    seq = event.seq_no;

    rtt = event.rtt;

    if (event_type == LINK_CHANGE) {
      ProcessLinkChangeEvent(event);
    } else if (event_type == SEND) {
      std::cout << "Event: SEND" << std::endl;
      OnPacketEnqueue(last_event_time_, sender_id);
      //std::cout << "Finished send event" << std::endl;
    } else if (event_type == ACKED) {
      std::cout << "Event: ACK " << seq << std::endl;
      CongestionEvent ce;
      ce.packet_number = seq;
      ce.bytes_acked = 1500;
      ce.bytes_lost = 0;
      ce.time = last_event_time_;
      AckedPacketVector acks;
      acks.push_back(ce);
      LostPacketVector lost;
      senders_[sender_id]->OnCongestionEvent(true, 0, last_event_time_ * 1000000.0, rtt * 1000000.0, acks, lost);
    } else {
      std::cout << "Event: LOSS " << seq << std::endl;
      CongestionEvent ce;
      ce.packet_number = seq;
      ce.bytes_acked = 0;
      ce.bytes_lost = 1500;
      ce.time = last_event_time_;
      AckedPacketVector acks;
      LostPacketVector lost;
      lost.push_back(ce);
      senders_[sender_id]->OnCongestionEvent(true, 0, last_event_time_ * 1000000.0, rtt * 1000000.0, acks, lost);
    }
  }
  printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
  printf("==== (DONE) %5.0lf sec (100.0%%)", duration_);
  fflush(stdout);
  usleep(500000);
  printf("\n");

#ifdef DEBUG_
  fclose(flog_);
#endif
}

void Simulator::Analyze() {
  int sender_id = 0;
  /*
  for(vector<PccSender*>::iterator itr = senders_.begin(); itr != senders_.end();
      ++ itr) {
    double th = 1.0 * kPacketSize * (*itr)->get_packets_acked() / kMbPerBit /
        (*itr)->get_sender_duration();
    double plr = 1.0 * (*itr)->get_packets_lost() /
        ((*itr)->get_packets_lost() + (*itr)->get_packets_acked());
    printf("Sender %d: throughput %.4lf, loss %.4lf, ", sender_id, th, plr);
    printf("avg rtt %.10lf, rtt dev %.10lf\n",
        (*itr)->get_avg_rtt(), (*itr)->get_dev_rtt());

    sender_id += 1;
  }
  */
}

void Simulator::OnPacketEnqueue(double event_time, int sender_id) {
  long long seq_no = ++seq_numbers_[sender_id];

  double queue_delay = GetCurrentQueueDelay(event_time);
  if (queue_delay > full_queue_delay_) {
    EnqueueLossEvent(event_time + 1.1 * full_queue_delay_,
        sender_id, seq_no);
  } else {
    UpdateQueueDelayOnSend(event_time);
    double ack_time = CalculateAckTime(event_time);
    EnqueueAckEvent(ack_time, sender_id, seq_no, ack_time - event_time);
  }
  senders_[sender_id]->OnPacketSent(event_time * 1000000.0, 0, seq_no, packet_size_, false);

  double next_sent_time =
      event_time + kPacketSize / senders_[sender_id]->PacingRate(0);
#ifdef DEBUG_
    fprintf(flog_, "  next sent time %.10lf\n", next_sent_time);
    fflush(flog_);
#endif
  EnqueueSendEvent(next_sent_time, sender_id);
}

double Simulator::GetCurrentQueueDelay(double cur_time) {
  double result = cur_queue_delay_ - (cur_time - last_queue_update_time_);
  if (result < 0) {
    result = 0;
  }
  return result;
}

void Simulator::UpdateQueueDelayOnSend(double event_time) {
  cur_queue_delay_ -= (event_time - last_queue_update_time_);
  if (cur_queue_delay_ < 0) {
    cur_queue_delay_ = 0;
  }
  cur_queue_delay_ += packet_size_ * 8.0 / (bandwidth_ * 1024 * 1024);
  last_queue_update_time_ = event_time;
}

double Simulator::CalculateAckTime(double cur_time) {
    return cur_time + base_rtt_ + (rand0_1() - 0.5) * (base_rtt_ * 0.00) + GetCurrentQueueDelay(cur_time);
}
