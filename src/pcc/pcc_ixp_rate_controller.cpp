#include "pcc_ixp_rate_controller.h"

PccIxpRateController::PccIxpRateController() {
}

QuicBandwidth PccIxpRateController::GetNextSendingRate(
        PccMonitorIntervalAnalysisGroup& past_monitor_intervals,
        QuicBandwidth current_rate,
        QuicTime cur_time) {

    return 0;
}
