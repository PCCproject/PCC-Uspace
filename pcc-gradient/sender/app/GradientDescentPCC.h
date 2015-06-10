#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : c_(INITIAL), round_(0), rand_dir_(-1), prev_utility_(0), curr_utility_(0) {}

protected:
	virtual void search() {
		if (c_ == GUESS) {
			rand_dir_ = ((rand() / (1.0 * RAND_MAX)) > 0.5 ? 1 : -1);
			setRate(rate() + kDelta * rand_dir_);
		} if (c_ == ADAPT) {
			double utility_diff = curr_utility_ - prev_utility_;
			double change = kEpsilon * project(rand_dir_ * (1 / kDelta) * utility_diff);
			double next_rate = rate() + change;
			//cout << "utility diff: " << utility_diff << ". direction " << rand_dir_<< ". CHANGE = " << change << endl;
			setRate(next_rate);
		}
	}
	virtual void decide(long double curr_utility) {
		//cout << "utility in rate " << rate() << " is " << curr_utility << endl;
		if (c_ == INITIAL) {
			prev_utility_ = curr_utility;
			c_ = GUESS;
		} else if (c_ == GUESS) {
			curr_utility_ = curr_utility;
			c_ = ADAPT;
		} else if (c_ == ADAPT) {
			prev_utility_ = curr_utility;
			c_ = GUESS;
			round_++;
			if (round_ == 50) {
				c_ = INITIAL;
				round_ = 0;
			}
		}
	}

private:
	enum MeasurementState {
		INITIAL,
		GUESS,
		ADAPT
	} c_;

	size_t round_;
	int rand_dir_;
	double prev_utility_;
	double curr_utility_;
};

#endif
