#ifndef _CHANNEL_SIMULATOR_H_
#define _CHANNEL_SIMULATOR_H_

//#include "sender.h"
//#include "PccSender/allegro_sender.h"
//#include "PccSender/vivace_sender.h"
//#include "PccSender/rinforzando_sender.h"
#include "../src/pcc/pcc_sender.h"

#include <stdio.h>
#include <map>
#include <vector>
#include <string>

using namespace std;

// #define DEBUG_

struct PacketInfo {
  PacketInfo();
  PacketInfo(int sender_id, long long seq, double event_time);
  ~PacketInfo() {}

  int sender_id;
  long long seq_no;
  double sent_timestamp;
};

enum EventType {
  SEND,
  ACKED,
  LOST,
  LINK_CHANGE
};

struct LinkChangeEventData {
    double bw, bw_range;
    double dl, dl_range;
    double bf, bf_range;
    double plr, plr_range;
    bool reset_queue;
    double change_interval;
};

struct EventInfo {
  EventInfo() {}
  EventInfo(EventType event_type, double event_time, int sender_id, long long seq, double rtt);
  ~EventInfo() {}

  EventType event_type;
  int sender_id;
  long long seq_no;
  double rtt;
  double time;
  void* data;
};

struct EventInfoComparison {  
   bool operator()(const EventInfo& l, const EventInfo& r) {  
       return l.time > r.time;  
   }
};  

class Simulator {
 public:
  Simulator(FILE* sender_config,
            double bw,
            double dl,
            double bf,
            double plr,
            int duration);
  ~Simulator();

  void EnqueueEvent(EventInfo& event);

  void EnqueueSendEvent(double event_time, int sender_id);
  void EnqueueLossEvent(double event_time,
                    int sender_id,
                    long long seq);
  void EnqueueAckEvent(double event_time,
                    int sender_id,
                    long long seq,
                    double rtt);
  void Run();
  void Analyze();

 private:
  void OnPacketEnqueue(double event_time, int sender_id);
  double CalculateAckTime(double send_time);
  double GetCurrentQueueDelay(double cur_time);
  void UpdateQueueDelayOnSend(double event_time);

  void ChangeLink(double bw, double dl, double bf, double plr, bool reset_queue);
  void ProcessLinkChangeEvent(EventInfo& event_info);

  double bandwidth_; // in Mbps
  double base_rtt_; // in seconds
  double plr_; // probabilistic loss rate
  double duration_; // in seconds

  double last_event_time_;

  priority_queue<double, vector<EventInfo>, EventInfoComparison> event_queue_;
  vector<PccSender*> senders_;
  vector<int> seq_numbers_;

  double full_queue_delay_;
  double cur_queue_delay_;
  double last_queue_update_time_;

  int packet_size_; // in bytes

#ifdef DEBUG_
  FILE* flog_;
#endif
};

#endif
