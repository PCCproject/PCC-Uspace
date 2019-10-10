#include "pcc_vivace_rc.h"

namespace {

    /***************************************************************************
     *                                CONSTANTS                                *
     **************************************************************************/

    /* Do NOT change any of the values in this section. */

    /* Number of bits in a megabit */
    double kMegabit = 1024.0 * 1024.0;

    /***************************************************************************
     *                              CONFIGURATION                              *
     **************************************************************************/

    /* 
     * Feel free to tune the values in this section based on common network
     * conditions.
     */

    /* 
     * kProbingStep: The proportion of current rate to change by when probing.
     * Smaller values are subject to greater noise, but larger values may test
     * detrimental (i.e. above link capacity) rates. In general, you might see
     * loss rates on a stable link a little higher than this value in the
     * PROBING state.
     *
     *   suggested values: [0.005, 0.05]
     *                     0.005 => 0.5% rate change in PROBING, very vulnerable
     *                              to noise.
     *                     0.05 => 5% rate change in PROBING, more reliable in
     *                             the presence of noise, but tests less
     *                             desirable rates.
     *   
     *   default: 0.02
     */
    double kProbingStep = 0.02;

    /*
     * kMinAmplifier: The minimum amplifier to rate change. This value
     * determines how fast out step size accelerates in the MOVING state.
     * Determines the number of monitor intervals in the MOVING state required
     * before the step size doubles. At the beginning of the MOVING state,
     * the amplifier will be set to this value. Each monitor interval, the
     * amplifier will be increased by 1, so if the kMinAmplifier is 1, then the
     * step size will double the 2nd interval of MOVING.
     *   
     *   suggested values: [2, 50]
     *                     2 => rapid acceleration (2 MIs to double step)
     *                     50 => very slow acceleration (50 MIs to double step)
     *
     *   default: 10
     */
    double kMinAmplifier = 10.0;
    
    /*
     * kGradientStepFactor: A multiplier applied to the gradient to determine
     * the step size in the MOVING state. This value will depend strongly on the
     * intended utility function. For functions that are nearly linear, the
     * minimum step size can be computed as:
     *
     *   d(utility)/d(rate) * kGradientStepFactor * kMinAmplifier
     *
     *   suggested values: Minimum step size should depend on expected
     *                     functional range, perhaps 1Mbps in many cases.
     *
     *   default: 0.1 * kMegabit
     *
     *            We approximate the derivative of the Vivace utility function
     *            as linear. Since we chose 10.0 as the kMinAmplifier, and we
     *            would like a 1Mbps minimum step size, our default value here
     *            is 0.1.
     */
    double kGradientStepFactor = 0.1 * kMegabit;
    
    /*
     * kMinChangeBound: Bounds the maximum rate change made in a single step as
     * a proportion of the current rate. This helps reduce effects of noise. A
     * bad MI can only change the rate by this proportion; however, this also
     * bounds the speed at which we can react to legitimate changes in network
     * conditions. A value of 0.1 means that we can change by at most 10% of the
     * current rate in any single MI.
     *
     *   suggested values: [0.05, 0.50]
     *                     0.05 => Can only change slowly, but should resist
     *                             noise.
     *                     0.50 => Can change by up to 50% in a single MI, which
     *                             may be extremely sensitive to noise, but
     *                             allows rapid reactions to changing network
     *                             conditions.
     *
     *   default: 0.10
     */
    double kMinChangeBound = 0.10;

    /*
     * kChangeBoundStepSize: A step size applied to the change bound. This step
     * allows us to make larger and larger adjustments in the case of repeated
     * decisions in the same direction. Hopefully consecutive decisions in the
     * same direction are not a result of noise.
     *
     *   suggested values: [0.0, 1.0] * kMinChangeBound
     *                     0.0 => no acceleration in large repeated decisions.
     *                     1.0 => accelerate quickly for large consecutive
     *                            decisions in the same direction.
     *
     *   default: 0.5 * kMinChangeBound
     */
    double kChangeBoundStepSize = 0.5 * kMinChangeBound;

    /*
     * kStartingRate: The starting rate for a connection in bits per second.
     *
     *   suggsted values: depends on expected network conditions.
     *
     *   default: 5 * kMegabit
     */
    QuicBandwidth kStartingRate = (QuicBandwidth)(5 * kMegabit);

    /*
     * kMinChangeProportion: The smallest allowed step as a proportion of the
     * current sending rate. If this number is small, PCC can make very tiny
     * steps, but it may be more vulnerable to noise when computing the
     * gradient. If this number is larger, PCC will be forced to change by
     * larger amounts, potentially reducing stability
     *
     *   suggested values: [0.01, 0.05]
     *
     *   default: 0.01
     */
    double kMinChangeProportion = 0.01;
}

PccVivaceRateController::PccVivaceRateController(PccEventLogger* log) {
    
    // Initialize the rate sample to a sentinel initial value.
    last_rate_sample_.rate = -1;

    // Begin with the target rate equal to our configured starting rate.
    target_rate_ = kStartingRate;

    log_ = log;
    state_ = STARTING;
}

QuicBandwidth PccVivaceRateController::GetNextStartingSendingRate(
        QuicBandwidth current_rate,
        QuicTime cur_time,
        int id) {

    if (last_rate_sample_.rate == -1) {
        // We haven't gotten a rate sample yet, so we don't know anything about
        // our rate decision. For now, just maintain the starting rate.
        return current_rate;
    }

    // As long as we're in STARTING, keep doubling sending rate.
    target_rate_ *= 2;
    return target_rate_;
}

QuicBandwidth PccVivaceRateController::GetNextProbingSendingRate(
        QuicBandwidth current_rate,
        QuicTime cur_time,
        int id) {

    // Determine which of the 4 probes we are starting.
    if (first_probing_sample_id_ == -1) {
        first_probing_sample_id_ = id;
    }
    int offset = id - first_probing_sample_id_;

    if (offset > 3) {
        // This wasn't a probe, so just send at the target rate.
        return target_rate_;
    }

    // Determine if the next probe should be a higher or lower rate.
    bool next_sample_high = false;
    if (probing_seed_ == 0) {
        next_sample_high = (offset == 1 || offset == 2);
    } else {
        next_sample_high = (offset == 0 || offset == 3);
    }

    // Return a rate either above or below our target rate by kProbingStep.
    if (next_sample_high) {
        return target_rate_ * (1.0 + kProbingStep);
    }
    return target_rate_ * (1.0 - kProbingStep);
}

QuicBandwidth PccVivaceRateController::GetMovingStep() {
    PccLoggableEvent event("Rate Change", "--log-utility-calc-lite");
    double step = kMegabit * ((double)last_gradient_) * kGradientStepFactor;
    event.AddValue("Utility Gradient", last_gradient_);
    event.AddValue("Gradient Step Factor", kGradientStepFactor);

    // Accelerate repeated rate changes.
    event.AddValue("Step Before Amplifier", step);
    step *= rate_change_amplifier_;
    ++rate_change_amplifier_;

    event.AddValue("Step Before Change Bound", step);
    // Put a proportional bound on the change (i.e. 10% or similar)
    int step_sign = step >= 0 ? 1 : -1;
    step *= step_sign;
    double step_ratio = step / target_rate_;
    if (step_ratio > rate_change_bound_) {
        step = rate_change_bound_ * target_rate_;
        rate_change_bound_ += kChangeBoundStepSize;
    } else {
        rate_change_bound_ = kMinChangeBound;
    }

    // Enforce a minimum change proportion (i.e. 1%)
    if (step_ratio < kMinChangeProportion) {
        step = kMinChangeProportion * target_rate_;
    }

    step *= step_sign;

    event.AddValue("Final Step", step);
    event.AddValue("Old Rate", target_rate_);
    event.AddValue("New Rate", target_rate_ + step);
    log_->LogEvent(event); 
   
    return step;
}

QuicBandwidth PccVivaceRateController::GetNextMovingSendingRate(
        QuicBandwidth current_rate,
        QuicTime cur_time,
        int id) {
    target_rate_ += GetMovingStep();
    return target_rate_;
}

QuicBandwidth PccVivaceRateController::GetNextSendingRate(
        QuicBandwidth current_rate,
        QuicTime cur_time,
        int id) {

    QuicBandwidth new_rate;
    switch (state_) {
    case STARTING :
        new_rate = GetNextStartingSendingRate(current_rate, cur_time, id);
        break;
    case PROBING :
        new_rate = GetNextProbingSendingRate(current_rate, cur_time, id);
        break;
    case MOVING :
        new_rate = GetNextMovingSendingRate(current_rate, cur_time, id);
        break;
    default:
        // This is an error
        assert(false && "Error: Unknown PccVivaceRateController State");
    }

    return new_rate;
}

void PccVivaceRateController::StartingMonitorIntervalFinished(
        const MonitorInterval& mi) {

    if (last_rate_sample_.rate == -1) {
        // We haven't gotten a rate sample yet.
        last_rate_sample_.rate = mi.GetTargetSendingRate();
        last_rate_sample_.utility = mi.GetObsUtility();
        return;
    }

    // We have a previous rate sample to compare to. If utility has decreased
    // since then, we will cut our rate in half and move to the probing phase.
    if (last_rate_sample_.utility > mi.GetObsUtility() && last_rate_sample_.rate < mi.GetTargetSendingRate()) {
        PccLoggableEvent event("Startup Finished", "--log-utility-calc-lite");
        event.AddValue("Last Rate", last_rate_sample_.rate);
        event.AddValue("Last Utility", last_rate_sample_.utility);
        event.AddValue("New Rate", mi.GetTargetSendingRate());
        event.AddValue("New Utility", mi.GetObsUtility());
        log_->LogEvent(event);
        target_rate_ = last_rate_sample_.rate;
        TransitionToProbing();
    } else {
        last_rate_sample_.rate = mi.GetTargetSendingRate();
        last_rate_sample_.utility = mi.GetObsUtility();
    }

}

void PccVivaceRateController::TransitionToProbing() {

    PccLoggableEvent event("State Change", "--log-utility-calc-lite");
    event.AddValue("New State", "Probing");
    log_->LogEvent(event); 
    

    // Reset the probing data
    first_probing_sample_id_ = -1;
    probing_seed_ = rand() % 2;
    
    state_ = PROBING;
}

void PccVivaceRateController::ProbingMonitorIntervalFinished(
        const MonitorInterval& mi) {
    
    int id = mi.GetId();
    int offset = id - first_probing_sample_id_;

    if (offset < 0) {
        // This sample is from before our probing started.
        return;
    }

    if (offset <= 3) {
        // This sample is a part of our probing.
        probing_rate_samples_[offset].rate = mi.GetTargetSendingRate();
        probing_rate_samples_[offset].utility = mi.GetObsUtility();
    }

    if (offset == 3) {
        // This sample is the last sample of our probing. We can make a decision
        // now.
        ProbingFinished();
    }
}

bool PccVivaceRateController::ProbingPairWasHigherBetter(int first_probe_offset) {
    RateSample& s1 = probing_rate_samples_[first_probe_offset];
    RateSample& s2 = probing_rate_samples_[first_probe_offset + 1];

    if (s1.rate == s2.rate) {
        return false;
    }

    return ComputeUtilityGradient(s1, s2) > 0;
}

double PccVivaceRateController::ProbingPairGetBetterRate(int first_probe_offset) {
    RateSample& s1 = probing_rate_samples_[first_probe_offset];
    RateSample& s2 = probing_rate_samples_[first_probe_offset + 1];
    return s1.utility > s2.utility ? s1.rate : s2.rate;
}

bool PccVivaceRateController::WasProbeConclusive() {
    return ProbingPairWasHigherBetter(0) == ProbingPairWasHigherBetter(2);
}

void PccVivaceRateController::ProbingFinished() {
    if (!WasProbeConclusive()) {
        TransitionToProbing();
        return;
    }

    bool two_change_pos = last_change_pos_ && ProbingPairWasHigherBetter(0);
    target_rate_ = ProbingPairGetBetterRate(0);
    last_rate_sample_ = probing_rate_samples_[3];
    last_change_pos_ = ProbingPairWasHigherBetter(0);
    last_gradient_ = ComputeUtilityGradient(probing_rate_samples_[2],
            probing_rate_samples_[3]);
    if (two_change_pos) {
        TransitionToMoving();
    } else {
        TransitionToProbing();
    }
}

void PccVivaceRateController::TransitionToMoving() {
    
    PccLoggableEvent event("State Change", "--log-utility-calc-lite");
    event.AddValue("New State", "Moving");
    log_->LogEvent(event); 
    
    // Reset the rate changing acceleration.
    rate_change_amplifier_ = kMinAmplifier;
    rate_change_bound_ = kMinChangeBound;
    
    state_ = MOVING;
}

double PccVivaceRateController::ComputeUtilityGradient(const RateSample& s1,
        const RateSample& s2) {
    if (s1.rate == s2.rate) {
        return 0;
    }

    return (s1.utility - s2.utility) / (s1.rate - s2.rate);
}

void PccVivaceRateController::MovingMonitorIntervalFinished(const MonitorInterval& mi) {
    RateSample rs;
    rs.rate = mi.GetTargetSendingRate();
    rs.utility = mi.GetObsUtility();
    last_gradient_ = ComputeUtilityGradient(rs, last_rate_sample_);
    if (last_change_pos_ == (last_gradient_ >= 0)) {
        // The gradient continues to point in our direction of movement. Keep
        // going.
        last_rate_sample_ = rs;
        return;
    }
        
    PccLoggableEvent event("Moving Finished", "--log-utility-calc-lite");
    event.AddValue("Last Rate", last_rate_sample_.rate);
    event.AddValue("Last Utility", last_rate_sample_.utility);
    event.AddValue("New Rate", rs.rate);
    event.AddValue("New Utility", rs.utility);
    event.AddValue("Last Change Positive", last_change_pos_);
    event.AddValue("Last Gradient", last_gradient_);
    log_->LogEvent(event);

    target_rate_ = last_rate_sample_.rate;
    target_rate_ += GetMovingStep();

    // The gradient is no longer pointing in the direction we are moving, time
    // to switch to probing.
    TransitionToProbing();
}

void PccVivaceRateController::MonitorIntervalFinished(const MonitorInterval& mi) {
    switch (state_) {
    case STARTING :
        StartingMonitorIntervalFinished(mi);
        break;
    case PROBING :
        ProbingMonitorIntervalFinished(mi);
        break;
    case MOVING :
        MovingMonitorIntervalFinished(mi);
        break;
    default:
        // This is an error
        assert(false && "Error: Unknown PccVivaceRateController State");
    }
}
