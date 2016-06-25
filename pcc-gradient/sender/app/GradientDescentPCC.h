#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : first_(true), up_utility_(0), down_utility_(0), seq_large_incs_(0), consecutive_big_changes_(0), trend_count_(0), decision_count_(0), curr_(0), prev_change_(0), next_delta(0) {}

protected:
	virtual void search(int current_monitor) {
        for(int i=0; i<number_of_probes_; i++) {
            GuessStat g = GuessStat();
            if (i%2 == 0) {
                g.rate = (1 + kDelta) * rate();
            } else{
                g.rate = (1 - kDelta) * rate();
            }
            g.ready = false;
            g.monitor = (current_monitor+i) % 100;
            guess_measurement_bucket.push_back(g);
        }
	}

	virtual void restart() {
		init();
		PCC::restart();
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
	virtual void decide(long double start_utility, long double end_utility, bool force_change) {
		double gradient = -1 * (start_utility - end_utility) / (2 * kDelta * base_rate_ * 0.05);
		prev_gradiants_[curr_] = gradient;

		/*
		if (gradient * prev_gradiants_[(curr_ + 99) % 100] > 0) {
			trend_count_++;
		} else {
			trend_count_ = 0;
		}
		*/
		trend_count_++;
		curr_ = (curr_ + 1) % 100;
		if ((trend_count_ < kRobustness) && (!force_change)) {
			return;
		}
		trend_count_ = 0;

		double change = 2 * rate()/1000 * kEpsilon * avg_gradient();

		if (force_change) {
			cout << "avg. gradient = " << avg_gradient() << endl;
			cout << "rate = " << rate() << endl;
			cout << "computed change: " << change << endl;
		}

		if ((change >= 0) && (change < getMinChange())) change = getMinChange();

		if (change * prev_change_ >= 0) decision_count_++;
		else decision_count_ = 0;
		prev_change_ = change;

		if ((change > 0) && (decision_count_ == kGoToStartCount)) {
			#ifdef DEBUG_PRINT
			cout << "restart." << endl;
			#endif
			restart();
		}

		if (change == 0) cout << "Change is zero!" << endl;

		base_rate_ += change;
		if (force_change) {
			setRate(base_rate_);
		}

		if ((base_rate_ < 0) && (state_ != START)) {
			base_rate_ = 1.05 * kMinRateMbps;
		}
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
		return sum / kRobustness;
	}
	void guess() {
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

	static constexpr int kRobustness = 1;
	static constexpr double kEpsilon = 0.015;
	static constexpr double kDelta = 0.1;
	static constexpr int kGoToStartCount = 50000;
	double next_delta;
};

#endif

