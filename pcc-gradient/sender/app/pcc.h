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
//#define DEBUG
#define MAX_MONITOR 500
using namespace std;

bool kInTimeout = false;

struct GuessStat {
    int monitor;
    double rate;
    long double utility;
    bool ready;
    bool isup;
};


struct MoveStat {
    double rate;
    double next_rate;
    double change;
    long double utility;
    int target_monitor;
    bool bootstrapping;
    bool isup;
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
        utility_sum_(0), measurement_intervals_(0), prev_utility_(-10000000),
        last_utility_(-100000) {
        amplifier = 0;
        boundary_amplifier = 0;
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
        swing_buffer = 0;
        recent_end_stat.initialized = false;
        setRTO(100000000);
        recent_end_stat.initialized = false;
        srand(time(NULL));
        avg_loss = 0;
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
        return kStep;
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
                                int& mss, int& length_amplifier) {
        ConnectionState old_state;
#ifdef DEBUG
        cerr<<"Monitor "<<current_monitor<<" starts"<<endl;
#endif
        //length_amplifier = probe_amplifier;
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
                }
                break;
            case MOVING:
                // TODO: should handle how we move and how we record utility as well
#ifdef DEBUG
                cerr<<"monitor "<<current_monitor<<"is in moving state setting rate to"<<move_stat.next_rate<<endl;
#endif
                setRate(move_stat.next_rate);
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
        long double curr_utility = utility(total, loss, in_time, rtt,
                                           latency_info);
        utility_sum_ += curr_utility;
        measurement_intervals_++;
        ConnectionState old_state;
#ifdef DEBUG
        cerr<<"Monitor "<<endMonitor<<" ended with utility "<<curr_utility<<endl;
#endif
        // TODO we should keep track of all monitors and closely mointoring RTT
        // and utility change between monitor
        if(double(loss)/total >0.8) {
            state_ = SEARCH;
            guess_measurement_bucket.clear();
            base_rate_ = base_rate_ * 0.5;
            if(base_rate_ < kMinRateMbps/ (1-getkDelta())) {
                base_rate_ = kMinRateMbps / (1-getkDelta());
            }
            setRate(base_rate_);
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
                        guess_measurement_bucket[i].ready = true;
                    }

                    if(guess_measurement_bucket[i].ready == false) {
                        all_ready = false;
                    }
                }

                if (all_ready) {
                    double utility_down=0, utility_up=0;
                    double rate_up = 0, rate_down = 0;
                    double change = 0;
                    int decision = 0;
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
                        //cout<<"decide "<<change<<endl;
                        //if(abs(change)/base_rate_ > 0.5 && change <0) {change = change/abs(change)*0.5*base_rate_;}
#ifdef DEBUG
                        cerr<<"all record is acquired and ready to change by "<<change<<endl;
#endif
                        base_rate_ += change;
                        if(probe_amplifier > 0)
                            probe_amplifier --;
                    } else {
                        //if(probe_amplifier < 5)
                        probe_amplifier ++;
                    }
                    if (base_rate_ < kMinRateMbps) {
#ifdef DEBUG
                        cerr<<"trying to set rate below min rate in moving phase just decided, enter guessing"<<endl;
#endif
                        base_rate_ = kMinRateMbps/ (1 - getkDelta());
                        setRate(base_rate_);
                        amplifier = 0;
                        boundary_amplifier = 0;
                        state_ = START;
                        guess_measurement_bucket.clear();
                        break;
                    }
                    setRate(base_rate_);
                    state_ = SEARCH;
                    move_stat.bootstrapping = true;
                    move_stat.target_monitor = (current +1) % MAX_MONITOR;
                    move_stat.next_rate = base_rate_;
                    move_stat.rate = base_rate_;
                    move_stat.change = change;
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
                    if(move_stat.bootstrapping) {
                        cerr<<"bootstrapping move operations"<<endl;
                        move_stat.bootstrapping = false;
                        move_stat.utility = curr_utility;
                        // change stay the same
                        move_stat.target_monitor = (current + 1) % MAX_MONITOR;
                        cerr<<"target monitor is "<<(current + 1) % MAX_MONITOR;
                        move_stat.next_rate = move_stat.next_rate + move_stat.change;
                        base_rate_ = move_stat.next_rate;

                        if (base_rate_ < kMinRateMbps) {
                            cerr<<"trying to set rate below min rate in moving phase bootstrapping, enter guessing"<<endl;
                            base_rate_ = kMinRateMbps/ (1 - getkDelta());
                            state_ = SEARCH;
                            guess_measurement_bucket.clear();
                            break;
                        }

                        setRate(base_rate_);
                    } else {
                        // see if the change direction is wrong and is reversed
                        double change = decide(move_stat.utility, curr_utility,
                                               move_stat.next_rate - move_stat.change, move_stat.next_rate, false);
                        cerr<<"change for move is "<<change<<endl;
                        if (change * move_stat.change < 0) {
                            cerr<<"direction changed"<<endl;
                            cerr<<"change is "<<change<<" old change is "<<move_stat.change<<endl;
                            // the direction is different, need to move to old rate start to re-guess
                            if (abs(change) > abs(move_stat.change)) {
                                base_rate_ = move_stat.next_rate + change;
                            } else {
                                base_rate_ = move_stat.next_rate - move_stat.change;
                            }
                            base_rate_ = move_stat.next_rate - move_stat.change;
                            setRate(base_rate_);
                            state_ = SEARCH;
                            guess_measurement_bucket.clear();
                        } else {
                            cerr<<"direction same, keep moving with change of "<<change<<endl;
                            move_stat.target_monitor = (current + 1) % MAX_MONITOR;
                            move_stat.utility = curr_utility;
                            move_stat.change = change;
                            move_stat.next_rate = move_stat.change + move_stat.next_rate;
                            base_rate_ = move_stat.next_rate;

                            if (base_rate_ < kMinRateMbps) {
                                cerr<<"trying to set rate below min rate in moving phase keep moving, enter guessing"<<endl;
                                base_rate_ = kMinRateMbps/ (1 - getkDelta());
                                state_ = SEARCH;
                                guess_measurement_bucket.clear();
                                break;
                            }

                            setRate(base_rate_);
                        }
                    }
                }
                prev_utility_ = curr_utility;
                // should add target monitor
                // decide based on prev_utility_
                // and prev_rate_
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
        if(trend_count_ >= 3) {
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
        if(change * prev_change_ <= 0) {
            if(swing_buffer < 2)
                swing_buffer ++;

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
        if(change * prev_change_ <= 0) {
            trend_count_ =0;
        } else {
            trend_count_ ++;
            if(swing_buffer == 0) {
                amplifier ++;
            }
            if (swing_buffer > 0) {
                swing_buffer --;
            }
        }
        //change *= (amplifier*0.05+ kFactor);
        //cout<<pow(amplifier, 2)<<endl;
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
        } else {
            if(boundary_amplifier >= 1)
                boundary_amplifier-=1;
            //cout<<"not forcing"<<endl;
        }

        if(change * prev_change_ <= 0) {
            amplifier = 0;
            boundary_amplifier = 0;
        }
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

        if (change>0 && change < 0.25) {
            change = 0.25;
        }
        if (change <0 && change > -0.25) {
            change = -0.25;
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
    static constexpr double kMinRateMbps = 0.8;
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
        prev_utility_ = -10000000;
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
        prev_utility_ = -10000000;
        trend_count_ = 0;
        curr_ = 0;
        prev_change_ = 0;
    }


    virtual double getMinChange() {
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
        } else if (mbps < kMinRateMbps) {
#ifdef DEBUG
            cerr << "rate is mimimal, changing to " << kMinRateMbps << " instead" << endl;
#endif
            mbps = kMinRateMbps;
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

    virtual long double utility(unsigned long total, unsigned long loss,
                                double time, double rtt, double latency_info) {
        static long double last_measurement_interval = 1;

        long double norm_measurement_interval = last_measurement_interval;

        if (!kInTimeout) {
            norm_measurement_interval = time / rtt;
            last_measurement_interval = norm_measurement_interval;
        }
        long double loss_rate = (long double)((double) loss/(double) total);


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
        if(loss_rate < 0.04)
            loss_contribution = total* (1* (pow((1+loss_rate), exponent_)-1));

        //avg_loss = avg_loss*0.9 + loss_rate*0.1;
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
        cerr<<"rtt is "<<rtt<<endl;
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
    int amplifier;
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

    int curr_;
    double prev_gradiants_[MAX_MONITOR];
    double swing_buffer;
};

#endif
