#ifndef _PCC_MI_METRIC_H_
#define _PCC_MI_METRIC_H_

#include "../pcc_mi.h"

#include <map>
#include <string>

class MonitorIntervalMetric {
  public:
    MonitorIntervalMetric(const std::string& name, double (*func)(const MonitorInterval& mi));
    double Evaluate(const MonitorInterval& mi) const {return (*func)(mi);};
    static const MonitorIntervalMetric* GetByName(const std::string& name);
  private:
    double (*func)(const MonitorInterval& mi);
    static std::map<std::string, const MonitorIntervalMetric*> name_lookup;
};

#endif
