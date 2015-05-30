#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

using namespace std;

class ExponentiatedWeightsPCC: public PCC {
public:
	ExponentiatedWeightsPCC() : selection_ (0) {
		for (size_t i = 0; i < kNumStrategies; i++) {
			weights_[i] = 1.0;
		}
		for (size_t i = 0; i < kNumStrategies; i++) {
			probs_[i] = 1.0 / kNumStrategies;
		}
	}

protected:
	virtual void search() {
		double p = rand() / (1.0 * RAND_MAX);
		unsigned int i = 0;
		double s = probs_[i];
		while ((s < p) && (i < kNumStrategies)) {
			i++;
			s += probs_[i];
		}

		selection_ = i;
		setRate(rate() + kEpsilon * (selection_ - kNumStrategies / 2));
	}

	virtual void decide(long double curr_utility) {
		long double previous_utility = curr_utility;
		if (prev_utilities_.size() > 0) {
			previous_utility = prev_utilities_[prev_utilities_.size() - 1];
		}

		weights_[selection_] = weights_[selection_] * (1 - kDelta * (previous_utility - curr_utility));
		double sum_weights = 0;
		for (size_t i = 0; i < kNumStrategies; i++) {
			sum_weights += weights_[i];
		}
		for (size_t i = 0; i < kNumStrategies; i++) {
			probs_[i] = weights_[i] / sum_weights;
		}
	}


private:
	static const size_t kNumStrategies = 1 << 16;

	double weights_[kNumStrategies];
	double probs_[kNumStrategies];
	int selection_;
};

#endif
