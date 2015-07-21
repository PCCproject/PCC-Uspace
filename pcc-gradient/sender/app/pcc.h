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
		if (state_ == START) {
			if (monitor_in_start_phase_ != -1) return;
			setRate(rate() * slow_start_factor_);
			monitor_in_start_phase_ = current_monitor;
		} else if (state_ == SEARCH) {
			search();
            search_monitor_number[search_number] = current_monitor;
            search_number ++;
            if(search_number == 2) {
			    search_number = 0;
                state_ = DECISION;
            }
		}
	}

	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {

		rtt /= (1000 * 1000);
		if (rtt == 0) rtt = 0.0001;
		if (previous_rtt_ == 0) previous_rtt_ = rtt;

		long double curr_utility = utility(total, loss, in_time, rtt);

		conditions_changed_too_much_ = (rtt / previous_rtt_ > 1.15) || (previous_rtt_ / rtt > 1.15);
		conditions_changed_too_much_ = conditions_changed_too_much_ || ((previous_loss_ * 10 < loss) && (previous_loss_ > 0));
		previous_rtt_ = rtt;
		previous_loss_ = loss;

		utility_sum_ += curr_utility;
		measurement_intervals_++;

		if(state_ == START) {
			if (monitor_in_start_phase_ == endMonitor) {
				if (loss > 0) {
					setRate(rate() / slow_start_factor_);
					slow_start_factor_ /= 1.5;
					if (slow_start_factor_ < 1.2) {
						state_ = SEARCH;
					}
				}
				monitor_in_start_phase_ = -1;
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
            }
		}
	}

protected:
	static const double kMaxProj = 1;
	bool conditions_changed_too_much_;
    long double search_monitor_utility[2];
    int search_monitor_number[2];
    int search_number;
	virtual void search() = 0;
	virtual void decide(long double utility) = 0;

	PCC(double alpha, bool latency_mode) : conditions_changed_too_much_(false), state_(START), monitor_in_start_phase_(-1), slow_start_factor_(2), alpha_(alpha), rate_(5.0), previous_rtt_(0),
			monitor_in_prog_(-1), utility_sum_(0), measurement_intervals_(0) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
        search_number = 0;
		setRTO(100000000);
		srand(time(NULL));

		if (!latency_mode) {
			beta_ = 0;
		} else {
			beta_ = 50;
		}
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

    double isAllSearchResultBack() {
        return search_monitor_number[0] == -1 && search_monitor_number[1] == -1;
    }

	virtual long double utility(unsigned long total, unsigned long loss, double time, double rtt) {

		//long double computed_utility = ((total-loss)/time*(1-1/(1+exp(-100*(double(loss)/total-0.05))))* (1-1/(1+exp(-1*(1-previous_rtt_/rtt)))) -1*double(loss)/time)/rtt*1000;

		long double norm_measurement_interval = time / rtt;
		long double utility = ((long double)total - (long double) (alpha_ * pow(loss, 1.2))) / norm_measurement_interval - beta_ * get_rtt(rtt) * total;
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
	size_t measurement_intervals_;
};

#endif
