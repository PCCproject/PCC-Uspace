#ifndef __EvalPCC_H__
#define __EvalPCC_H__

#define _USE_MATH_DEFINES

#include <udt.h>
#include <ccc.h>
#include<iostream>
#include<cmath>
#include <deque>
#include <time.h>
#include <ctime> 
#include <pthread.h>

using namespace std;

class EvalPCC : public CCC {
public:
	EvalPCC() : rate_(2), last_update_(time(NULL)) {
		m_dPktSndPeriod = 10000;
		m_dCWndSize = 100000.0;
		setRTO(100000000);
		srand(time(NULL));
	}

	virtual ~EvalPCC() {}

	long double avg_utility() {
		return 0;
	}

	virtual void onLoss(const int32_t*, const int&) {}
	virtual void onTimeout(){}
	virtual void onACK(const int& ack){}

	virtual void onMonitorStart(int current_monitor) {
		pthread_mutex_lock(&mutex_);

		if (last_update_ + 1 < time(NULL)) {
			time(&last_update_);
			setRate(rate() + 0.01);
		}
		
		pthread_mutex_unlock(&mutex_);
	}

	virtual void onMonitorEnds(unsigned long total, unsigned long loss, double in_time, int current, int endMonitor, double rtt) {}
protected:
	virtual void setRate(double mbps) {
		rate_ = mbps;
		m_dPktSndPeriod = (m_iMSS * 8.0) / mbps;
	}

	double rate() const { return rate_; }

private:
	double rate_;
	pthread_mutex_t mutex_;
	time_t last_update_;
};

#endif
