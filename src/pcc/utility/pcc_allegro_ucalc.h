
#ifndef _PCC_ALLEGRO_UCALC_H_
#define _PCC_ALLEGRO_UCALC_H_

#include "pcc_ucalc.h"

class PccAllegroUtilityCalculator : public PccUtilityCalculator {
  public:
    PccAllegroUtilityCalculator(PccEventLogger* log) {this->log = log;};
    ~PccAllegroUtilityCalculator() {};
    float CalculateUtility(PccMonitorIntervalAnalysisGroup& past_monitor_intervals, MonitorInterval&
        cur_monitor_interval);
  private:
    PccEventLogger* log;
};

#endif
