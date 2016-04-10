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
#include <map>

using namespace std;

class Measurement {
	public:
		Measurement(double base_rate, int other_monitor = -1): utility_(0), base_rate_(base_rate), other_monitor_(other_monitor), set_(false), rtt_(0), loss_(0) {}
		long double utility_;
		double base_rate_;
		int other_monitor_;
		bool set_;
		double rtt_;
		double loss_;
};

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
			setRate(rate() * slow_start_factor_);
		} else if (state_ == HOLD) {
			if (monitor_in_start_phase_ == -1) {
				monitor_in_start_phase_ = current_monitor;
			}
		} else if (state_ == SEARCH) {
			if (start_measurement_) {
				start_measurment_map_.insert(pair<int,Measurement*>(current_monitor, new Measurement(base_rate_)));
				current_start_monitor_ = current_monitor;
			} else {
				if (start_measurment_map_.find(current_start_monitor_) != start_measurment_map_.end()) {
					end_measurment_map_.insert(pair<int,Measurement*>(current_monitor, new Measurement(base_rate_,current_start_monitor_)));
					start_measurment_map_.at(current_start_monitor_)->other_monitor_ = current_monitor;
				} else {
					start_measurment_map_.clear();
					end_measurment_map_.clear();
				}
			}
			search();
			start_measurement_ = !start_measurement_;
		}
		pthread_mutex_unlock(&mutex_);
	}

	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {
		pthread_mutex_lock(&mutex_);
		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;

		long double curr_utility = utility(total, loss, in_time, rtt);

		utility_sum_ += curr_utility;
		measurement_intervals_++;

		bool continue_slow_start = (loss == 0) && (curr_utility > prev_utility_);
		long double tmp_prev_utility = prev_utility_;
		prev_utility_ = curr_utility;
		
		if(state_ == START) {
			if (monitor_in_start_phase_ == endMonitor) {
				monitor_in_start_phase_ = -1;
				if (!continue_slow_start) {
					//cout << "changing to Hold, rate =  " << rate() << endl;
					//cout << "previous utility = " << tmp_prev_utility << ", this utility = " << curr_utility << endl;
					state_ = HOLD;
				}
			}
		} else if(state_ == HOLD) {
			if (monitor_in_start_phase_ == endMonitor) {
				if (!continue_slow_start) {
					hold_count_--;
					if (hold_count_ == 0) {
						setRate(rate() / slow_start_factor_);
						slow_start_factor_ /= 1.2;
						//cout << "slow_start_factor_ = " << slow_start_factor_ << endl; 
						hold_count_ = kInitHoldCount;
						if (slow_start_factor_ < 1) {
							cout << "Slow start done, rate = " << rate() << endl;
							state_ = SEARCH;
						}
					} else {
						//cout << "Holding rate " << rate() << endl;
						prev_utility_ = tmp_prev_utility;
					}
				} else {
					//cout << "slow start back again.." << endl;
					hold_count_ = kInitHoldCount;
					state_ = START;
				}
				monitor_in_start_phase_ = -1;
			}
		} else if(state_ == SEARCH) {
            if(start_measurment_map_.find(endMonitor) != start_measurment_map_.end()) {
				start_measurment_map_.at(endMonitor)->utility_ = curr_utility;
				start_measurment_map_.at(endMonitor)->set_ = true;
				start_measurment_map_.at(endMonitor)->rtt_ = rtt;
				start_measurment_map_.at(endMonitor)->loss_ = loss;
            } else if(end_measurment_map_.find(endMonitor) != end_measurment_map_.end()) {
				end_measurment_map_.at(endMonitor)->utility_ = curr_utility;
				end_measurment_map_.at(endMonitor)->set_ = true;
				end_measurment_map_.at(endMonitor)->rtt_ = rtt;
				end_measurment_map_.at(endMonitor)->loss_ = loss;
            }

            if(isAllSearchResultBack(endMonitor)) {
				int start_utility, end_utility;
				int other_monitor;

				double start_rtt, start_loss, end_rtt, end_loss;
				double start_base, end_base;

				if (start_measurment_map_.find(endMonitor) != start_measurment_map_.end()) {
					start_utility = start_measurment_map_.at(endMonitor)->utility_;
					other_monitor = start_measurment_map_.at(endMonitor)->other_monitor_;
					end_utility = end_measurment_map_.at(other_monitor)->utility_;
					
					start_base = start_measurment_map_.at(endMonitor)->base_rate_;
					end_base = end_measurment_map_.at(other_monitor)->base_rate_;

					start_rtt = start_measurment_map_.at(endMonitor)->rtt_;
					start_loss = start_measurment_map_.at(endMonitor)->loss_;
					end_rtt = end_measurment_map_.at(other_monitor)->rtt_;
					end_loss = end_measurment_map_.at(other_monitor)->loss_;

					
					delete start_measurment_map_.at(endMonitor);
					delete end_measurment_map_.at(other_monitor);
					start_measurment_map_.erase(endMonitor);
					end_measurment_map_.erase(other_monitor);
				} else {
					end_utility = end_measurment_map_.at(endMonitor)->utility_;
					other_monitor = end_measurment_map_.at(endMonitor)->other_monitor_;
					start_utility = start_measurment_map_.at(other_monitor)->utility_;
					
					start_base = start_measurment_map_.at(other_monitor)->base_rate_;
					end_base = end_measurment_map_.at(endMonitor)->base_rate_;


					start_rtt = start_measurment_map_.at(other_monitor)->rtt_;
					start_loss = start_measurment_map_.at(other_monitor)->loss_;
					end_rtt = end_measurment_map_.at(endMonitor)->rtt_;
					end_loss = end_measurment_map_.at(endMonitor)->loss_;


					delete end_measurment_map_.at(endMonitor);
					delete start_measurment_map_.at(other_monitor);
					end_measurment_map_.erase(endMonitor);
					start_measurment_map_.erase(other_monitor);
				}

				bool contidutions_changed_too_much = false;
				if ((end_rtt > 1.15 * start_rtt) || (start_rtt > 1.15 * end_rtt)) contidutions_changed_too_much = true;
				if (((end_loss > 10 * start_loss) && (end_loss > 10) ) || ((start_loss > 10 * end_loss) && (start_loss > 10) )) contidutions_changed_too_much = true;

				if (start_base == end_base) {
					decide(start_utility, end_utility, start_base, contidutions_changed_too_much);
				}
            }
		}
		pthread_mutex_unlock(&mutex_);
	}

	static void set_utility_params(double alpha = 4, double beta = 54, double exponent = 1.5, bool polyUtility = true) {
		kAlpha = alpha;
		kBeta = beta;
		kExponent = exponent;
		kPolyUtility = polyUtility;
	}
	
protected:

	static double kAlpha, kBeta, kExponent;
	static bool kPolyUtility;
	
    long double search_monitor_utility[2];
    int search_monitor_number[2];
    bool start_measurement_;
	double base_rate_;
	static constexpr double kMinRateMbps = 0.01;

	virtual void search() = 0;
	virtual void decide(long double start_utility, long double end_utility, long double base_rate, bool condition_change) = 0;
	
	void restart() {
		cout <<"restart!"<<endl;
		continue_slow_start_ = true;
		start_measurement_ = true;
		slow_start_factor_ = 1.03;
		start_measurment_map_.clear();
		end_measurment_map_.clear();
		state_ = START;
		hold_count_ = kInitHoldCount;
	}
	
	PCC() : start_measurement_(true), base_rate_(5.0), state_(START), hold_count_(kInitHoldCount), monitor_in_start_phase_(-1), slow_start_factor_(2),
			alpha_(kAlpha), beta_(kBeta), exponent_(kExponent), poly_utlity_(kPolyUtility), rate_(5.0), monitor_in_prog_(-1), utility_sum_(0), measurement_intervals_(0), prev_utility_(-10000000), continue_slow_start_(true) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
		srand(time(NULL));
		cout << "configuration: alpha = " << alpha_ << ", beta = " << beta_   << ", exponent = " << exponent_ << " poly utility = " << poly_utlity_ << endl;
		
		/*
		if (!latency_mode) {
			beta_ = 0;
		} else {
			beta_ = 50;
		}
		*/
	}

	virtual void setRate(double mbps) {
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
	}

	double rate() const { return rate_; }

private:
	static double get_rtt(double rtt) {
		double conv_diff = (double)(((long) (rtt * 1000 * 1000)) % kMillisecondsDigit);
		return conv_diff / (1000.0 * 1000.0);
	}

    double isAllSearchResultBack(int current_monitor) {
		if ((start_measurment_map_.find(current_monitor) != start_measurment_map_.end()) && (start_measurment_map_.at(current_monitor)->set_)) {
			int other_monitor = start_measurment_map_.at(current_monitor)->other_monitor_;
			return ((end_measurment_map_.find(other_monitor) != end_measurment_map_.end()) && (end_measurment_map_.at(other_monitor)->set_));
		} else if ((end_measurment_map_.find(current_monitor) != end_measurment_map_.end()) && (end_measurment_map_.at(current_monitor)->set_)) {
			int other_monitor = end_measurment_map_.at(current_monitor)->other_monitor_;
			return ((start_measurment_map_.find(other_monitor) != start_measurment_map_.end()) && (start_measurment_map_.at(other_monitor)->set_));
		}
		return false;
    }


	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {

		long double norm_measurement_interval = time / rtt;
		long double rtt_penalty = get_rtt(rtt);
		long double utility;		
		//static long double previous_utility;
		if (poly_utlity_) {
		 	utility = ((long double)total - total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1))) / norm_measurement_interval - beta_ * total * pow(rtt_penalty, 1.02);
		} else {
			utility = (total - loss - (long double) (alpha_ * pow(2.3, loss))) / norm_measurement_interval - 10 * total * pow(1000 * rtt, 1.15);
			//((long double)total - total * (long double) (alpha_ * pow(exponent_, 1 + ((long double)((double) loss/(double) total))))) / norm_measurement_interval - beta_ * total * pow(rtt_penalty, 1.02);
		}
		/*if (loss > 0) {
			cout << previous_utility << " --> " << utility << " at rate: " << rate() << endl;
		}*/
		//previous_utility = utility;
		
		return utility;
	}

	static constexpr long kMillisecondsDigit = 10 * 1000;

	enum ConnectionState {
		START,
		HOLD,
		SEARCH
	} state_;
	static constexpr int kInitHoldCount = 2;
	int hold_count_;
	
	int monitor_in_start_phase_;
	double slow_start_factor_;
	double alpha_;
	double beta_;
	double exponent_;
	bool poly_utlity_;
	double rate_;
	int monitor_in_prog_;
	pthread_mutex_t mutex_;
	long double utility_sum_;
	size_t measurement_intervals_;
	long double prev_utility_;
	bool continue_slow_start_;
	map<int, Measurement*> start_measurment_map_;
	map<int, Measurement*> end_measurment_map_;
	int current_start_monitor_;
};

#endif
