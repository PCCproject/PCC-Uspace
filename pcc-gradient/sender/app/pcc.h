#ifndef __PCC_H__
#define __PCC_H__

#define _USE_MATH_DEFINES

#include <udt.h>
#include <ccc.h>
#include<iostream>
#include<cmath>
#include <deque>
#include <time.h>

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
		if (monitor_in_prog_ != -1) return;
		monitor_in_prog_ = current_monitor;
		
		if (state_ == START) {
			setRate(rate() * slow_start_factor_);
		} else if (state_ == SEARCH) {
			search();
			state_ = DECISION;
			monitor_in_prog_ = current_monitor;
		}
	}
	
	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {
		if (endMonitor != monitor_in_prog_) return;

		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;
		if (previous_rtt_ == 0) previous_rtt_ = rtt;

		long double curr_utility = utility(total, loss, in_time, rtt);

		rtt_changed_too_much_ = (rtt / previous_rtt_ > 1.2) || (previous_rtt_ / rtt > 1.2);
		previous_rtt_ = rtt;

		utility_sum_ += curr_utility;
		measurement_intervals_++;

		if(state_ == START) {
			if (loss > 0) {
				setRate(rate() / slow_start_factor_);
				slow_start_factor_ /= 1.5;
				if (slow_start_factor_ < 1.2) {
					state_ = SEARCH;
				}
			}
		} else if (state_ == DECISION) {			
			decide(curr_utility);
			state_ = SEARCH;
		}
		monitor_in_prog_ = -1;
	}
	
protected:
	enum LatencySensitivity {
		NONE,
		LOW,
		MEDIUM,
		HIGH
	};


	static const double kMaxProj = 1;
	bool rtt_changed_too_much_;
	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC(double alpha, LatencySensitivity latency_mode) : rtt_changed_too_much_(false), state_(START), slow_start_factor_(2), alpha_(alpha), latency_mode_(latency_mode), rate_(5.0), previous_rtt_(0), 
			monitor_in_prog_(-1), utility_sum_(0), measurement_intervals_(0) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
		srand(time(NULL));
		
		switch(latency_mode_) {
			case NONE: 
				beta_ = 0; break;
			case LOW: 
				beta_ = 100; break;
			case MEDIUM: 
				beta_ = 1000; break;
			case HIGH: 
				beta_ = 10000; break;
		}
		beta_ *= 3.2;
	}

	virtual void setRate(double mbps) {
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;	
	}
	
	double rate() const { return rate_; }

private:
	double get_rtt(double rtt) const {
		double conv_diff = (double)(((long) (rtt * 1000 * 1000)) % kMillisecondsDigit);
		return conv_diff / (1000.0 * 1000.0);
	}
	
	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {

		//long double computed_utility = ((total-loss)/time*(1-1/(1+exp(-100*(double(loss)/total-0.05))))* (1-1/(1+exp(-1*(1-previous_rtt_/rtt)))) -1*double(loss)/time)/rtt*1000;
		
		long double norm_measurement_interval = time / rtt;
		long double utility = ((long double)total - (long double) (alpha_ * loss)) / norm_measurement_interval - beta_ * get_rtt(rtt);
		//cout << "total " << total << ". loss " << loss << " RTT " << get_rtt(rtt) << " rtt cont. " << - beta_ * get_rtt(rtt) << " utility = " << utility << " interval: " << norm_measurement_interval;
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
		
	static const double kMinRateMbps = 0.01;
	static const size_t kHistorySize = 9;
	static const size_t kFallbackIndex = 8;
	static const long kMillisecondsDigit = 10 * 1000;
	static const long kMaxTransRate = 1 << 16;
	//static const long kMaxLoss = ;

	enum ConnectionState {
		START,
		SEARCH,
		DECISION
	} state_;

	double slow_start_factor_;
	double alpha_;
	double beta_;
	LatencySensitivity latency_mode_;
	double rate_;
	double previous_rtt_;
	int monitor_in_prog_;
	double previous_utility_;
	long double utility_sum_;
	size_t measurement_intervals_;
};

#endif
