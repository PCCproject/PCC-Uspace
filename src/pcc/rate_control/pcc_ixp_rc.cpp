#include "pcc_ixp_rc.h"

namespace {
    //double kRateChangeFactor = 1.08;
    int kRateReplicas = 20;
    int kMaxRateSamples = 2;
    double kProjectionDist = 30.0;
    double kMinRateDiffForGradient = 4096;
    double kMaxStepSize = 0.08;
    double kMinStepSize = 0.01;
    double kStepSizeIncrease = 0.01;
}

PccIxpRateController::PccIxpRateController(double call_freq, PccEventLogger* log) {
    cur_replica_ = 0;
    cur_mi_ = NULL;
    cur_rs_ = NULL;
    log_ = log;
    step_size_ = kMaxStepSize;
    last_change_pos_ = true;
}

PccIxpRateController::~PccIxpRateController() {
    if (cur_mi_ != NULL) {
        delete cur_mi_;
    }
    if (cur_rs_ != NULL) {
        delete cur_rs_;
    }
    for (int i = 0; i < rate_samples_.size(); ++i) {
        delete rate_samples_[i];
    }
}

QuicBandwidth PccIxpRateController::GetNextSendingRate(
        QuicBandwidth current_rate,
        QuicTime cur_time) {

    if (rate_samples_.empty()) {
        return current_rate;
    }

    ++cur_replica_;
    if (cur_replica_ == kRateReplicas) {
        cur_replica_ = 0;
    }

    if (cur_replica_ != 0) {
        return current_rate;
    }

    PccLoggableEvent event("Rate Change", "--log-utility-calc-lite");
    double change = 0;
    double gradient = ComputeProjectedUtilityGradient();
    event.AddValue("Utility Gradient", gradient);
    event.AddValue("Step Size", step_size_);

    if (gradient >= 0) {
        change = current_rate * step_size_;
    } else {
        change = current_rate / (1.0 + step_size_) - current_rate;
    }

    if ((change > 0) == last_change_pos_) {
        step_size_ += kStepSizeIncrease;
    } else {
        step_size_ /= 2.0;
    }
    if (step_size_ < kMinStepSize) {
        step_size_ = kMinStepSize;
    } else if (step_size_ > kMaxStepSize) {
        step_size_ = kMaxStepSize;
    }

    event.AddValue("Old Rate", current_rate);
    event.AddValue("New Rate", current_rate + change);
    log_->LogEvent(event); 
    
    if (change > 0) {
        last_change_pos_ = true;
    } else {
        last_change_pos_ = false;
    }
    return current_rate + change;
}

void PccIxpRateController::MonitorIntervalFinished(const MonitorInterval& mi) {
    
    double rate = mi.GetTargetSendingRate();
    std::cout << "Rate = " << rate << std::endl;
    if (cur_rs_ == NULL) {
        std::cout << "RS NULL" << std::endl;
        cur_rs_ = new RateSample(rate);
        cur_mi_ = new MonitorInterval(mi);
        return;
    }

    std::cout << "RS rate = " << cur_rs_->rate << std::endl;
    if (rate != cur_rs_->rate) {
        if (cur_rs_->n_samples == 0) {
            delete cur_rs_;
        } else {
            if (rate_samples_.size() >= kMaxRateSamples) {
                RateSample* rs = rate_samples_.front();
                rate_samples_.pop_front();
                delete rs;
            }
            rate_samples_.push_back(cur_rs_);
        }
        cur_rs_ = new RateSample(rate);
        delete cur_mi_;
        cur_mi_ = new MonitorInterval(mi);
        return;
    }

    cur_rs_->AddSample(cur_mi_, &mi);
    if (cur_rs_->n_samples > kRateReplicas) {
        if (rate_samples_.size() >= kMaxRateSamples) {
            RateSample* rs = rate_samples_.front();
            rate_samples_.pop_front();
            delete rs;
        }
        rate_samples_.push_back(cur_rs_);
        cur_rs_ = new RateSample(rate);
    }
    delete cur_mi_;
    cur_mi_ = new MonitorInterval(mi);
}

double PccIxpRateController::ComputeProjectedUtilityGradient() {

    if (rate_samples_.size() < 2) {
        return 0;
    }
    
    PccLoggableEvent event("Projected Gradient", "--log-utility-calc-lite");

    std::deque<RateSample*>::iterator it = rate_samples_.begin();
    const RateSample* cur_rs = *it;
    ++it;
    double grad_sum = 0;
    double cur_utility = ComputeProjectedUtility(cur_rs);
    std::vector<double> gradients;
    while (it != rate_samples_.end()) {
        const RateSample* next_rs = *it;
        double next_utility = ComputeProjectedUtility(next_rs);
        if (abs(cur_rs->rate - next_rs->rate) >= kMinRateDiffForGradient) {
            double gradient = (next_utility - cur_utility) / (next_rs->rate - cur_rs->rate);
            gradients.push_back(gradient);
            grad_sum += gradient;

            event.AddValue("Utility 1", cur_utility);
            event.AddValue("Projection 1", cur_rs->ud_mean * kProjectionDist * cur_rs->GetDeltaCertainty());
            event.AddValue("Certainty 1", cur_rs->GetDeltaCertainty());
            event.AddValue("Utility 2", next_utility);
            event.AddValue("Projection 2", next_rs->ud_mean * kProjectionDist * next_rs->GetDeltaCertainty());
            event.AddValue("Certainty 2", next_rs->GetDeltaCertainty());
            event.AddValue("Rate 1", cur_rs->rate);
            event.AddValue("Rate 2", next_rs->rate);
        }
        cur_rs = next_rs;
        cur_utility = next_utility;
        ++it;
    }
    log_->LogEvent(event); 
    if (gradients.size() == 0) {
        return 0;
    }
    double avg_grad = grad_sum / gradients.size();
    return avg_grad;
}

double PccIxpRateController::ComputeProjectedUtility(const RateSample* rs) {
    //double utility_projection = rs->ud_mean * kProjectionDist * rs->GetDeltaCertainty();
    double utility_projection = rs->ud_smallest * kProjectionDist;
    //if (utility_projection > 0) {
    //    utility_projection = 0;
    //}
    return rs->u_mean + utility_projection;
}

RateSample::RateSample(double rate) {
    this->rate = rate;
    n_samples = 0;
    u_mean = 0;
    u_var = 0;
    ud_mean = 0;
    ud_var = 0;
    ud_smallest = 0;
}

void RateSample::AddSample(const MonitorInterval* first_mi, const MonitorInterval* second_mi) {
    double utility = (first_mi->GetObsUtility() + second_mi->GetObsUtility()) / 2.0;
    double utility_delta = second_mi->GetObsUtility() - first_mi->GetObsUtility();
    if (n_samples == 0 || abs(utility_delta) < abs(ud_smallest)) {
        ud_smallest = utility_delta;
    }
    ++n_samples;
    double prev_mean = u_mean;
    u_mean += (utility - u_mean) / n_samples;
    u_var += (utility - prev_mean) * (utility - u_mean);

    double prev_delta_mean = ud_mean;
    ud_mean += (utility_delta - ud_mean) / n_samples;
    ud_var += (utility_delta - prev_delta_mean) * (utility_delta - ud_mean);
}

double RateSample::GetDeltaCertainty() const {
    if (n_samples < 2) {
        return 0;
    }
    double variance = ud_var / (n_samples - 1);
    return (1.0 - abs(ud_mean) * sqrt(n_samples) / sqrt(variance));
}
