#ifndef __PCC_H__
#define __PCC_H__

#define _USE_MATH_DEFINES

#include <udt.h>
#include <ccc.h>
#include<iostream>
#include<cmath>
#include <deque>
#include <time.h>
#include <map>
#include <memory>
#include <deque>
#include <mutex>
#include <thread>
#include <stdlib.h>
#define DEBUG
#define MAX_MONITOR 500
using namespace std;

bool kInTimeout = false;

struct GuessStat {
    int monitor;
    double rate;
    long double utility;
    bool ready;
    bool isup;
    double loss_rate;
    double loss;
    double total;
    double latency_info;
    double time;
    double rtt;
};


struct MoveStat {
    double reference_rate;
    double target_rate;
    double change;
    long double reference_utility;
    long double target_utility;
    int reference_monitor;
    int target_monitor;
    bool reference_ready;
    bool target_ready;
    double reference_loss_rate;
    double target_loss_rate;
    double target_loss_pkt;
    double reference_loss_pkt;
    double target_total_pkt;
    double reference_total_pkt;
    double target_latency_info;
    double reference_latency_info;
    double target_time;
    double reference_time;
    double target_rtt;
    double reference_rtt;
};

struct RecentEndMonitorStat {
    int monitor;
    double utility;
    double loss;
    double rtt;
    double total;
    double rate;
    bool initialized;
};

class PCC : public CCC {
  public:
    PCC() : base_rate_(0.6), kPrint(false), state_(START),
        monitor_in_start_phase_(-1), slow_start_factor_(2), number_of_probes_(4),
        guess_time_(0),
        alpha_(kAlpha), beta_(kBeta), exponent_(kExponent),
        factor_(kFactor), step_(kStep), rate_(0.8),
        utility_sum_(0), measurement_intervals_(0),
        last_utility_(-100000) {
        amplifier = 0;
        boundary_amplifier = 0;
	sum_total = 0;
        sum_loss = 0;
        probe_amplifier = 0;
        m_dPktSndPeriod = 10000;
        m_dCWndSize = 100000.0;
        prev_change_ = 0;
        avg_rtt = 0;
        hibernate_depth = 0;
        timeout_immune_monitor = -1;
        deviation_immune_monitor = -1;
        trend_count_ = 0;
        curr_ = 0;
	loss_control_amplifier = 1;
        swing_buffer = 0;
        recent_end_stat.initialized = false;
        setRTO(100000000);
        recent_end_stat.initialized = false;
        srand(time(NULL));
        avg_loss = 0;
        move_stat.reference_monitor = -1;
        move_stat.target_monitor = -1;
        move_stat.reference_ready = false;
        move_stat.target_ready = false;
        double_check = 1;
        loss_ignore_count = 50;
        last_stop_monitor = -1;
        cerr << "new Code!!!" << endl;
        cerr << "configuration: alpha = " << alpha_ << ", beta = " << beta_   <<
             ", exponent = " << exponent_ <<
             ", factor = " << factor_ << ", step = " << step_ << endl;

        /*
        if (!latency_mode) {
        	beta_ = 0;
        } else {
        	beta_ = 50;
        }
        */
    }

    ~PCC() {}
    double getkDelta() {
        if(0.5/base_rate_ >0.05) {
           return 0.5/base_rate_;
        }
        return 0.05;
        return 0.05 * (1 + probe_amplifier);
    }

    long double avg_utility() {
        if (measurement_intervals_ > 0) {
            return utility_sum_ / measurement_intervals_;
        }
        return 0;
    }

    virtual void onLoss(const int32_t*, const int&) {}
    virtual bool onTimeout(int total, int loss, double in_time, int current,
                           int endMonitor, double rtt) {
#ifdef DEBUG
        cerr<<"Timeout happens to monitor "<<endMonitor<<endl;
#endif
        //if(!(deviation_immune_monitor != -1 && deviation_immune_monitor != endMonitor)) {
        deviation_immune_monitor = -1;
        //}
        if (endMonitor == timeout_immune_monitor) {
            timeout_immune_monitor = -1;
            return true;
        }

        if (endMonitor < timeout_immune_monitor) {
            return true;
        }

        double factor = double(total-loss)/total;
        factor = 0.5;
        recent_end_stat.initialized = false;
        base_rate_ = base_rate_ * factor;
        if (base_rate_ < kMinRateMbps + 0.25) {
            state_ = HIBERNATE;
            double divider = 1;
            if(hibernate_depth > 3) {
                hibernate_depth = 3;
            }
            for(int i=0; i<hibernate_depth; i++) {
                divider *= 2;
            }
#ifdef DEBUG
            cerr<<"Diverder is "<<divider<<endl;
#endif
            setRate(kHibernateRate/divider, true);
            guess_measurement_bucket.clear();
            hibernate_depth++;
            return false;
        } else {
            guess_measurement_bucket.clear();
            state_ = SEARCH;
            setRate(base_rate_);
            return false;
        }
    }
    virtual void onACK(const int& ack) {}

    virtual void onMonitorStart(int current_monitor, int& suggested_length,
                                int& mss, double& length_amplifier) {
        ConnectionState old_state;
#ifdef DEBUG
        cerr<<"Monitor "<<current_monitor<<" starts"<<endl;
#endif
        length_amplifier = loss_control_amplifier;
        do {
            old_state = state_;
            switch (state_) {
            case START:
                if (monitor_in_start_phase_ != -1) {
                    return;
                }
                monitor_in_start_phase_ = current_monitor;
                base_rate_ = rate()* slow_start_factor_;
                setRate(base_rate_);
#ifdef DEBUG
                cerr<<"slow starting of monitor"<<current_monitor<<endl;
#endif
                break;
            case SEARCH:
                if(base_rate_ < kMinRateMbps)
                   base_rate_ = kMinRateMbps;
 
#ifdef DEBUG
                cerr<<"Monitor "<<current_monitor<<"is in search state"<<endl;
#endif
                state_ = RECORDING;
                search(current_monitor);
                guess_time_ = 0;
                break;
            case RECORDING:
                //m_iMSS = 1000 + current_monitor%MAX_MONITOR;
                //mss=1000 + current_monitor%MAX_MONITOR;
                m_iMSS = 1500;
                mss = 1500;
                if(guess_time_ != number_of_probes_) {
#ifdef DEBUG
                    cerr<<"Monitor "<<current_monitor<<"is in recording state "<<guess_time_<<"th trial with rate of"<<guess_measurement_bucket[guess_time_].rate<<endl;
#endif
                    setRate(guess_measurement_bucket[guess_time_].rate);
                    guess_time_ ++;
                } else {
#ifdef DEBUG
                    cerr<<"Monitor "<<current_monitor<<"is in recording state, waiting result for recording to come back"<<endl;
#endif
                    setRate(base_rate_);
                    move_stat.reference_monitor = current_monitor;
                    move_stat.reference_rate = base_rate_;
                }
                break;
            case MOVING:
                // TODO: should handle how we move and how we record utility as well
#ifdef DEBUG
                cerr<<"monitor "<<current_monitor<<"is in moving state setting rate to"<<move_stat.target_rate<<endl;
#endif
                setRate(move_stat.target_rate);
                base_rate_ = move_stat.target_rate;
                if(move_stat.target_monitor == -1) {
                move_stat.target_monitor = current_monitor;
                }
                if (base_rate_ < kMinRateMbps) {
#ifdef DEBUG
                    cerr<<"trying to set rate below min rate in moving phase just decided, enter guessing"<<endl;
#endif
                    base_rate_ = kMinRateMbps;
                    setRate(base_rate_);
                    amplifier = 0;
                    boundary_amplifier = 0;
                    state_ = SEARCH;
                    guess_measurement_bucket.clear();
                    move_stat.target_monitor = -1;
                    break;
                }

                break;
            case HIBERNATE:
#ifdef DEBUG
                cerr<<"Hibernating, setting it to really low rate"<<endl;
#endif
                m_iMSS = 100;
                mss = 100;
                double divider = 1;
                if(hibernate_depth > 3) {
                    hibernate_depth = 3;
                }
                for(int i=0; i<hibernate_depth; i++) {
                    divider *= 2;
                }
                setRate(kHibernateRate/divider, true);
                suggested_length = 1;
                break;
            }

        } while(old_state != state_);
    }

    virtual void onMonitorEnds(int total, int loss, double in_time, int current,
                               int endMonitor, double rtt, double latency_info) {
        rtt /= (1000 * 1000);
        if (rtt == 0) rtt = 0.0001;
        if (avg_rtt ==0) {
            avg_rtt = 0.04;
        } else {
            avg_rtt = avg_rtt *0.8 + rtt*0.2;
        }
        if (endMonitor >= timeout_immune_monitor) {
            timeout_immune_monitor = -1;
        }
        
        if(state_ == START) {
          if(loss_ignore_count >= loss) {
              if (latency_info > -0.2 && latency_info < 0.2) {
                latency_info = 0.0;
              }
              loss = 0;
              loss_ignore_count -= loss;
          } else {
              if (loss_ignore_count > 0) {
                if (latency_info > -0.2 && latency_info < 0.2) {
                  latency_info = 0.0;
                }
              }
              loss -= loss_ignore_count;
              loss_ignore_count = 0;
          }
        }
      
        double loss_rate = loss/double(total);
        //if(loss_rate > 0) {
        //   loss_rate = ceil(loss_rate * 100)/100.0;
        //}
        long double curr_utility = utility(total, loss, in_time, rtt,
                                           latency_info);
        if(endMonitor == move_stat.reference_monitor) {
            move_stat.reference_utility = curr_utility;
            move_stat.reference_ready = true;
            move_stat.reference_loss_rate = loss_rate;
	    move_stat.reference_loss_pkt = loss;
	    move_stat.reference_time = in_time;
	    move_stat.reference_rtt = rtt;
            move_stat.reference_total_pkt = total;
            move_stat.reference_latency_info = latency_info;
        }

        utility_sum_ += curr_utility;
        measurement_intervals_++;
        ConnectionState old_state;
#ifdef DEBUG
        cerr<<"Monitor "<<endMonitor<<" ended with utility "<<curr_utility<<"total "<<total<<"loss pkt"<<loss<<endl;
#endif
        // TODO we should keep track of all monitors and closely mointoring RTT
        // and utility change between monitor
        if(double(loss)/total >0.5) {
            bool stop_flag = true;
            if(last_stop_monitor == -1) {
                last_stop_monitor = current;
                if(current < endMonitor) {
                    last_stop_monitor += 100;
                }
            } else {
                if(endMonitor==last_stop_monitor || endMonitor+100==last_stop_monitor) {
                    last_stop_monitor = current;
                    if(current < endMonitor) {
                        last_stop_monitor += 100;
                    }
                } else {stop_flag = false;}
            }

            if(stop_flag) {
#ifdef DEBUG
                cerr<<"Emergency stop"<<endl;
#endif
                state_ = SEARCH;
                guess_measurement_bucket.clear();
                base_rate_ = base_rate_ * 0.5;
                move_stat.target_monitor = -1;
                if(base_rate_ < kMinRateMbps) {
                    base_rate_ = kMinRateMbps;
                }
                setRate(base_rate_);
            }
        } else {
            last_stop_monitor = -1;
        }

        if (state_ != HIBERNATE && total != 1) {
            if(!(deviation_immune_monitor != -1 &&
                    deviation_immune_monitor != endMonitor)) {
                deviation_immune_monitor = -1;
                //if(!recent_end_stat.initialized) {
                //    recent_end_stat.initialized = true;
                //} else {
                //    //if(recent_end_stat.rtt/ rtt > 1.2 || recent_end_stat.rtt / rtt <0.8){
                //    if(recent_end_stat.rtt/ rtt < 0.6){
                //    //if(recent_end_stat.rtt/ rtt < 0.7){
                //        cerr<<"RTT deviation severe, halving rate and re-probing"<<endl;
                //        state_ = SEARCH;
                //        guess_measurement_bucket.clear();
                //        base_rate_ = base_rate_ * 0.5;
                //        if(base_rate_ < kMinRateMbps/ (1-getkDelta())) {
                //           base_rate_ = kMinRateMbps / (1-getkDelta());
                //        }
                //        setRate(base_rate_);
                //        recent_end_stat.initialized = false;
                //        //deviation_immune_monitor = current;
                //    }
                //}
                recent_end_stat.utility = curr_utility;
                recent_end_stat.total = total;
                recent_end_stat.loss = loss;
                recent_end_stat.rtt = rtt;
                recent_end_stat.monitor = endMonitor;
            } else {
                cout<<"deviation immuned until "<<deviation_immune_monitor<<"currently end"<<endMonitor<<endl;
            }
        }
        do {
            old_state = state_;
            switch (state_) {
            case START:
                // TODO to aid debuggin as we change code architecture, we will not
                // have slow start here, we will immediately transit to SEARCH state
                //state_ = SEARCH;
                if(endMonitor == monitor_in_start_phase_) {
                    if(last_utility_> curr_utility) {
                        base_rate_ /= slow_start_factor_;
                        state_ = SEARCH;
                        monitor_in_start_phase_ = -1;
                    } else {
                        monitor_in_start_phase_ = -1;
                    }
                    last_utility_ = curr_utility;
                }
                break;
            case SEARCH:
                // When doing search (calculating the results and stuff), onmonitorends should do nothing
                // and ignore the monitor that ended
#ifdef DEBUG
                cerr<<"monitor"<<endMonitor<<
                    "ends in search state, this should not happen often"<<endl;
#endif
                break;
            case RECORDING:
                // onMoniitorEnd will check if all search results have come back
                // and decide where to move the rate
                // TODO: it should enter the MOVING state here, but I will just keep it simple to make it enter
                // search state again. To first switch the architecture
                //if(!recent_end_stat.initialized) {
                //    recent_end_stat.initialized = true;
                //} else {
                //    //if(recent_end_stat.rtt/ rtt > 1.2 || recent_end_stat.rtt / rtt <0.8){
                //    if(recent_end_stat.rtt/ rtt < 0.8){
                //        cerr<<"RTT deviation severe, halving rate and re-probing"<<endl;
                //        state_ = SEARCH;
                //        guess_measurement_bucket.clear();
                //        base_rate_ = base_rate_ * 0.6;
                //        if(base_rate_ < kMinRateMbps/ (1-kDelta)) {
                //           base_rate_ = kMinRateMbps / (1-kDelta);
                //        }
                //        setRate(base_rate_);
                //        recent_end_stat.initialized = false;
                //        break;
                //    }
                //}
                //recent_end_stat.utility = curr_utility;
                //recent_end_stat.total = total;
                //recent_end_stat.loss = loss;
                //recent_end_stat.rtt = rtt;
                //recent_end_stat.monitor = endMonitor;
                bool all_ready;
                all_ready = true;

#ifdef DEBUG
                cerr<<"checking if all recording ready at monitor"<<current<<endl;
#endif
                for (int i=0; i<number_of_probes_; i++) {
                    if (guess_measurement_bucket[i].monitor == endMonitor) {
#ifdef DEBUG
                        cerr<<"found matching monitor"<<endMonitor<<endl;
#endif
                        guess_measurement_bucket[i].utility = curr_utility;
                        guess_measurement_bucket[i].loss_rate = loss_rate;
                        guess_measurement_bucket[i].loss = loss;
                        guess_measurement_bucket[i].total = total;
                        guess_measurement_bucket[i].time = in_time;
                        guess_measurement_bucket[i].rtt = rtt;
                        guess_measurement_bucket[i].latency_info = latency_info;
                        guess_measurement_bucket[i].ready = true;
                    }

                    if(guess_measurement_bucket[i].ready == false) {
                        all_ready = false;
                    }
                }

                if (all_ready) {
                    double utility_down=0, utility_up=0, loss_up = 0, loss_down = 0;
                    double rate_up = 0, rate_down = 0;
                    double change = 0;
                    int decision = 0;
                    double overall_total=0, overall_loss=0;
                    double overall_loss_rate =0;

                    for(int i=0; i < number_of_probes_; i++) {
                        overall_total += guess_measurement_bucket[i].total;
                        overall_loss += guess_measurement_bucket[i].loss;
                    }
                    
                    overall_loss_rate = overall_loss/overall_total;


                    /*if(overall_loss_rate >= 0.05) {
                        //cout<<"detect "<<overall_loss_rate<<endl;
                        double loss_rate_to_use = 0;
                        for(int i=0; i < number_of_probes_; i++) {
                            if(guess_measurement_bucket[i].loss_rate>loss_rate_to_use) {
                                loss_rate_to_use = guess_measurement_bucket[i].loss_rate;
                            }
                        }
                        for(int i=0; i < number_of_probes_; i++) {
                            guess_measurement_bucket[i].utility = utility(guess_measurement_bucket[i].total, loss_rate_to_use*guess_measurement_bucket[i].total, guess_measurement_bucket[i].time, guess_measurement_bucket[i].rtt,
                                           guess_measurement_bucket[i].latency_info);
                        }
                    }*/

                    if(overall_loss_rate <= 0.01) {
                        //cout<<"detect 2"<<overall_loss_rate<<endl;
                        //cout<<"overall loss rate"<<overall_loss_rate<<endl;
                        double loss_rate_to_use = 0;
                        for(int i=0; i < number_of_probes_; i++) {
                                //cout<<guess_measurement_bucket[i].loss_rate<<endl;
                            if(guess_measurement_bucket[i].loss_rate>loss_rate_to_use) {
                            }
                        }
                        loss_rate_to_use = overall_loss_rate;
                        for(int i=0; i < number_of_probes_; i++) {
                            guess_measurement_bucket[i].utility = utility(guess_measurement_bucket[i].total, loss_rate_to_use*guess_measurement_bucket[i].total, guess_measurement_bucket[i].time, guess_measurement_bucket[i].rtt,
                                           guess_measurement_bucket[i].latency_info);
                        }
                    }

                    for(int i=0; i < number_of_probes_/2; i++) {
                        if(guess_measurement_bucket[i*2].utility < guess_measurement_bucket[i*2
                                +1].utility) {
                            if(guess_measurement_bucket[i*2 + 1].isup) {
                                decision ++;
                            } else {
                                decision --;
                            }
                        }

                        if(guess_measurement_bucket[i*2].utility > guess_measurement_bucket[i*2
                                +1].utility) {
                            if(guess_measurement_bucket[i*2].isup) {
                                decision ++;
                            } else {
                                decision --;
                            }
                        }
                    }
                    for(int i=0; i < number_of_probes_; i++) {
                        if(guess_measurement_bucket[i].isup){
                            loss_up += guess_measurement_bucket[i].loss;
                        } else {
                            loss_down += guess_measurement_bucket[i].loss;
                        }
                    }


                    if((loss_down - loss_up) >2 && decision>0 && loss_up !=0 && overall_loss_rate>=0.05) {
                       cout<<"hit"<<endl;
                       if(double_check >0) {
                          decision = 0;
                          double_check --;
                       } else {
                          double_check = 1; 
                          trend_count_ = 2;
                       }
                    } else {
                       double_check = 1;
                    }

                    if(decision != 0) {
                        for (int i=0; i<number_of_probes_; i++) {
                            if(guess_measurement_bucket[i].isup) {
                                utility_up += guess_measurement_bucket[i].utility;
                                rate_up = guess_measurement_bucket[i].rate;
                            } else {
                                utility_down += guess_measurement_bucket[i].utility;
                                rate_down = guess_measurement_bucket[i].rate;
                            }
                        }
                        int factor = number_of_probes_/2;
                        // Sanity check maybe needed here, but not sure
                        // but watch out for huge jump is needed
                        // maybe this will work, if this does not, need to revisit sanity check
                        change = decide(utility_down/factor, utility_up/factor, rate_down, rate_up,
                                        false);
                        //TODO: take care of this special case where change =0
                        //if(avg_loss > 0.05 && change >0) {
                        //   change = 0;
                        //}
                        //cout<<"decide "<<change<<endl;
                        //if(abs(change)/base_rate_ > 0.5 && change <0) {change = change/abs(change)*0.5*base_rate_;}
#ifdef DEBUG
                        cerr<<"all record is acquired and ready to change by "<<change<<endl;
#endif
                        if(change/base_rate_<=0.1) {
                            state_ = SEARCH;
                            base_rate_ =base_rate_+change;
                            setRate(base_rate_);
                        } else {
                            state_ = MOVING;
                            move_stat.target_rate = move_stat.reference_rate + change;
                            move_stat.change = change;
                            if(probe_amplifier > 0)
                                probe_amplifier --;
                        }
                    } else {
                        //if(probe_amplifier < 5)
                        trend_count_ = 2;
                        prev_change_ = 0;
                        state_ = SEARCH;
                        move_stat.reference_ready = false;
                        probe_amplifier ++;
                    }
                    guess_measurement_bucket.clear();
                }
                break;
            case MOVING:
                if (endMonitor > move_stat.target_monitor) {
                    // should handle the fact that when endMonitor is larger than the target monitor
                    // somethign really bad should have happened

#ifdef DEBUG
                    cerr<<"end monitor:"<<endMonitor<<"is larger than the target monitor: "<<move_stat.target_monitor<<endl;
#endif
                }
                if(endMonitor == move_stat.target_monitor) {
#ifdef DEBUG
                    cerr<<"find the right monitor"<<endMonitor<<endl;
#endif
                    move_stat.target_utility = curr_utility;
                    move_stat.target_loss_rate = loss_rate;
                    move_stat.target_loss_pkt = loss;
                    move_stat.target_time = in_time;
                    move_stat.target_rtt = rtt;
                    move_stat.target_total_pkt = total;
                    move_stat.target_latency_info = latency_info;
                    move_stat.target_ready = true;
                }

                if (move_stat.reference_ready && move_stat.target_ready){
                        // see if the change direction is wrong and is reversed
                        double overall_loss =0, overall_total = 0;
                        overall_loss = move_stat.reference_loss_pkt + move_stat.target_loss_pkt;
                        overall_total = move_stat.reference_total_pkt + move_stat.target_loss_pkt;
                        double overall_loss_rate = overall_loss/overall_total;
                        //if(overall_loss_rate >= 0.05) {
                        //   cerr<<"detect"<<overall_loss_rate<<" refernece loss "<<move_stat.reference_loss_rate<<"target loss rate"<<move_stat.target_loss_rate<<endl;
                        //   double loss_rate_to_use = (move_stat.reference_loss_rate>move_stat.target_loss_rate)?move_stat.reference_loss_rate:move_stat.target_loss_rate;
                        //   move_stat.reference_utility = utility(move_stat.reference_total_pkt, move_stat.reference_total_pkt*loss_rate_to_use, move_stat.reference_time, move_stat.reference_rtt,
                        //                   move_stat.reference_latency_info);
                        //   move_stat.target_utility = utility(move_stat.target_total_pkt, move_stat.target_total_pkt*loss_rate_to_use, move_stat.target_time, move_stat.target_rtt,
                        //                   move_stat.target_latency_info);
                        //   cerr<<"loss rate to use is "<<loss_rate_to_use<<"utility after calculation is "<<move_stat.target_utility<<" "<<move_stat.reference_utility<<endl;
                        //}

                        double change = decide(move_stat.reference_utility, move_stat.target_utility,
                                               move_stat.reference_rate, move_stat.target_rate, false);
                        cerr<<"change for move is "<<change<<endl;
                        bool second_guess = false;
                        if((move_stat.reference_rate - move_stat.target_rate) * (move_stat.reference_loss_rate - move_stat.target_loss_rate) <0
                            && (abs(move_stat.reference_loss_pkt - move_stat.target_loss_pkt) > 2 || overall_loss_rate >= 0.05)) {
                           second_guess = true;
                           if(double_check > 0) {
                               double_check--;
                           }
                        } else {
                           
                        }
                        if (second_guess) {
                                cerr<<"second guess"<<endl;
                                move_stat.target_monitor = -1;
                                state_ = SEARCH;
                                trend_count_ = 2;
                                prev_change_ = 0;
                                move_stat.reference_ready = false;
                                move_stat.target_ready = false;
                                guess_measurement_bucket.clear();

                        } else {
                            if (change * move_stat.change <= 0) {
                                cerr<<"direction changed"<<endl;
                                cerr<<"change is "<<change<<" old change is "<<move_stat.change<<endl;
                                // the direction is different, need to move to old rate start to re-guess
                                //base_rate_ = move_stat.reference_rate;
                                base_rate_ = change + base_rate_;
                                setRate(base_rate_);
                                move_stat.target_monitor = -1;
                                state_ = SEARCH;
                                move_stat.reference_ready = false;
                                move_stat.target_ready = false;
                                guess_measurement_bucket.clear();
                                number_of_probes_ = 4;
                            } else {
                                cerr<<"direction same, keep moving with change of "<<change<<endl;
                                if(change/base_rate_<=0.1) {
                                    base_rate_ = change + base_rate_;
                                    setRate(base_rate_);
                                    move_stat.target_monitor = -1;
                                    state_ = SEARCH;
                                    move_stat.reference_ready = false;
                                    move_stat.target_ready = false;
                                    guess_measurement_bucket.clear();
                                    trend_count_ = 2;
                                } else {
                                    move_stat.reference_monitor = move_stat.target_monitor;
                                    move_stat.reference_utility = move_stat.target_utility;
                                    move_stat.target_monitor = (current + 1) % MAX_MONITOR;
                                    move_stat.reference_ready = true;
                                    move_stat.target_ready = false;
                                    move_stat.change = change;
                                    move_stat.reference_rate = move_stat.target_rate;
                                    move_stat.reference_loss_rate = move_stat.target_loss_rate;
                                    move_stat.reference_loss_pkt = move_stat.target_loss_pkt;
                                    move_stat.reference_total_pkt = move_stat.target_total_pkt;
                                    move_stat.reference_time = move_stat.target_time;
                                    move_stat.reference_rtt = move_stat.target_rtt;
                                    move_stat.target_rate = move_stat.reference_rate + change;

                                    //move_stat.reference_monitor = current;
                                    //move_stat.target_monitor = (current + 1) % MAX_MONITOR;
                                    //move_stat.reference_ready = false;
                                    //move_stat.target_ready = false;
                                    //move_stat.change = change;
                                    //move_stat.reference_rate = move_stat.target_rate;
                                    //move_stat.reference_loss_rate = move_stat.target_loss_rate;
                                    //move_stat.reference_loss_pkt = move_stat.target_loss_pkt;
                                    //move_stat.target_rate = move_stat.reference_rate + change;
                                }
                            }
                        }
                }
                break;
            case HIBERNATE:
                double base_line_rate = 2 * 1.5 / 1024 * 8/(rtt);
                if (base_line_rate < 0.4) {
                    break;
                }
                //base_rate_ =kMinRateMbps / (1-kDelta) >base_line_rate?kMinRateMbps / (1-kDelta):base_line_rate;
                base_rate_ = kMinRateMbps;
                //if(base_rate_ == kMinRateMbps) {
                //    base_rate_ += 0.25;
                //}
                if(base_rate_ - 0.1 < kMinRateMbps) {
                    base_rate_ += 0.1;
                }
                setRate(base_rate_);
#ifdef DEBUG
                cerr<<"coming out of comma with base rate of"<<base_rate_<<endl;
#endif
                guess_measurement_bucket.clear();
                state_ = SEARCH;
                hibernate_depth = 0;
                timeout_immune_monitor = current;
            }

        } while(old_state != state_);

    }

    void search(int current_monitor) {
        if(trend_count_ >= 1) {
            //cout<<"turn to fast moving mode"<<endl;
            number_of_probes_ = 2;
        } else {
            //cout<<"turn to SLOW moving mode"<<endl;
            number_of_probes_ = 4;
        }
        
        

        for(int i=0; i<number_of_probes_/2; i++) {
            GuessStat g = GuessStat();
            int dir = rand()%2*2-1;
            for(int j=0; j<2; j++) {
                if((getkDelta()) * base_rate_ > 0.1) {
                    g.rate = (1 + dir * getkDelta()) * base_rate_;
                } else {
                    g.rate = base_rate_ + dir * 0.1;
                }
                g.isup = (dir>0);
                g.ready = false;
                g.monitor = (current_monitor + i*2 + j) % MAX_MONITOR;
                guess_measurement_bucket.push_back(g);
                dir *= -1;
            }
        }
    }

    virtual double decide(long double start_utility, long double end_utility,
                          double old_rate, double new_rate, bool force_change) {
        double gradient = (end_utility - start_utility) / (new_rate - old_rate);
        //cout<<"gradient is "<<gradient<<" "<<end_utility<<" "<<start_utility<<" "<<new_rate<<" "<<old_rate<<endl;
        prev_gradiants_[curr_] = gradient;

        curr_ = (curr_ + 1) % MAX_MONITOR;

        //double change = 2 * rate()/1000 * kEpsilon * avg_gradient();
        //double change = avg_gradient() * rate();
        double change = avg_gradient() * kFactor;
        if(change * prev_change_ < 0) {
            //if(amplifier >0)
            //    amplifier --;
            //if(boundary_amplifier >0)
            //    boundary_amplifier --;
            amplifier = 0;
            boundary_amplifier = 0;
            if(swing_buffer < 2)
                swing_buffer ++;

        } else if(prev_change_ * change ==0) {
            // do nothing
        }
        //cout<<"amplifier"<<amplifier<<endl;
        if(amplifier<3) {
            change *= (pow(amplifier, 1) * 1  + 1);
        } else if (amplifier < 6) {
            change *= (pow(amplifier, 1) * 2  - 3 + 1);
        } else if (amplifier < 9) {
            change *= (pow(amplifier, 1) * 4 - 15 + 1);
        } else {
            change *= (pow(amplifier, 1) * 8 - 51 + 1);
        }

        //if(amplifier<6) {
        //    change *= (pow(amplifier, 1) * 1  + 1);
        //} else if (amplifier < 10) {
        //    change *= (pow(amplifier, 1) * 4  - 18 + 1);
        //} else {
        //    change *= (pow(amplifier, 1) * 8 - 58 + 1);
        //}
        if(change * prev_change_ < 0) {
            trend_count_ =0;
        } else if(prev_change_ * change ==0) {
            // do nothing
        } else {
            trend_count_ ++;
            if(swing_buffer == 0) {
                if(amplifier > 3) {
                    amplifier +=0.5;
                }
                else {
                    amplifier ++;
                }
            }
            if (swing_buffer > 0) {
                swing_buffer --;
            }
        }
        //change *= (amplifier*0.05+ kFactor);
        
        //cout<<swing_buffer<<"\t"<<amplifier<<endl;
        //cout<<boundary_amplifier<<endl;
        //cout<<change<<endl;

#ifdef DEBUG
        cerr<<"change before force to boundary "<< change<<endl;
#endif

        double ratio = kBoundaryIncrement*boundary_amplifier + kInitialBoundary;
        //if(ratio>0.8 && change<0) {
        //   ratio = 0.8;
        //}
        //if(ratio>2 && change>0) {
        //   ratio = 2;
        //}
        if((abs(change)/base_rate_)>ratio) {
            change = abs(change)/change*base_rate_*ratio;
                boundary_amplifier+=1;
                //if(boundary_amplifier >= 2) {
                //    boundary_amplifier += 0.5;
                //} else { boundary_amplifier+=1;}
        } else {
            if(boundary_amplifier >= 1)
                boundary_amplifier-=1;
            //cout<<"not forcing"<<endl;
        }

        if(change * prev_change_ < 0) {
            amplifier = 0;
            boundary_amplifier = 0;
        } 

        if(abs(change)/base_rate_ > 0.5) {
            //change = abs(change)/change*rate()*(0.5);
        }
        if(abs(change)/base_rate_ < 0.05) {
            //if(0.5 > base_rate_ * 0.05)
            //change = abs(change)/change*base_rate_*(0.05);
            //else
            //change = 0.5 * abs(change)/change;
        }


        if (force_change) {
            cout << "avg. gradient = " << avg_gradient() << endl;
            cout << "rate = " << rate() << endl;
            cout << "computed change: " << change << endl;
        }
#ifdef DEBUG
        cerr<<"change before force to min change is "<< change<<endl;
        cerr<<"gradient is "<< avg_gradient()<<endl;
        cerr<<"amplifier"<<amplifier<<endl;
#endif

        //if ((change >= 0) && (change < getMinChange())) change = getMinChange();

        //if (change>0 && change < base_rate_*getkDelta()) { change = base_rate_ * getkDelta();}
        //if (change <0 && change > base_rate_*getkDelta() * (-1)) {change = base_rate_ *getkDelta() * (-1);}

        if (change>0 && change < 0.5) {
            change = 0.5;
        }
        if (change <0 && change > -0.5) {
            change = -0.5;
        }

        prev_change_ = change;

        if (change == 0) cout << "Change is zero!" << endl;
#ifdef DEBUG
        cerr<<"change is "<<change<<endl;
#endif
        return change;

    }
    static void set_utility_params(double alpha = 0.2, double beta = 54,
                                   double exponent = 1.5, bool polyUtility = true, double factor = 2.0,
                                   double step = 0.05, double latencyCoefficient = 1,
                                   double initialBoundary = 0.1, double boundaryIncrement = 0.07) {
        kAlpha = alpha;
        kBeta = beta;
        kExponent = exponent;
        kFactor = factor;
        kLatencyCoefficient = latencyCoefficient;
        kInitialBoundary = initialBoundary;
        kBoundaryIncrement = boundaryIncrement;
        kStep = step;
    }


  protected:

    static double kAlpha, kBeta, kExponent;
    static bool kPolyUtility;
    static double kFactor, kStep;
    static double kLatencyCoefficient, kInitialBoundary, kBoundaryIncrement;

    int timeout_immune_monitor;
    int deviation_immune_monitor;
    double base_rate_;
    bool kPrint;
    double prev_change_;
    static constexpr double kMinRateMbps = 1.3;
    static constexpr double kMinRateMbpsSlowStart = 0.1;
    static constexpr double kHibernateRate = 0.04;
    static constexpr double kMaxRateMbps = 1024.0;
    static constexpr int kRobustness = 1;
    static constexpr double kEpsilon = 0.003;
    static constexpr double kDelta = 0.05;
    static constexpr int kGoToStartCount = 50000;

    enum ConnectionState {
        START,
        SEARCH,
        RECORDING,
        MOVING,
        HIBERNATE
    } state_;



    virtual void clear_state() {
        slow_start_factor_ = 2;
        state_ = SEARCH;
        monitor_in_start_phase_ = -1;
        kPrint = false;
        trend_count_ = 0;
        curr_ = 0;
        prev_change_ = 0;
    }

    virtual void restart() {
        slow_start_factor_ = 2;
        monitor_in_start_phase_ = -1;
        setRate(base_rate_);
        kPrint = false;
        state_ = SEARCH;
        trend_count_ = 0;
        curr_ = 0;
        prev_change_ = 0;
    }


    virtual double getMinChange() {
        return 0.5;
        if (base_rate_ > kMinRateMbps) {
            return kMinRateMbps;
        } else if (base_rate_ > kMinRateMbps / 2) {
            return kMinRateMbps / 2;
        } else {
            return 2 * kMinRateMbpsSlowStart;
        }
    }
    virtual void setRate(double mbps, bool force=false) {
        if(force) {
            m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
            return;
        }

#ifdef DEBUG
        cerr << "set rate: " << rate_ << " --> " << mbps << endl;
#endif
        if (state_ == START) {
            if (mbps < kMinRateMbpsSlowStart) {
#ifdef DEBUG
                cerr << "rate is mimimal at slow start, changing to " << kMinRateMbpsSlowStart
                     << " instead" << endl;
#endif
                mbps = kMinRateMbpsSlowStart;
            }
        } else if (mbps < kMinRateMbps * (1-getkDelta())) {
#ifdef DEBUG
            cerr << "rate is mimimal, changing to " << kMinRateMbps << " instead" << endl;
#endif
            mbps = kMinRateMbps * (1-getkDelta());
        }

        if (mbps > kMaxRateMbps) {
            mbps = kMaxRateMbps;
#ifdef DEBUG
            cerr << "rate is maximal, changing to " << kMaxRateMbps << " instead" << endl;
#endif
        }
        rate_ = mbps;
        m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
        //cerr << "setting rate: mbps = " << mbps << endl;
    }

    double rate() const {
        return rate_;
    }

  public:

    double avg_gradient() const {
        int base = curr_;
        double sum = 0;
        for (int i = 0; i < kRobustness; i++) {
            base += MAX_MONITOR-1;
            sum += prev_gradiants_[base % MAX_MONITOR];
            //cout << "gradient " << prev_gradiants_[base % MAX_MONITOR] << " ";
        }
        return sum / kRobustness;
    }

    double get_min_rtt(double curr_rtt) {
        double min = curr_rtt;
        if ((rtt_history_.size()) == 0) {
            min = curr_rtt;
        } else {
            min = *rtt_history_.cbegin();
            for (deque<double>::const_iterator it = rtt_history_.cbegin();
                    it!=rtt_history_.cend(); ++it) {
                if (min > *it) {
                    min = *it;
                }
            }
        }

        rtt_history_.push_front(curr_rtt);
        if (rtt_history_.size() > kHistorySize) {
            rtt_history_.pop_back();
        }

        return min;
    }

    virtual long double utility(double total, double loss,
                                double time, double rtt, double latency_info) {
        static long double last_measurement_interval = 1;

        long double norm_measurement_interval = last_measurement_interval;

        if (!kInTimeout) {
            norm_measurement_interval = time / rtt;
            last_measurement_interval = norm_measurement_interval;
        }
        long double loss_rate = (long double)((double) loss/(double) total);
	sum_total += total;
        sum_loss += loss;
        //if(loss_rate > 0) {
        //   loss_rate = ceil(loss_rate * 100)/100.0;
        //}
        //if(loss_rate > 0.05) {
        //   loss_rate = ceil(loss_rate * 100 +1)/2*2/100.0;
        //}

        avg_loss =  loss_rate * 0.2 + avg_loss *0.8;


        // convert to milliseconds
        double rtt_penalty = (rtt - get_min_rtt(rtt))/time;
        //rtt_penalty = (pow(rtt_penalty+1, 2) -1) * rtt/0.03;
        //cout<<"RTT penalty is"<<rtt_penalty<<endl;
        rtt_penalty = int(int(latency_info * 100) / 100.0 * 100) / 2  * 2/ 100.0;
        if(rtt_penalty < -0.2) {
            //cout<<"rtt penalty"<<rtt_penalty<<endl;
            //rtt_penalty = -0.2;
        }
        //rtt_penalty = int(latency_info * 100) / 100.0;
        //cerr<<"new rtt penalty is "<<rtt_penalty<<endl;
        //if (rtt_penalty > 2) rtt_penalty  = 2;
        //if (rtt_penalty < -2) rtt_penalty  = -2;
        exponent_ = 1;
        //if(rtt_penalty<1) {
        //    rtt_penalty=1;
        //}

        //long double loss_contribution = total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1));
        long double loss_contribution = total* (11.35 * (pow((1+loss_rate),
                                                exponent_)-1));
        

        if(loss_rate <= 0.03)
            loss_contribution = total* (1* (pow((1+loss_rate), exponent_)-1));

        //    //loss_contribution = 0;
        //else if (loss_rate<=0.01)
        //    loss_contribution = 0;

        //if(avg_loss > 0.05) {
        //   loss_control_amplifier += 0.1;
        //   if(loss_control_amplifier > 4) {
        //       loss_control_amplifier = 4;
        //   }
        //}else {
        //   loss_control_amplifier = 1;
        //}


        //cout<<"avg_loss is "<<avg_loss<<endl;
        //cout<<"loss rate"<<loss_rate<<endl;
        //long double loss_contribution = total* (11.35 * (pow((1+loss_rate), exponent_)-1));
        //long double loss_contribution = total* (kBeta * (1/(1-loss_rate)-1));
        //long double rtt_contribution = 1 * total*(pow(rtt_penalty,1) -1);
        //long double rtt_contribution = 1 * total*(pow(latency_info,2) -1);
        //long double rtt_contribution = 1 * total*(pow(latency_info,1));
        long double rtt_contribution =  kLatencyCoefficient * 11330 * total*(pow(
                                            rtt_penalty,1));
        long double rtt_factor = rtt;
        //TODO We should also consider adding just rtt into the utility function, because it is not just change that matters
        // This may turn out to be extremely helpful during LTE environment
        //long double utility = ((long double)total - loss_contribution - rtt_contribution)/norm_measurement_interval/rtt;
        //long double utility = (((long double)total - loss_contribution) - rtt_contribution)/time/norm_measurement_interval;
        double normalized_rtt = rtt / 0.04;
        //long double utility = (((long double)total - loss_contribution) - rtt_contribution)*m_iMSS/1024/1024*8/time/latency_info;
        long double utility = kAlpha * pow((long double)(total)
                                           *m_iMSS/1024/1024*8/time,
                                           kExponent) - (1*loss_contribution + rtt_contribution)*m_iMSS/1024/1024*8/time;
#ifdef DEBUG
        cerr<<"total "<<
            total<<"loss contri"<<loss_contribution<<"rtt contr"<<rtt_contribution<<"time "<<time<<"norm measurement"<<norm_measurement_interval<<endl;
        cerr<<"rtt is "<<rtt<<"loss rate is "<<loss_rate<<"avg loss rate is "<<avg_loss<<endl;
        cerr<<"latency info is "<<latency_info<<endl;
#endif

        return utility;
    }

    static const long kMillisecondsDigit = 10 * 1000;

    int monitor_in_start_phase_;
    double avg_rtt;
    double slow_start_factor_;
    double alpha_;
    double beta_;
    double exponent_;
    double factor_;
    double step_;
    double rate_;
    long double utility_sum_;
    size_t measurement_intervals_;
    double amplifier;
    int probe_amplifier;
    double boundary_amplifier;
    long double prev_utility_;
    int number_of_probes_;
    vector<GuessStat> guess_measurement_bucket;
    MoveStat move_stat;
    RecentEndMonitorStat recent_end_stat;
    int guess_time_;
    int current_start_monitor_;
    long double last_utility_;
    deque<double> rtt_history_;
    static constexpr size_t kHistorySize = 1;
    int hibernate_depth;
    int trend_count_;
    double avg_loss;
    double loss_control_amplifier;
    uint64_t sum_total;
    uint64_t sum_loss;
    int double_check;

    int curr_;
    double prev_gradiants_[MAX_MONITOR];
    double swing_buffer;
    int loss_ignore_count;
    int last_stop_monitor;
};

#endif
