#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

using namespace std;

class ExponentiatedWeightsPCC: public PCC {
public:
	ExponentiatedWeightsPCC() : selection_ (0), is_first_(true), previous_utility_(0), count_(0) {
		for (int i = 0; i < kNumStrategies; i++) {
			weights_[i] = 1.0;
		}
		for (int i = 0; i < kNumStrategies; i++) {
			probs_[i] = 1.0 / kNumStrategies;
		}
	}

protected:
	virtual void search() {
		double p = rand() / (1.0 * RAND_MAX);
		int i = 0;
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

	virtual void decide(long double start_utility, long double end_utility, long double base_rate, bool conditions_changed) {
		static int count = 0;
		count++;
		if (is_first_) {
			previous_utility_ = start_utility;
			is_first_ = false;
		}

		double change = (start_utility - previous_utility_);

		weights_[selection_] = weights_[selection_] * (1 + 0.2 * change);
		avoid_starvation();
		double sum_weights = 0;
		for (int i = 0; i < kNumStrategies; i++) {
			sum_weights += weights_[i];
		}

		for (int i = 0; i < kNumStrategies; i++) {
			probs_[i] = weights_[i] / sum_weights;
		}

		if (count % 100 == 0) print_probs();

		previous_utility_ = start_utility;

	}

private:

	void avoid_starvation() {
		double sum_weights = 0;
		for (int i = 0; i < kNumStrategies; i++) {
			sum_weights += weights_[i];
		}

		double avg = sum_weights / kNumStrategies;

		for (int i = 0; i < kNumStrategies; i++) {
			if (weights_[i] < 0.1 * avg) {
				weights_[i] = 0.1 * avg;
			}
		}		
	}
	void print_probs() {
		for (int i = 0; i < kNumStrategies; i++) {
			cout << probs_[i] << " ";
		}
		cout << endl;
	}

	static const int kNumStrategies = (1 << 1) + 1;
	static const double kEpsilon = 0.05;
	static const double kDelta = 1;

	int selection_;
	bool is_first_;
	long double previous_utility_;
	size_t count_;
	double weights_[kNumStrategies];
	double probs_[kNumStrategies];};

#endif
