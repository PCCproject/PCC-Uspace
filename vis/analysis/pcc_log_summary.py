import numpy

_all_summary_stats = []
class PccSummaryStatistic:
    def __init__(self, name, func):
        self.name = name
        self.func = func
        global _all_summary_stats
        _all_summary_stats.append(self)

    def compute(self, values):
        return self.func(values)

def _compute_all_summary_stats(value_list):
    global _all_summary_stats
    summary_stats = {}
    for stat in _all_summary_stats:
        summary_stats[stat.name] = stat.compute(value_list)
    return summary_stats

class PccEventSummary:
    def __init__(self, pcc_log, event_type):
        self.event_type = event_type
        self.summary_stats = {}
        
        event_list = pcc_log.get_event_list(event_type)
        #print "Event list for " + event_type + " = " + str(event_list)
        self.summarized_values = []
        first_event = event_list[0]
        #print first_event
        for k in first_event.keys():
            try:
                float(first_event[k])
                self.summarized_values.append(k)
            except ValueError:
                pass

        for value in self.summarized_values:
            #print "Computing summary for " + value
            value_list = []
            for event in event_list:
                value_list.append(float(event[value]))
            self.summary_stats[value] = _compute_all_summary_stats(value_list)

    def get_summary_stat(self, value, statistic):
        #print "Getting summary (" + statistic + ") for " + value
        #print "Have summaries for " + str(self.summary_stats.keys())
        #print "Statistics for " + value + " are: " + str(self.summary_stats[value].keys())
        return self.summary_stats[value][statistic]

class PccLogSummary:
    def __init__(self, pcc_log):
        self.event_summaries = {}
        #for event_type in pcc_log.get_event_types():
        #    self.event_summaries[event_type] = PccEventSummary(pcc_log, event_type)
        event_type = "Calculate Utility"
        self.event_summaries[event_type] = PccEventSummary(pcc_log, event_type)

    def get_event_summary(self, event_type):
        return self.event_summaries[event_type]

def _summary_stat_avg(values):
    return numpy.mean(values)

def _summary_stat_stddev(values):
    return numpy.std(values)

_summary_avg = PccSummaryStatistic("Mean", _summary_stat_avg)
_summary_stddev = PccSummaryStatistic("Standard Deviation", _summary_stat_stddev)
