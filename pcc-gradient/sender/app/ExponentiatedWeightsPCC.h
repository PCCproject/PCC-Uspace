#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

using namespace std;

class ExponentiatedWeightsPCC: public PCC {
public:
	ExponentiatedWeightsPCC() : selection_ (0), is_first_(true), previous_utility_(0), count_(0) {
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
		int diff1 = selection_ - (kNumStrategies >> 1);
		long double diff = kEpsilon * diff1;
//		cout << "selection = " << selection_ << " chanding rate by " << diff << endl;
//		if (count_ % 10 == 0) {
//			print_weights();
//		} 
		count_++;
		setRate(rate() + diff);
		
	}

	virtual void decide(long double curr_utility) {
		if (is_first_) {
			previous_utility_ = curr_utility;
			is_first_ = false;
		}

		cout << "utility change:" << curr_utility << ","<< previous_utility_ << endl;
		weights_[selection_] = weights_[selection_] * (1 + 0.05 * project(curr_utility - previous_utility_)/kMaxProj);
		double sum_weights = 0;
		for (size_t i = 0; i < kNumStrategies; i++) {
			sum_weights += weights_[i];
		}

		double avg_weight = sum_weights / kNumStrategies;
		for (size_t i = 0; i < kNumStrategies; i++) {
			if (weights_[i] < 0.1 * avg_weight) {
				weights_[i] = 0.1 * avg_weight;
			}
		}

		for (size_t i = 0; i < kNumStrategies; i++) {
			sum_weights += weights_[i];
		}


		for (size_t i = 0; i < kNumStrategies; i++) {
			probs_[i] = weights_[i] / sum_weights;
		}
		previous_utility_ = curr_utility;
	}


private:
	void print_weights() {
		for (size_t i = 0; i < kNumStrategies; i++) {
			cout << probs_[i] << " ";
		}
		cout << endl;
	}

	static const size_t kNumStrategies = 1 << 4;
	unsigned int selection_;
	bool is_first_;
	long double previous_utility_;
	size_t count_;
	double weights_[kNumStrategies];
	double probs_[kNumStrategies];};

#endif
