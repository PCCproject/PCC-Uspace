#include "pcc_mi_metric.h"

std::map<std::string, const MonitorIntervalMetric*> MonitorIntervalMetric::name_lookup;


MonitorIntervalMetric::MonitorIntervalMetric(
        const std::string& name,
        double (*func)(const MonitorInterval& mi)) {

    this->func = func;
    name_lookup[name] = this;
}

const MonitorIntervalMetric* MonitorIntervalMetric::GetByName(const std::string& name) {
    std::map<std::string, const MonitorIntervalMetric*>::iterator search = name_lookup.find(name);
    if (search == name_lookup.end()) {
        return NULL;
    }
    return search->second;
}
