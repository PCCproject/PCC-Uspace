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
#include <thread>
#include <chrono>

using namespace std;
using namespace std::chrono;

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
            if(search_number == kHistorySize) {
			    search_number = 0;
                state_ = DECISION;
            }
		}
		pthread_mutex_unlock(&mutex_);
	}

	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {
		pthread_mutex_lock(&mutex_);
		rtt /= (1000);
		if (rtt == 0) rtt = 0.0001;
		if (previous_rtt_ == 0) previous_rtt_ = rtt;

		rtt_history_.push_back(rtt);
		if (rtt_history_.size() > 10) {
			rtt_history_.erase (rtt_history_.begin());
		}

		long double curr_utility = utility(total, loss, in_time, rtt);
		last_measurement_timestamp_ = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

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
					if (slow_start_factor_ < 2) {
						cout << "Slow Start: Done! new = " << curr_utility << " prev " << tmp_prev_utility << endl;
						state_ = SEARCH;
					}
				}
			}
		} else if (state_ == DECISION) {
			for (int i = 0; i < kHistorySize ; i++) {
	           if(endMonitor == search_monitor_number[i]) {
    	           search_monitor_number[i] = -1;
        	       search_monitor_utility[i] = curr_utility;
	           }
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
	static const int kHistorySize = 2 * 1;
	bool conditions_changed_too_much_;
    long double search_monitor_utility[kHistorySize ];
    int search_monitor_number[kHistorySize ];
    int search_number;
	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC(double alpha, bool latency_mode) : conditions_changed_too_much_(false), state_(START), monitor_in_start_phase_(-1), slow_start_factor_(2), alpha_(alpha), rate_(kMinRateMbps), previous_rtt_(0),
			monitor_in_prog_(-1), utility_sum_(0), no_loss_count_(0), measurement_intervals_(0), prev_utility_(-10000000), continue_slow_start_(true), timeout_monitor_(&PCC::timeout_handler, this) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
        search_number = 0;
		setRTO(100000000);
		srand(time(NULL));

		last_measurement_timestamp_ = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

		if (!latency_mode) {
			beta_ = 0;
		} else {
			beta_ = 50; 
		}
		beta_ = 50;
		for (int i = 0; i < kHistorySize; i++) {
			search_monitor_number[i] = -1;
		}

	}
 
	virtual void setRate(double mbps) {
		//cout << "rate = " << mbps << endl;
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
	}

	double rate() const { return rate_; }

	virtual void reduce_rate(double reduce_factor) {
		//cout << "saving the day " << rate();
		setRate(reduce_factor * rate());
		//cout << " --> " << rate() << endl;
	}

private:
	static double get_rtt(double rtt) {
		return rtt;
		double conv_diff = (double)(((long) (rtt * 1000 * 1000)) % kMillisecondsDigit);
		return conv_diff / (1000.0 * 1000.0);
	}

    double isAllSearchResultBack() {
		for (int i = 0; i < kHistorySize; i++) {
			if (search_monitor_number[i] != -1) return false;
		}
		return true;
    }

	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {

		long double norm_measurement_interval = time / rtt;
		long double rtt_penalty = get_rtt(rtt);
		long double utility = 4.5 * ((long double)total - (long double) (alpha_ * pow(loss, 1.2))) / norm_measurement_interval - beta_ * total * rtt_penalty;

		return utility;

	}

	void timeout_handler() {
		while(true) {
			pthread_mutex_lock(&mutex_);
			double rtt_estimate = 0;
			if (rtt_history_.size() >= 10) {

				for (vector<double>::iterator it = rtt_history_.begin() ; it != rtt_history_.end(); ++it) {
					rtt_estimate += *it;
				}
				rtt_estimate /= rtt_history_.size();
				milliseconds now = duration_cast< milliseconds >(system_clock::now().time_since_epoch());

				if ((last_measurement_timestamp_.count() + 4 * rtt_estimate < now.count()) && (state_ != START)) {
					state_ = SEARCH;
					for (int i = 0; i < kHistorySize; i++) search_monitor_number[i] = -1;
					rtt_history_.clear();
					reduce_rate(0.5);
				}
			}
			pthread_mutex_unlock(&mutex_);
			usleep(1);
		}
	}

	static constexpr double kMinRateMbps = 0.5;
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
	thread timeout_monitor_;
	milliseconds last_measurement_timestamp_;
	vector<double> rtt_history_;
};

#endif
