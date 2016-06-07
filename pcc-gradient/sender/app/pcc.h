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
//#define DEBUG_PRINT

using namespace std;

bool kInTimeout = false;

enum MeasurementType {
	FIRST,
	SECOND
};

class Measurement {
	public:
		Measurement(double base_rate, int other_monitor, double test_rate, MeasurementType t, int monitor_number): utility_(0), base_rate_(base_rate), other_monitor_(other_monitor), set_(   false), rtt_(0), loss_(0), loss_panelty_(0), rtt_panelty_(0), test_rate_(test_rate), type_(t), monitor_number_(monitor_number) {}
		
		Measurement* copy() const {
			Measurement* ret = new Measurement(base_rate_, other_monitor_, test_rate_, type_, monitor_number_);
			ret->utility_ = utility_;
			ret->rtt_ = rtt_;
			ret->loss_ = loss_;
			ret->loss_panelty_ = loss_panelty_;
			ret->actual_packets_sent_rate_ = actual_packets_sent_rate_;
			return ret;
		}
		
		long double utility_;
		double base_rate_;
		int other_monitor_;
		bool set_;
		double rtt_;
		double loss_;
		double loss_panelty_;
		double rtt_panelty_;
		double actual_packets_sent_rate_;
		double test_rate_;
		MeasurementType type_;
		int monitor_number_;
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
	virtual bool onTimeout(int total, int loss, double in_time, int current, int endMonitor, double rtt){ 
		lock_guard<mutex> lck(monitor_mutex_);
		//cout << "handling timeout in PCC! for monitor " << monitor << endl;
		if (state_ != START) {
			if (start_measurment_map_.find(endMonitor) == start_measurment_map_.end() && end_measurment_map_.find(endMonitor) == end_measurment_map_.end()) {
				#ifdef DEBUG_PRINT
					cout << "NOT IN START: monitor " << endMonitor << " already gone!" << endl;
				#endif
				return false;
			}
		} /*else if (monitor_in_start_phase_ != monitor) {
			cout << "START: monitor " << monitor << " already gone! current monitor: " << monitor_in_start_phase_ << endl;
			return false;			
		}*/
		
	
		kInTimeout = true;
		long double curr_utility = utility(total, 0, in_time, rtt, NULL);
		if (curr_utility > last_utility_) {
			last_utility_ = curr_utility;
			return true;
		}
		//#ifdef DEBUG_PRINT
		cout << "computing utility: total = " << total << ", loss = " << loss << " in_time = " << in_time << ", rtt = " << rtt << endl;
		cout << "current utility = " << curr_utility << " and previous utility = " << last_utility_ << endl;
		cout << "current rate " << rate() << " --> ";
		//#endif
		decide(last_utility_, curr_utility, true);
		
		double r = rate();
		//#ifdef DEBUG_PRINT
			cout << "timeout! new rate is " << r << endl;
		//#endif
		restart(); 
		state_ = SEARCH;
		
		if (r < 1.01 * kMinRateMbps) {
			cout << "going to bed" << endl;
			this_thread::sleep_for(std::chrono::seconds(5));
			cout << "done sleeping" << endl;
		}
		
		/*
		if (r > 1.01 * kMinRateMbps) {
			cout << "going to SEARCH rate = " << rate() << ". Thresh = " << 1.01 * kMinRateMbps << endl;
			state_ = SEARCH;
		} else {
			cout << "going to "<< kMinRateMbpsSlowStart << "mbps" << endl;
			base_rate_ = kMinRateMbpsSlowStart;
			restart();
			slow_start_factor_ = 2;
			state_ = START;
			setRate(base_rate_);
			lock_guard<mutex> lck(data_lock_);
			this_thread::sleep_for(std::chrono::seconds(1));
			cout << "done sleeping;"
		}
		*/
		kInTimeout = false;
		return false;
	}
	virtual void onACK(const int& ack){}

	void keep_last_measurement(Measurement* measurement) {
		if (measurement->type_ == FIRST) {
			start_measurment_map_.insert(pair<int,shared_ptr<Measurement> >(measurement->monitor_number_, shared_ptr<Measurement>(measurement)));
			current_start_monitor_ = measurement->monitor_number_;
			start_measurement_ = false;
		} else {
			end_measurment_map_.insert(pair<int,shared_ptr<Measurement> >(measurement->monitor_number_, shared_ptr<Measurement>(measurement)));
			on_next_start_bind_to_end_ = measurement->monitor_number_;
			start_measurement_ = true;
		}
	}
	
	virtual void onMonitorStart(int current_monitor) {
		lock_guard<mutex> lck(monitor_mutex_);
		//cout << "starting monitor " << current_monitor << endl;
		if (state_ == START) {
			if (monitor_in_start_phase_ != -1) {
				return;
			}
			monitor_in_start_phase_ = current_monitor;
			setRate(rate() * slow_start_factor_);
			//cout << "doubling the rate --> " << rate() << endl;
		} else if (state_ == SEARCH) {
			if (start_measurement_) {
				if (start_measurment_map_.find(current_monitor) == start_measurment_map_.end()) {
					start_measurment_map_.insert(pair<int,shared_ptr<Measurement> >(current_monitor, shared_ptr<Measurement>(new Measurement(base_rate_, on_next_start_bind_to_end_, rate(), FIRST, current_monitor))));
					current_start_monitor_ = current_monitor;
					
					on_next_start_bind_to_end_ = -1;
				}
			} else {
				if (start_measurment_map_.find(current_start_monitor_) != start_measurment_map_.end()) {
					if (end_measurment_map_.find(current_monitor) == end_measurment_map_.end()) {
						end_measurment_map_.insert(pair<int,shared_ptr<Measurement> >(current_monitor, shared_ptr<Measurement>(new Measurement(base_rate_, current_start_monitor_, rate(), SECOND, current_monitor))));
						start_measurment_map_.at(current_start_monitor_)->other_monitor_ = current_monitor;
					}
				} else {
					start_measurment_map_.clear();
					end_measurment_map_.clear();
					start_measurement_ = true;
					return;
				}
			}
			search();
			start_measurement_ = !start_measurement_;
		}
	}
	
	Measurement* get_monitor_measurement(int monitor) {
		if (start_measurment_map_.find(monitor) != start_measurment_map_.end()) {
			return start_measurment_map_.at(monitor).get();
		}

		if (end_measurment_map_.find(monitor) != end_measurment_map_.end()) {
			return end_measurment_map_.at(monitor).get();
		}
		return NULL;
	}
	
	virtual void onMonitorEnds(int total, int loss, double in_time, int current, int endMonitor, double rtt) {
		lock_guard<mutex> lck(monitor_mutex_);
		Measurement* this_measurement = get_monitor_measurement(endMonitor);
		if ((this_measurement == NULL) && (state_ != START)) {
			return;
		}
	
		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;
		
		long double curr_utility = utility(total, loss, in_time, rtt, this_measurement);
		last_utility_ = curr_utility;
		utility_sum_ += curr_utility;
		measurement_intervals_++;

		bool continue_slow_start = (curr_utility > prev_utility_);
		//long double tmp_prev_utility = prev_utility_;
		prev_utility_ = curr_utility;
		
		if(state_ == START) {
			if (monitor_in_start_phase_ == endMonitor) {
				monitor_in_start_phase_ = -1;
				if (!continue_slow_start) {
					setRate(rate() / slow_start_factor_);
					state_ = SEARCH;
						//#ifdef DEBUG_PRINT
						//cout << "exit slow start, rate =  " << rate() << endl;
						//#endif
					//cout << "previous utility = " << tmp_prev_utility << ", this utility = " << curr_utility << endl;
				} /*else {
					cout << "current rate: " << rate() << " current utility " << curr_utility << " going forward." << endl; 
				}*/
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
				int start_utility = 0;
				int end_utility = 0;
				int other_monitor;

				double start_base = 0;
				double end_base = 1;
				bool check_result = false;
				Measurement* private_copy = NULL;
				if (start_measurment_map_.find(endMonitor) != start_measurment_map_.end()) {
					start_utility = start_measurment_map_.at(endMonitor)->utility_;
					other_monitor = start_measurment_map_.at(endMonitor)->other_monitor_;
					
					if (end_measurment_map_.find(other_monitor) != end_measurment_map_.end()) {
						end_utility = end_measurment_map_.at(other_monitor)->utility_;
						
						start_base = start_measurment_map_.at(endMonitor)->base_rate_;
						end_base = end_measurment_map_.at(other_monitor)->base_rate_;

						//start_rtt = start_measurment_map_.at(endMonitor)->rtt_;
						//start_loss = start_measurment_map_.at(endMonitor)->loss_;
						//end_rtt = end_measurment_map_.at(other_monitor)->rtt_;
						//end_loss = end_measurment_map_.at(other_monitor)->loss_;

						//delete end_measurment_map_.at(other_monitor);
						
						check_result = sanety_check(start_measurment_map_.at(endMonitor).get(), end_measurment_map_.at(other_monitor).get());
						if (!check_result) private_copy = this_measurement->copy();
						
						end_measurment_map_.erase(other_monitor);
					}
					//delete start_measurment_map_.at(endMonitor);
					start_measurment_map_.erase(endMonitor);
					
				} else {
					end_utility = end_measurment_map_.at(endMonitor)->utility_;
					other_monitor = end_measurment_map_.at(endMonitor)->other_monitor_;
					
					if (start_measurment_map_.find(other_monitor) != start_measurment_map_.end()) {
						start_utility = start_measurment_map_.at(other_monitor)->utility_;
						
						start_base = start_measurment_map_.at(other_monitor)->base_rate_;
						end_base = end_measurment_map_.at(endMonitor)->base_rate_;

						check_result = sanety_check(start_measurment_map_.at(other_monitor).get(), end_measurment_map_.at(endMonitor).get());
						if (!check_result) private_copy = this_measurement->copy();
						
						start_measurment_map_.erase(other_monitor);
					}

					//delete end_measurment_map_.at(endMonitor);
					end_measurment_map_.erase(endMonitor);
				}

				//bool contidutions_changed_too_much = false;
				//if ((end_rtt > 1.15 * start_rtt) || (start_rtt > 1.15 * end_rtt)) contidutions_changed_too_much = true;
				//if (((end_loss > 10 * start_loss) && (end_loss > 10) ) || ((start_loss > 10 * end_loss) && (start_loss > 10) )) contidutions_changed_too_much = true;
				/*
				if (!check_result) {
					cout << "sanety check failed!" << endl;
				}
				*/
				
				if (start_base == end_base) {
					if (check_result) {
						decide(start_utility, end_utility, false);
					} else {
						keep_last_measurement(private_copy);
					}
				}
            }
		}
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
	bool kPrint;
	static constexpr double kMinRateMbps = 0.5;
	static constexpr double kMinRateMbpsSlowStart = 0.03;
	static constexpr double kMaxRateMbps = 1024.0;

	enum ConnectionState {
		START,
		SEARCH
	} state_;
	

	virtual void search() = 0;
	virtual void decide(long double start_utility, long double end_utility, bool force_change) = 0;
	
	virtual void clear_state() {
		continue_slow_start_ = true;
		start_measurement_ = true;
		slow_start_factor_ = 1.1;
		start_measurment_map_.clear();
		end_measurment_map_.clear();
		state_ = SEARCH;
		monitor_in_start_phase_ = -1;
		prev_utility_ = -10000000;
		kPrint = false;
	}

	virtual void restart() {
		continue_slow_start_ = true;
		start_measurement_ = true;
		slow_start_factor_ = 1.2;
		start_measurment_map_.clear();
		end_measurment_map_.clear();
		monitor_in_start_phase_ = -1;
		setRate(base_rate_);
		kPrint = false;
		state_ = START;
		prev_utility_ = -10000000;
	}
	
	PCC() : start_measurement_(true), base_rate_(kMinRateMbps), kPrint(false), state_(START), monitor_in_start_phase_(-1), slow_start_factor_(2),
			alpha_(kAlpha), beta_(kBeta), exponent_(kExponent), poly_utlity_(kPolyUtility), rate_(kMinRateMbps), monitor_in_prog_(-1), utility_sum_(0), measurement_intervals_(0), prev_utility_(-10000000), continue_slow_start_(true), last_utility_(0), on_next_start_bind_to_end_(-1) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
		srand(time(NULL));
		cout << "new Code!!!" << endl;
		cout << "configuration: alpha = " << alpha_ << ", beta = " << beta_   << ", exponent = " << exponent_ << " poly utility = " << poly_utlity_ << endl;
		
		/*
		if (!latency_mode) {
			beta_ = 0;
		} else {
			beta_ = 50;
		}
		*/
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
	virtual void setRate(double mbps) {
		//cout << "set rate: " << rate_ << " --> " << mbps << endl;
		if (state_ == START) {
			if (mbps < kMinRateMbpsSlowStart){ 
				#ifdef DEBUG_PRINT
					cout << "rate is mimimal at slow start, changing to " << kMinRateMbpsSlowStart << " instead" << endl;
				#endif
				mbps = kMinRateMbpsSlowStart; 
			} 
		} else if (mbps < kMinRateMbps){ 
			#ifdef DEBUG_PRINT
				cout << "rate is mimimal, changing to " << kMinRateMbps << " instead" << endl;
			#endif
			mbps = kMinRateMbps; 
		} 
		
		if (mbps > kMaxRateMbps) {
			mbps = kMaxRateMbps;
			cout << "rate is maximal, changing to " << kMaxRateMbps << " instead" << endl;
		}
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
		//cout << "setting rate: mbps = " << mbps << endl;
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

	double get_min_rtt() const {
		double min = *rtt_history_.cbegin();
		for (deque<double>::const_iterator it = rtt_history_.cbegin(); it!=rtt_history_.cend(); ++it) {
			if (min > *it) {
				min = *it;
			}
		}
		return min;
	}

	bool sanety_check(Measurement* start, Measurement* end) {
		
		if (end->test_rate_ < start->test_rate_) {
			//cout << "swapping. Rates: " << start->test_rate_ << ", " << end->test_rate_ << endl; 
			Measurement* swap_temp = start;
			start = end;
			end = swap_temp;
		}
		
		if (start->loss_panelty_ < end->loss_panelty_) { 
			//cout << "failed on loss. Start = " << start->loss_panelty_ << ". End = " << end->loss_panelty_ << endl;
			return false;
		}
		if (start->rtt_panelty_ < end->rtt_panelty_) {
			//cout << "failed on rtt" << endl;
			return false;
		}
		if (start->actual_packets_sent_rate_ < end->actual_packets_sent_rate_) {
			//cout << "failed on packets sent" << endl;
			return false;
		}
		return true;
	}

	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt, Measurement* out_measurement) {
		static long double last_measurement_interval = 1;
		
		long double norm_measurement_interval = last_measurement_interval;
		
		if (!kInTimeout) {
			norm_measurement_interval = time / rtt;
			last_measurement_interval = norm_measurement_interval;
		}
		
		rtt_history_.push_front(rtt);
		if (rtt_history_.size() > kHistorySize) {
			rtt_history_.pop_back();
		}

		// convert to milliseconds
		double rtt_penalty = rtt / get_min_rtt();
		if (rtt_penalty > 4) rtt_penalty  = 4;
		exponent_ = 2.5;
	
		long double loss_contribution = total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1));
		long double rtt_contribution = 2 * total*(pow(rtt_penalty,1.6) - 1); 
		long double utility = ((long double)total - loss_contribution - rtt_contribution)/norm_measurement_interval;
	
		if (out_measurement != NULL) {
			out_measurement->loss_panelty_ = loss_contribution / norm_measurement_interval;
			out_measurement->rtt_panelty_ = rtt_contribution / norm_measurement_interval;
			out_measurement->actual_packets_sent_rate_ = total / norm_measurement_interval;
		}
		return utility;
	}

	static const long kMillisecondsDigit = 10 * 1000;
	
	int monitor_in_start_phase_;
	double slow_start_factor_;
	double alpha_;
	double beta_;
	double exponent_;
	bool poly_utlity_;
	double rate_;
	int monitor_in_prog_;
	long double utility_sum_;
	size_t measurement_intervals_;
	long double prev_utility_;
	bool continue_slow_start_;
	map<int, shared_ptr<Measurement> > start_measurment_map_;
	map<int, shared_ptr<Measurement> > end_measurment_map_;
	int current_start_monitor_;
	long double last_utility_;
	deque<double> rtt_history_;
	static constexpr size_t kHistorySize = 10;
	mutex monitor_mutex_;
	int on_next_start_bind_to_end_;
};

#endif
