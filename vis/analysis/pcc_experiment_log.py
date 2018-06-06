import json

def dict_str_to_num(d):
    for k in d.keys():
        try:
            d[k] = float(d[k])
        except:
            pass
    return d

class PccExperimentLog:
    def __init__(self, filename):
        print filename
        self.filename = filename
        self.dict = {}
        try:
            self.dict = json.load(open(filename))
        except:
            self.dict = {}
            return

        self.event_dict = {}
        for event in self.dict["Events"]:
            event_type = event["Name"]
            if event_type not in self.event_dict.keys():
                self.event_dict[event_type] = []
            ev = dict_str_to_num(event)
            self.event_dict[event_type].append(ev)
    
    def get_param(self, param):
        if "Experiment Parameters" not in self.dict.keys():
            return ""
        if param in self.dict["Experiment Parameters"].keys():
            return self.dict["Experiment Parameters"][param]
        else:
            if "Rate Control Parameters" not in self.dict.keys():
                return ""
            rate_control_params = self.dict["events"][0]["Rate Control Parameters"]
            return rate_control_params[param]

    def get_event_list(self, event_type):
        return self.event_dict[event_type]

    def get_event_types(self):
        return self.event_dict.keys()

    def apply_timeshift(self):
        timeshift_str = self.get_param("Time Shift")
        if timeshift_str == "":
            return
        timeshift = float(timeshift_str)
        for event_type in self.get_event_types():
            events = self.get_event_list(event_type)
            for event in events:
                if "Time" in event.keys():
                    event["Time"] = str(float(event["Time"]) + timeshift)
