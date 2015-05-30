#ifndef __PCC_H__
#define __PCC_H__

#include <udt.h>
#include <ccc.h>
#include<iostream>
#include<cmath>
#include <deque>
#include <time.h>

using namespace std;

double rate_limit = 1024;

void set_rate_limit(double new_limit) { rate_limit = new_limit; }

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
			if ((prev_utilities_.size() >= kFallbackIndex + 1) && (prev_utilities_[kFallbackIndex] > 2 * curr_utility)) {
				double new_rate = max<double>(0.85 * rate(), rate() - kMaxChangeMbpsDown);
				setRate(new_rate);
				monitor_in_prog_ = -1;
				state_ = SEARCH;
				prev_utilities_.clear();
				prev_rates_.clear();
				return;			
			}
			
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
	static const double kEpsilon = 0.000035;
	static const double kDelta = 0.00001;
	deque<long double> prev_utilities_;
	deque<long double> prev_rates_;


	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC() : state_(START), link_capacity_(rate_limit), rate_(5.0), previous_rtt_(0), 
			monitor_in_prog_(-1), previous_utility_(-1000000), utility_sum_(0), measurement_intervals_(0) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
	}

	virtual void setRate(double mbps) {
		if (mbps < kMinRateMbps) { mbps = kMinRateMbps; };
		if (mbps > link_capacity_) {
			mbps = link_capacity_;
		}
		if (state_ != START){
			if ((rate_ < mbps) && (rate_ + kMaxChangeMbps < mbps)) {
				mbps = rate_ + kMaxChangeMbps;
			}
			if ((rate_ > mbps) && (rate_ - kMaxChangeMbpsDown > mbps)) {
				mbps = rate_ - kMaxChangeMbpsDown;
			}

		}
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;	
	}
	
	double rate() const { return rate_; }

private:	
	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {
		if (previous_rtt_ == 0) previous_rtt_ = rtt;

		//long double rate = (total-loss)/time;
		//long double loss_rate = double(loss) / double(total);
	//	long double computed_utility = total - total * exp((10 * loss_rate) / 0.05 - 1);
		long double computed_utility = ((total-loss)/time*(1-1/(1+exp(-100*(double(loss)/total-0.005))))* (1-1/(1+exp(-10*(1-previous_rtt_/rtt)))) -1*double(loss)/time)/rtt*1000;
		previous_rtt_ = rtt;
		//cout << "total " << total << " loss " << loss << " utility = " << computed_utility << endl;
		return computed_utility;
	}
	
	bool slow_start(double curr_utility, unsigned long loss) {
		if (previous_utility_ > curr_utility) { return false; }
		if (loss > 0) { return false; }

		previous_utility_ = curr_utility;
		return true;
	}

	static const double kMaxChangeMbps = 0.1;
	static const double kMaxChangeMbpsDown = 6;
	static const double kMinRateMbps = 0.01;
	static const size_t kHistorySize = 10;
	static const size_t kFallbackIndex = 8;

	enum ConnectionState {
		START,
		SEARCH,
		DECISION
	} state_;

	const double link_capacity_;
	double rate_;
	double previous_rtt_;
	int monitor_in_prog_;
	double previous_utility_;
	
	long double utility_sum_;
	size_t measurement_intervals_;
};

#endif
