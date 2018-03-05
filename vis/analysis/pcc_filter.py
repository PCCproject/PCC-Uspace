import pcc_experiment_log

class PccFilterParameterRange:
    def __init__(self, param, param_min, param_max, inverted):
        self.inverted = inverted
        self.param = param
        self.param_min = param_min
        self.param_max = param_max

    def matches(self, experiment_log):
        param_value = float(experiment_log.get_param(self.param))
        return not (self.inverted == (param_value >= self.param_min and param_value <= self.param_max))

    def matches_dict(self, input_dict):
        #print "Checking if event " + str(input_dict) + " matches " + self.param + " in [" + str(self.param_min) + ", " + str(self.param_max) + "]" 
        param_value = float(input_dict[self.param])
        result = param_value >= self.param_min and param_value <= self.param_max
        #print "Result = " + str(result)
        return not (self.inverted == result)

    def get_legend_label(self):
        if self.inverted:
            return self.param + " NOT in [" + str(self.param_min) + ", " + str(self.param_max) + "]" 
        else:
            return self.param + " in [" + str(self.param_min) + ", " + str(self.param_max) + "]" 

class PccFilterParameterSubstring:
    def __init__(self, param, substring, inverted):
        self.inverted = inverted
        self.param = param
        self.substring = substring

    def matches(self, experiment_log):
        param_value = experiment_log.get_param(self.param)
        return not (self.inverted == (self.substring in param_value))

    def matches_dict(self, input_dict):
        param_value = input_dict[self.param]
        return not (self.inverted == (self.substring in param_value))

    def get_legend_label(self):
        if self.inverted:
            return self.param + " does NOT contain " + self.substring
        else:
            return self.param + " contains " + self.substring

class PccFilterParameterValue:
    def __init__(self, param, value_list, inverted):
        self.param = param
        self.value_list = value_list
        self.inverted = inverted

    def matches(self, experiment_log):
        param_value = experiment_log.get_param(self.param)
        return not (self.inverted == (param_value in self.value_list))

    def matches_dict(self, input_dict):
        param_value = input_dict[self.param]
        return not (self.inverted == (param_value in self.value_list))

    def get_legend_label(self):
        if self.inverted:
            return self.param + " NOT in " + str(self.value_list)
        else:
            return self.param + " in " + str(self.value_list)

class PccLogFilter:
    def __init__(self):
        self.parameter_checks = []

    def __init__(self, json_obj):
        self.parameter_checks = []
        print(json_obj)
# FIGURE OUT AND CHANGE LATER
        #json_obj = json_obj["log filters"]
        for json_filter in json_obj:
            if ("values" in json_filter.keys()):
                self.add_parameter_check(PccFilterParameterValue(
                    json_filter["param"],
                    json_filter["values"], "not" in json_filter.keys()))
            elif ("min" in json_filter.keys()):
                self.add_parameter_check(PccFilterParameterRange(
                    json_filter["param"],
                    float(json_filter["min"]),
                    float(json_filter["max"]), "not" in json_filter.keys()))
            elif ("substring" in json_filter.keys()):
                self.add_parameter_check(PccFilterParameterSubstring(
                    json_filter["param"],
                    json_filter["substring"], "not" in json_filter.keys()))

    def add_parameter_check(self, check):
        self.parameter_checks.append(check)

    def matches(self, experiment_log):
        for check in self.parameter_checks:
            if not check.matches(experiment_log):
                return False
        return True

    def apply_filter(self, experiment_log_list):
        matched_experiments = []
        for experiment_log in experiment_log_list:
            if self.matches(experiment_log):
                matched_experiments.append(experiment_log)
        return matched_experiments

    def get_legend_label(self): 
        legend_label = ""
        for i in range(0, len(self.parameter_checks)):
            if i > 0:
                legend_label += " AND"
            legend_label += " " + self.parameter_checks[i].get_legend_label()
        return legend_label

class PccEventFilter:
    def __init__(self, json_obj):
        self.event_name = json_obj["event name"]
        self.parameter_checks = []
        for json_filter in json_obj["filters"]:
            if ("values" in json_filter.keys()):
                self.add_parameter_check(PccFilterParameterValue(
                    json_filter["param"],
                    json_filter["values"], "not" in json_filter.keys()))
            elif ("min" in json_filter.keys()):
                self.add_parameter_check(PccFilterParameterRange(
                    json_filter["param"],
                    float(json_filter["min"]),
                    float(json_filter["max"]), "not" in json_filter.keys()))
            elif ("substring" in json.filter.keys()):
                self.add_parameter_check(PccFilterParameterSubstring(
                    json_filter["param"],
                    json_filter["substring"], "not" in json_filter.keys()))

    def add_parameter_check(self, check):
        self.parameter_checks.append(check)

    def matches_dict(self, event):
        for check in self.parameter_checks:
            if not check.matches_dict(event):
                return False
        return True

    def apply_filter(self, experiment_log):
        event_list = experiment_log.get_event_list(self.event_name)
        matched_events = []
        for event in event_list:
            if self.matches_dict(event):
                matched_events.append(event)
        if len(matched_events) == 0:
            experiment_log.event_dict.pop(self.event_name)
            experiment_log.event_types.remove(self.event_name)
        else:
            experiment_log.event_dict[self.event_name] = matched_events


