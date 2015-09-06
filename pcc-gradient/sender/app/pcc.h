#ifndef __PCC_H__
#define __PCC_H__

#define _USE_MATH_DEFINES

#include <udt.h>
#include <ccc.h>
#include<iostream>
#include<cmath>
#include <deque>
#include <time.h>
#include <pthread.h>

using namespace std;

class PCC : public CCC {
public:
	virtual ~PCC() {}

	long double avg_utility() {
		if (measurement_intervals_ > 0) {
			return utility_sum_ / measurement_intervals_;
		}
		return 0;
	}

	virtual void onLoss(const int32_t*, const int&) {}
	virtual void onTimeout(){}
	virtual void onACK(const int& ack){}

	virtual void onMonitorStart(int current_monitor) {
		pthread_mutex_lock(&mutex_);
		if (state_ == START) {
			if (monitor_in_start_phase_ != -1) {
				pthread_mutex_unlock(&mutex_);
				return;
			}
			monitor_in_start_phase_ = current_monitor;
			//cout << "Start. STATE = " << state_ << " Monitor = " << monitor_in_start_phase_ << endl;
			//cout << rate() << "," << slow_start_factor_ << "-->" << rate() * slow_start_factor_ << endl;
			setRate(rate() * slow_start_factor_);
		} else if (state_ == SEARCH) {
			search();
            search_monitor_number[search_number] = current_monitor;
            search_number ++;
            if(search_number == 2) {
			    search_number = 0;
                state_ = DECISION;
            }
		}
		pthread_mutex_unlock(&mutex_);
	}

	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {
		pthread_mutex_lock(&mutex_);
		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;
		if (previous_rtt_ == 0) previous_rtt_ = rtt;

		long double curr_utility = utility(total, loss, in_time, rtt);

		conditions_changed_too_much_ = (rtt / previous_rtt_ > 1.3) || (previous_rtt_ / rtt > 1.3);
		conditions_changed_too_much_ = conditions_changed_too_much_ || ((previous_loss_ * 10 < loss) && (previous_loss_ > 0));
		previous_rtt_ = rtt;
		previous_loss_ = loss;

		utility_sum_ += curr_utility;
		measurement_intervals_++;

		//cout << "utility = " << curr_utility << endl;
		bool continue_slow_start = (loss == 0) && (curr_utility >= prev_utility_);
		long double tmp_prev_utility = prev_utility_;
		prev_utility_ = curr_utility;
		if (continue_slow_start_) no_loss_count_++;
		else no_loss_count_ = 0;
		//cout << "loss count " << no_loss_count_ << endl;
		

		//cout << "Monitor @ end: " << endMonitor << endl;
		if(state_ == START) {
			if (monitor_in_start_phase_ == endMonitor) {
				//cout << "END: State = " << state_ << " Monitor = " << monitor_in_start_phase_ << endl;		
				monitor_in_start_phase_ = -1;
				if (!continue_slow_start) {
					setRate(rate() / slow_start_factor_);
					slow_start_factor_ /= 1.5;
					if (slow_start_factor_ < 1.2) {
						//init();
						//cout << "Slow Start: Done! new = " << curr_utility << " prev " << tmp_prev_utility << endl;
						state_ = SEARCH;
					}
				}
			}
		} else if (state_ == DECISION) {
            if(endMonitor == search_monitor_number[0]) {
                search_monitor_number[0] = -1;
                search_monitor_utility[0] = curr_utility;
            }
            if(endMonitor == search_monitor_number[1]) {
                search_monitor_number[1] = -1;
                search_monitor_utility[1] = curr_utility;
            }

            if(isAllSearchResultBack()) {
                decide(curr_utility);
				state_ = SEARCH;
				/*
				if ((no_loss_count_ > 1000) && false) {
					no_loss_count_ = 0;
					state_ = START;
				}
				*/
            }
		}
		pthread_mutex_unlock(&mutex_);
	}

protected:
	bool conditions_changed_too_much_;
    long double search_monitor_utility[2];
    int search_monitor_number[2];
    int search_number;
	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC(double alpha, bool latency_mode) : conditions_changed_too_much_(false), state_(START), monitor_in_start_phase_(-1), slow_start_factor_(2), alpha_(alpha), rate_(kMinRateMbps), previous_rtt_(0),
			monitor_in_prog_(-1), utility_sum_(0), no_loss_count_(0), measurement_intervals_(0), prev_utility_(-10000000), continue_slow_start_(true) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
        search_number = 0;
		setRTO(100000000);
		srand(time(NULL));

		if (!latency_mode) {
			beta_ = 0;
		} else {
			beta_ = 0.00052; 
		}
	}

	/*
	virtual void init() {
		cout << "Init()" <<endl;
		slow_start_factor_ = 2;
		continue_slow_start_ = true;
	}
	*/
	
	virtual void setRate(double mbps) {
		//cout << "rate = " << mbps << endl;
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
	}

	double rate() const { return rate_; }

private:
	static double get_rtt(double rtt) {
		return 1000 * rtt;
		double conv_diff = (double)(((long) (rtt * 1000 * 1000)) % kMillisecondsDigit);
		return conv_diff / (1000.0 * 1000.0);
	}

    double isAllSearchResultBack() {
        return search_monitor_number[0] == -1 && search_monitor_number[1] == -1;
    }

	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {

		long double norm_measurement_interval = time / rtt;
		long double rtt_penalty = beta_ * total * get_rtt(rtt) * total;
		long double utility = 4.5 * ((long double)total - (long double) (alpha_ * pow(loss, 1.2))) / norm_measurement_interval - pow(rtt_penalty, 1.02);
		//cout << "total " << total << ". loss " << loss << " RTT " << get_rtt(rtt) << " rtt cont. " << - beta_ * get_rtt(rtt) << " utility = " << utility << " interval: " << norm_measurement_interval;
		//cout << "RTT = " << rtt_penalty << " utility = " << utility << " total = " << total << endl;

		return utility;

		//long double a = 100;
		//long double thresh = 1.05;
		//double base = 2;
		//long double loss_suffered = //(loss - thresh * total) / norm_measurement_interval;
		//long double penelty = alpha_ * pow(base, a * (loss_rate - thresh));
		//long double computed_utility = packets_recieved - penelty;

		//cout << "Utility " << computed_utility << ". loss_rate = " << loss_rate << ". Loss = " << loss << ". packets received = " << packets_recieved << ". penelty = " << penelty << endl;
		//return computed_utility;
	}

	static const double kMinRateMbps = 0.5;
	static const long kMillisecondsDigit = 10 * 1000;

	enum ConnectionState {
		START,
		SEARCH,
		DECISION
	} state_;

	int monitor_in_start_phase_;
	double slow_start_factor_;
	double alpha_;
	double beta_;
	double rate_;
	double previous_rtt_;
	unsigned long previous_loss_;
	int monitor_in_prog_;
	double previous_utility_;
	long double utility_sum_;
	size_t no_loss_count_;
	size_t measurement_intervals_;
	pthread_mutex_t mutex_;
	long double prev_utility_;
	bool continue_slow_start_;
};

#endif
