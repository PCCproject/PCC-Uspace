#ifndef __GRADIENT_DESCENT_PCC_H__
#define __GRADIENT_DESCENT_PCC_H__

#include "pcc.h"

class GradientDescentPCC: public PCC {
public:
	GradientDescentPCC() : first_(true), up_utility_(0), down_utility_(0), seq_large_incs_(0), consecutive_big_changes_(0), decision_count_(0), curr_(0), next_delta(0) {trend_count_ =0;}

protected:
	virtual void search(int current_monitor) {
        if(trend_count_ >= 3) {
            //cout<<"turn to fast moving mode"<<endl;
            number_of_probes_ = 2;
        } else {
            //cout<<"turn to SLOW moving mode"<<endl;
            number_of_probes_ = 4;
        }

        for(int i=0; i<number_of_probes_/2; i++) {
            GuessStat g = GuessStat();
            int dir = rand()%2*2-1;
            for(int j=0;j<2;j++) {
                if((getkDelta()) * base_rate_ > 0.1) {
                    g.rate = (1 + dir * getkDelta()) * base_rate_;
                } else {
                    g.rate = base_rate_ + dir * 0.1;
                }
                g.isup = (dir>0);
                g.ready = false;
                g.monitor = (current_monitor + i*2 + j) % MAX_MONITOR;
                guess_measurement_bucket.push_back(g);
                dir *= -1;
            }
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
	virtual double decide(long double start_utility, long double end_utility, double old_rate, double new_rate, bool force_change) {
		double gradient = (end_utility - start_utility) / (new_rate - old_rate);
                //cout<<"gradient is "<<gradient<<" "<<end_utility<<" "<<start_utility<<" "<<new_rate<<" "<<old_rate<<endl;
		prev_gradiants_[curr_] = gradient;

		curr_ = (curr_ + 1) % MAX_MONITOR;

		//double change = 2 * rate()/1000 * kEpsilon * avg_gradient();
		//double change = avg_gradient() * rate();
		double change = avg_gradient() * kFactor;
                if(change * prev_change_ <= 0) {
                   amplifier = 0;
                   if(swing_buffer < 2)
                       swing_buffer ++;
                   
                }
                //cout<<"amplifier"<<amplifier<<endl;
                if(amplifier<3) {
                    change *= (pow(amplifier, 1) * 1  + 1);
                } else if (amplifier < 6) {
                    change *= (pow(amplifier, 1) * 2  - 3 + 1);
                } else if (amplifier < 9){
                    change *= (pow(amplifier, 1) * 4 - 15 + 1);
                } else {
                    change *= (pow(amplifier, 1) * 8 - 51 + 1);
                }

                //if(amplifier<6) {
                //    change *= (pow(amplifier, 1) * 1  + 1);
                //} else if (amplifier < 10) {
                //    change *= (pow(amplifier, 1) * 4  - 18 + 1);
                //} else {
                //    change *= (pow(amplifier, 1) * 8 - 58 + 1);
                //}
                if(change * prev_change_ <= 0) {
                     trend_count_ =0;
                     amplifier = 0;
                     boundary_amplifier = 0;
                } else {
                     trend_count_ ++;
                     if(swing_buffer == 0)
                        {amplifier ++;
                     }
                     if (swing_buffer > 0) {
                        swing_buffer --;
                     }
                }
                //change *= (amplifier*0.05+ kFactor);
                //cout<<pow(amplifier, 2)<<endl;
                //cout<<boundary_amplifier<<endl;
                //cout<<change<<endl;

#ifdef DEBUG
                cerr<<"change before force to boundary "<< change<<endl;
#endif

                double ratio = kBoundaryIncrement*boundary_amplifier + kInitialBoundary; 
                //if(ratio>0.8 && change<0) {
                //   ratio = 0.8;
                //}
                //if(ratio>2 && change>0) {
                //   ratio = 2;
                //}
                if((abs(change)/base_rate_)>ratio) {
                    change = abs(change)/change*base_rate_*ratio;
                    boundary_amplifier+=1;
                } else {
                    if(boundary_amplifier >= 1)
                       boundary_amplifier-=1;
                    //cout<<"not forcing"<<endl;
                }


                if(abs(change)/base_rate_ > 0.5) {
                    //change = abs(change)/change*rate()*(0.5);
                }
                if(abs(change)/base_rate_ < 0.05) {
                   //if(0.5 > base_rate_ * 0.05)
                   //change = abs(change)/change*base_rate_*(0.05);
                   //else
                   //change = 0.5 * abs(change)/change;
                 }
 

		if (force_change) {
			cout << "avg. gradient = " << avg_gradient() << endl;
			cout << "rate = " << rate() << endl;
			cout << "computed change: " << change << endl;
		}
#ifdef DEBUG
                cerr<<"change before force to min change is "<< change<<endl;
                cerr<<"gradient is "<< avg_gradient()<<endl;
                cerr<<"amplifier"<<amplifier<<endl;
#endif

		//if ((change >= 0) && (change < getMinChange())) change = getMinChange();

                //if (change>0 && change < base_rate_*getkDelta()) { change = base_rate_ * getkDelta();}
                //if (change <0 && change > base_rate_*getkDelta() * (-1)) {change = base_rate_ *getkDelta() * (-1);}

                if (change>0 && change < 0.25) { change = 0.25;}
                if (change <0 && change > -0.25) {change = -0.25;}

		prev_change_ = change;

		if (change == 0) cout << "Change is zero!" << endl;
#ifdef DEBUG
                cerr<<"change is "<<change<<endl;
#endif
        return change;

	}

private:
	double avg_gradient() const {
		int base = curr_;
		double sum = 0;
		for (int i = 0; i < kRobustness; i++) {
			base += MAX_MONITOR-1;
			sum += prev_gradiants_[base % MAX_MONITOR];
			//cout << "gradient " << prev_gradiants_[base % MAX_MONITOR] << " ";
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
                swing_buffer = 0;
	}

	bool first_;
	double up_utility_;
	double down_utility_;
	int seq_large_incs_;
	size_t consecutive_big_changes_;
	int decision_count_;
	int curr_;
	double prev_gradiants_[MAX_MONITOR];
        double swing_buffer;

	double next_delta;
};

#endif

