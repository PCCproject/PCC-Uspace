#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : c_(0), rand_dir_(-1), prev_utility_(0), curr_utility_(0) {}

protected:
	virtual void search() {
		if (c_ == 1) {
			rand_dir_ = ((rand() / (1.0 * RAND_MAX)) > 0.5 ? 1 : -1);
			setRate(rate() + kDelta * rand_dir_);
		} if (c_ == 2) {
			double utility_diff = curr_utility_ - prev_utility_;
			double change = kEpsilon * rand_dir_ * (1 / kDelta) * project(utility_diff);
			double next_rate = rate() + change;
			//cout << "utility diff: " << utility_diff << ". direction " << rand_dir_<< ". CHANGE = " << change << endl;
			setRate(next_rate);
		}
	}
	virtual void decide(long double curr_utility) {
		//cout << "utility in rate " << rate() << " is " << curr_utility << endl;
		if (c_ == 0) {
			prev_utility_ = curr_utility;
		} else if (c_ == 1) {
			curr_utility_ = curr_utility;
		} 
		c_ = (c_ + 1) % 4;
	}

private:
	int c_;
	int rand_dir_;
	double prev_utility_;
	double curr_utility_;
};

#endif
