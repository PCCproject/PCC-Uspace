#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : first_(true), up_utility_(0), down_utility_(0), seq_large_incs_(0), consecutive_big_changes_(0), trend_count_(0), decision_count_(0), curr_(0), prev_change_(0) {}

protected:
	virtual void search() {
		guess();
	}
	virtual void clear_state() {
		PCC::clear_state();
		first_ = true;
		up_utility_ = 0;
		down_utility_ = 0;
		seq_large_incs_ = 0;
		consecutive_big_changes_ = 0;
		trend_count_ = 0;
		decision_count_ = 0;
		curr_ = 0;
		prev_change_ = 0;
	}
	virtual void decide(long double start_utility, long double end_utility, long double base_rate, bool condition_changed) {
		
		if ((condition_changed) && (consecutive_big_changes_ < 5) && (!kPrint)) {
			consecutive_big_changes_++;
			trend_count_ = 0;
			decision_count_ = 0;
			return;
		}
		
		consecutive_big_changes_ = 0;

		double gradient = -1 * (start_utility - end_utility) / (2 * kDelta * base_rate_ * 0.05);
		prev_gradiants_[curr_] = gradient;
		if (gradient * prev_gradiants_[(curr_ + 99) % 100] > 0) {
			trend_count_++;
		} else {
			trend_count_ = 0;
		}
		curr_ = (curr_ + 1) % 100;
		
		if (((trend_count_ == 0) || (trend_count_ == kRobustness)) && (!kPrint)) {
			return;
		}
		trend_count_ = 0;
		
		if (kPrint) cout << "rate:" << rate() << endl;
		double change = (rate() / 100.) * kEpsilon * avg_gradient();
		
		//if ((change > 0) && (change < 0.1)) change = 0.1; 
		//if ((change < 0) && (change > 0.1)) change = -0.1;
		
		if (kPrint) change *= 2;
		if (change * prev_change_ > 0) decision_count_++;
		else decision_count_ = 0;
		prev_change_ = change;

		//base_rate_ = base_rate;
		//cout << "trend: " << decision_count_ << ". change " << change << " ratio " <<  rate() / 100. << endl;
		if ((change > 0) && (decision_count_ == 20)) {
			init();
			restart();
		}
		
		if (kPrint) cout << "base rate: " << base_rate_ << " --> ";
		
		base_rate_ += change;
		if (base_rate_ < kDelta) base_rate_ = kDelta;
		//cout << "base rate = " << base_rate_ << endl;
		
		if (kPrint) cout << base_rate_ << endl;
		if (kPrint) setRate(base_rate_);
		if (kPrint) init();
		kPrint = false;
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
		if (kPrint) cout <<"Gradient = " << kEpsilon * sum / kRobustness << endl;
		return sum / kRobustness;
	}
	void guess() {
		if (first_) {
			if (start_measurement_) base_rate_ = rate();
			if (!start_measurement_) first_ = false;
		}
		if (start_measurement_) {
			setRate(base_rate_ - kDelta * base_rate_ * 0.05);
		} else {
			setRate(base_rate_ + kDelta * base_rate_ * 0.05);
		}

	}
	void adapt() {
	}
	
	virtual void init() {
		trend_count_ = 0;
		curr_ = 0;
		first_ = true;
		up_utility_ = 0;
		down_utility_ = 0;
		seq_large_incs_ = 0;
		consecutive_big_changes_ = 0;
		decision_count_ = 0;
	}

	bool first_;
	double up_utility_;
	double down_utility_;
	int seq_large_incs_;
	size_t consecutive_big_changes_;
	int trend_count_;
	int decision_count_;
	int curr_;
	double prev_gradiants_[100];
	double prev_change_;

	static const int kRobustness = 4;
	static const double kEpsilon = 0.06;
	static const double kDelta = 1; 

};

#endif
