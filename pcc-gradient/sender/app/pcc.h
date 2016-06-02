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
//#define DEBUG_PRINT

using namespace std;

bool kInTimeout = false;

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
	virtual bool onTimeout(int total, int loss, double in_time, int current, int endMonitor, double rtt){ 
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
		//state_ = SEARCH;
	
		kInTimeout = true;
		long double curr_utility = utility(total, 0, in_time, rtt);
		//#ifdef DEBUG_PRINT
		cout << "computing utility: total = " << total << ", loss = " << loss << " in_time = " << in_time << ", rtt = " << rtt << endl;
		cout << "current utility = " << curr_utility << " and previous utility = " << last_utility_ << endl;
		cout << "current rate " << rate() << " --> ";
		decide(last_utility_, curr_utility, true);
		//#endif
		
		
		
		//setRate(0.75 * rate());
		//base_rate_ = rate();
		#ifdef DEBUG_PRINT
			cout << "timeout! new rate is " << rate() << endl;
		#endif
		restart();
		//clear_state();
		//start_measurment_map_.clear();
		//end_measurment_map_.clear();
		kInTimeout = false;
		cout << "new rate: " << rate() << endl;
		return false;
	}
	virtual void onACK(const int& ack){}

	virtual void onMonitorStart(int current_monitor) {
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
				start_measurment_map_.insert(pair<int,unique_ptr<Measurement> >(current_monitor, unique_ptr<Measurement>(new Measurement(base_rate_))));
				current_start_monitor_ = current_monitor;
			} else {
				if (start_measurment_map_.find(current_start_monitor_) != start_measurment_map_.end()) {
					end_measurment_map_.insert(pair<int,unique_ptr<Measurement> >(current_monitor, unique_ptr<Measurement>(new Measurement(base_rate_,current_start_monitor_))));
					start_measurment_map_.at(current_start_monitor_)->other_monitor_ = current_monitor;
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

	virtual void onMonitorEnds(int total, int loss, double in_time, int current, int endMonitor, double rtt) {
		/*
		if (rtt == -1) {
			start_measurment_map_.clear();
			end_measurment_map_.clear();
			setrate(total/(timeout - 0.5 * m_iRTT))
			return;
		}
		*/
		
		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;

		long double curr_utility = utility(total, loss, in_time, rtt);
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

				//double start_rtt = 0;
				//double start_loss = 0;
				double start_base = 0;
				double end_base = 1;
				//double end_rtt = 0;
				//double end_loss = 0; 

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


						//start_rtt = start_measurment_map_.at(other_monitor)->rtt_;
						//start_loss = start_measurment_map_.at(other_monitor)->loss_;
						//end_rtt = end_measurment_map_.at(endMonitor)->rtt_;
						//end_loss = end_measurment_map_.at(endMonitor)->loss_;
						//delete start_measurment_map_.at(other_monitor);
						start_measurment_map_.erase(other_monitor);
					}

					//delete end_measurment_map_.at(endMonitor);
					end_measurment_map_.erase(endMonitor);
				}

				//bool contidutions_changed_too_much = false;
				//if ((end_rtt > 1.15 * start_rtt) || (start_rtt > 1.15 * end_rtt)) contidutions_changed_too_much = true;
				//if (((end_loss > 10 * start_loss) && (end_loss > 10) ) || ((start_loss > 10 * end_loss) && (start_loss > 10) )) contidutions_changed_too_much = true;

				if (start_base == end_base) {
					decide(start_utility, end_utility, false);
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
	static constexpr double kMinRateMbps = 0.2;
	static constexpr double kMaxRateMbps = 1024.0;
	

	virtual void search() = 0;
	virtual void decide(long double start_utility, long double end_utility, bool force_change) = 0;
	
	virtual void clear_state() {
		continue_slow_start_ = true;
		start_measurement_ = true;
		slow_start_factor_ = 1.2;
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
			alpha_(kAlpha), beta_(kBeta), exponent_(kExponent), poly_utlity_(kPolyUtility), rate_(kMinRateMbps), monitor_in_prog_(-1), utility_sum_(0), measurement_intervals_(0), prev_utility_(-10000000), continue_slow_start_(true), last_utility_(0) {
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
		//cout << "set rate: " << rate_ << " --> " << mbps << endl;
		if (mbps < kMinRateMbps) { 
			#ifdef DEBUG_PRINT
				cout << "rate is mimimal, changing to " << kMinRateMbps << " instead" << endl;
			#endif
			mbps = kMinRateMbps; 
		} else if (mbps > kMaxRateMbps) {
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


	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {
		static long double last_measurement_interval = 1;
		
		long double norm_measurement_interval = last_measurement_interval;
		
		if (!kInTimeout) {
			norm_measurement_interval = time / rtt;
			last_measurement_interval = norm_measurement_interval;
		}
		
		// convert to milliseconds
		long double rtt_penalty_1 = 1000 * rtt;
		unsigned long rtt_penalty = rtt_penalty_1 / 5;
		long double utility;
			//cout <<"RTT: " << rtt_penalty << endl;
		//static long double previous_utility;
		exponent_ = 2.5;
		
	
		 utility = ((long double)total - total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1))) / norm_measurement_interval - 3 * pow(rtt_penalty,1);//0.01 * total *pow(rtt_penalty, 1.4);
		 
		 if (kInTimeout) {
			cout << "utility components:" << endl;
			cout << "total = " << total << endl;
			cout << "loss = " << -1 * total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1)) << endl;
			cout << "RTT = " << - pow(rtt_penalty,0.1) << endl;
			cout << "computed utility = " << utility << endl;
		}


/*
		if (poly_utlity_) {
		 	utility = ((long double)total - total * (long double) (alpha_* (pow((1+((long double)((double) loss/(double) total))), exponent_)-1))) / norm_measurement_interval - 0.01 * total *pow(rtt_penalty, 1.2);
			//cout << "Total: " << total << " RTT part:" << rtt << "  utility: " << utility << " total = " << total << endl;
		} else {
			utility = (total - loss - (long double) (alpha_ * pow(2.3, loss))) / norm_measurement_interval - 10 * total * pow(1000 * rtt, 1.15);
			//((long double)total - total * (long double) (alpha_ * pow(exponent_, 1 + ((long double)((double) loss/(double) total))))) / norm_measurement_interval - beta_ * total * pow(rtt_penalty, 1.02);
		}
		//previous_utility = utility;
*/	
		return utility;
	}

	static const long kMillisecondsDigit = 10 * 1000;

	enum ConnectionState {
		START,
		SEARCH
	} state_;
	
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
	map<int, unique_ptr<Measurement> > start_measurment_map_;
	map<int, unique_ptr<Measurement> > end_measurment_map_;
	int current_start_monitor_;
	long double last_utility_;
};

#endif