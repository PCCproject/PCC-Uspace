#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : first_(true), up_utility_(0), down_utility_(0), seq_large_incs_(0), consecutive_big_changes_(0), trend_count_(0), curr_(0) {}

protected:
	virtual void search() {
		guess();
	}
	virtual void decide(long double start_utility, long double end_utility, long double base_rate, bool condition_changed) {
		/*
		if ((condition_changed) && (consecutive_big_changes_ < 5)) {
			consecutive_big_changes_++;
			return;
		}
		*/
		consecutive_big_changes_ = 0;

		double gradient = (start_utility - end_utility) / (2 * kDelta);
		prev_gradiants_[curr_] = gradient;
		if (gradient * prev_gradiants_[(curr_ + 99) % 100] > 0) {
			trend_count_++;
		} else {
			trend_count_ = 0;
		}
		curr_ = (curr_ + 1) % 100;
		
		if ((trend_count_ == 0) || (trend_count_ % kRobustness != 0)) {
			return;
		}
		//trend_count_ = 0;
		
		//cout << "rate:" << base_rate_ << endl;
		double change = kEpsilon * avg_gradient();

		base_rate_ = base_rate;
		//cout << "trend: " << trend_count_ / kRobustness << endl;
		if ((change > 0) && (trend_count_ > 40 * kRobustness)) {
			init();
			restart();
		}

		base_rate_ += change;
		if (base_rate_ < kMinRateMbps) base_rate_ = kMinRateMbps;
	}
	
private:
	double avg_gradient() const {
		int base = curr_;
		double sum = 0;
		for (int i = 0; i < kRobustness; i++) {
			base += 99;
			sum += prev_gradiants_[base % 100];
			//cout << "gradient " << prev_gradiants_[base % 100] << " ";
		}
		//cout <<"Gradient = " << kEpsilon * sum / kRobustness << endl;
		return sum / kRobustness;
	}
	void guess() {
		if (first_) {
			if (start_measurement_) base_rate_ = rate();
			if (!start_measurement_) first_ = false;
		}
		if (start_measurement_) {
			setRate(base_rate_ + kDelta);
		} else {
			setRate(base_rate_ - kDelta);
		}

	}
	void adapt() {
	}
	
	void init() {
		trend_count_ = 0;
		curr_ = 0;
		first_ = true;
		up_utility_ = 0;
		down_utility_ = 0;
		seq_large_incs_ = 0;
		consecutive_big_changes_ = 0;
	}

	bool first_;
	double up_utility_;
	double down_utility_;
	int seq_large_incs_;
	size_t consecutive_big_changes_;

	int trend_count_;
	int curr_;
	double prev_gradiants_[100];

	static constexpr int kRobustness = 3;
	static constexpr double kEpsilon = 0.075;
	static constexpr double kDelta = 1;

};

#endif
