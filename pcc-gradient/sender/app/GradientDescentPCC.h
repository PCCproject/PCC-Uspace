#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : PCC(6, false), first_(true), up_utility_(0), down_utility_(0), consecutive_big_changes_(0) {}

protected:
	virtual void search() {
		guess();
	}
	virtual void decide(long double curr_utility) {
		if ((conditions_changed_too_much_) && (consecutive_big_changes_ < 2)){
			consecutive_big_changes_++;
			return;
		}
		consecutive_big_changes_ = 0;

		double gradient = (search_monitor_utility[0] - search_monitor_utility[1]) / (2 * kDelta);
		double change = kEpsilon * gradient;
		//cout << "up utility " << up_utility_ << ". down utility " << down_utility_ << ". Diff = " << up_utility_ - down_utility_ << ". CHANGE = " << change << endl;
		base_rate_ += change;
	}

private:
	void guess() {
		if (first_) {
			if (search_number == 0) base_rate_ = rate();
			if (search_number == 1) first_ = false;
		}
		if (search_number == 0) {
			setRate(base_rate_ + kDelta);
		} else if (search_number == 1) {
			setRate(base_rate_ - kDelta);
		}

	}
	void adapt() {
	}

	bool first_;
	double base_rate_;
	double up_utility_;
	double down_utility_;
	size_t consecutive_big_changes_;

	static const double kEpsilon = 0.08;
	static const double kDelta = 0.7;

};

#endif
