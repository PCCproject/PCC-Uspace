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
			setRate(rate() * 2);
		} else if (state_ == SEARCH) {
			search();
			state_ = DECISION;
			monitor_in_prog_ = current_monitor;
		}
	}
	
	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {
		if (endMonitor != monitor_in_prog_) return;
		long double curr_utility = utility(total, loss, in_time, rtt);

		utility_sum_ += curr_utility;
		measurement_intervals_++;

		if(state_ == START) {
			if (!slow_start(curr_utility, loss)) {
				setRate(rate()/2);
				state_ = SEARCH;
			}			
		} else if (state_ == DECISION) {			
			prev_utilities_.push_back(curr_utility);
			prev_rates_.push_back(rate());

			if (prev_utilities_.size() > kHistorySize) {
				prev_utilities_.pop_front();
				prev_rates_.pop_front();
			}

			decide(curr_utility);
			state_ = SEARCH;
		}
		monitor_in_prog_ = -1;
	}
	
protected:
	deque<long double> prev_utilities_;
	deque<long double> prev_rates_;
	static const double kMaxProj = 5;

	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC(double proj_alpha, double proj_beta) : state_(START), proj_alpha_(proj_alpha), proj_beta_(proj_beta), rate_(5.0), previous_rtt_(0), 
			monitor_in_prog_(-1), previous_utility_(-1000000), utility_sum_(0), measurement_intervals_(0) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
		srand(time(NULL));
	}

	virtual void setRate(double mbps) {
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;	
	}
	
	double rate() const { return rate_; }

	double project(long double utility_diff) {
		double projection = proj_alpha_ * (2 * atan(proj_beta_ * utility_diff)) / M_PI;
		if ((projection > 0) && (projection > kMaxProj)) return kMaxProj;
		if ((projection < 0) && (projection < -1 * kMaxProj)) return (-1 * kMaxProj);
		return projection;
	}

private:	
	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {
		if (previous_rtt_ == 0) previous_rtt_ = rtt;
		//long double computed_utility = ((total-loss)/time*(1-1/(1+exp(-100*(double(loss)/total-0.05))))* (1-1/(1+exp(-1*(1-previous_rtt_/rtt)))) -1*double(loss)/time)/rtt*1000;

		long double throughput = (((long double) total) - ((long double) loss)) / time;
		long double send_rate = ((long double) total) / time;
		long double a = 1;
		long double b = 1.05;
		double base = 1.03;

		long double computed_utility = throughput - 10 * pow(base, a * (send_rate - b * throughput)) + 10;
		computed_utility /= 100;
		previous_rtt_ = rtt;
		return computed_utility;
	}
	
	void clear_after_fallback() {
		while (prev_utilities_.size() > kFallbackIndex) {
			prev_utilities_.pop_back();
			prev_rates_.pop_back();
		}
	}

	
	bool slow_start(double curr_utility, unsigned long loss) {
		if (previous_utility_ > curr_utility) { return false; }
		if (loss > 0) { return false; }

		previous_utility_ = curr_utility;
		return true;
	}

	static const double kMinRateMbps = 0.01;
	static const size_t kHistorySize = 9;
	static const size_t kFallbackIndex = 8;

	enum ConnectionState {
		START,
		SEARCH,
		DECISION
	} state_;

	double proj_alpha_;
	double proj_beta_;

	double rate_;
	double previous_rtt_;
	int monitor_in_prog_;
	double previous_utility_;
	
	long double utility_sum_;
	size_t measurement_intervals_;
};

#endif
