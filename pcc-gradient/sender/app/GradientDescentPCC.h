#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : rand_dir_(-1) {}

protected:
	virtual void search() {
		rand_dir_ = ((rand() / (1.0 * RAND_MAX)) > 0.5 ? 1 : -1);
		setRate(rate() + kDelta * rand_dir_);
	}
	virtual void decide(long double curr_utility) {
		size_t next_rate = rate() + rand_dir_ * (kEpsilon / kDelta) * curr_utility;
		setRate(next_rate);
	}

private:
	int rand_dir_;
};

#endif
