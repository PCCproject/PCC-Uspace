#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : PCC(6, HIGH), measure_(UP), first_(true), up_utility_(0), down_utility_(0), consecutive_big_rtt_changes_(0) {}

protected:
	virtual void search() {
		if ((!first_) && (measure_ == UP)) {
			adapt();
		}
		guess();
	}
	virtual void decide(long double curr_utility) {
		if (measure_ == UP) {
			//cout << " direction = UP" << endl;
			up_utility_ = curr_utility;
			measure_ = DOWN;
		} else if (measure_ == DOWN) {
			//cout << " direction = DOWN" << endl;
			down_utility_ = curr_utility;
			measure_ = UP;
		}
	}

private:
	void guess() {
		if (first_) {
			if (measure_ == UP) base_rate_ = rate();
			if (measure_ == DOWN) first_ = false;
		}
		if (measure_ == UP) {
			setRate(base_rate_ + kDelta);
		} else if (measure_ == DOWN) {
			setRate(base_rate_ - kDelta);
		}
		
	}
	void adapt() {
		if ((rtt_changed_too_much_) && (consecutive_big_rtt_changes_ < 2)){
			consecutive_big_rtt_changes_++;
			return;
		}
		consecutive_big_rtt_changes_ = 0;
		
		double gradient = (up_utility_ - down_utility_) / (2 * kDelta);
		double change = kEpsilon * gradient;
		//cout << "up utility " << up_utility_ << ". down utility " << down_utility_ << ". Diff = " << up_utility_ - down_utility_ << ". CHANGE = " << change << endl;
		base_rate_ += change;
	}

	enum MeasurementDirection {
		UP,
		DOWN
	} measure_;
	
	bool first_;
	double base_rate_;
	double up_utility_;
	double down_utility_;
	size_t consecutive_big_rtt_changes_;
	
	static const double kEpsilon = 0.08;
	static const double kDelta = 0.7;
	
};

#endif
