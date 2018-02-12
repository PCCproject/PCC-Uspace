import json

class PccExperimentLog:
    def __init__(self, filename):
        print filename
        self.dict = {}
        try:
            self.dict = json.load(open(filename))
        except:
            self.dict = {}
            return

        self.event_types = []
        for event in self.dict["events"]:
            event_type = event.keys()[0]
            if event_type not in self.event_types:
                self.event_types.append(event_type)
        self.event_dict = {}
        for event_type in self.event_types:
            self.event_dict[event_type] = []
            for event in self.dict["events"]:
                if event_type in event.keys():
                    self.event_dict[event_type].append(event[event_type])
    
    def get_param(self, param):
        if "Experiment Parameters" not in self.dict.keys():
            return ""
        if param in self.dict["Experiment Parameters"].keys():
            return self.dict["Experiment Parameters"][param]
        else:
            rate_control_params = self.dict["events"][0]["Rate Control Parameters"]
            return rate_control_params[param]

    def get_event_list(self, event_type):
        return self.event_dict[event_type]

    def get_event_types(self):
        return self.event_types

    def apply_timeshift(self):
        timeshift_str = self.get_param("Time Shift")
        if timeshift_str == "":
            return
        timeshift = float(timeshift_str)
        for event_type in self.event_types:
            events = self.get_event_list(event_type)
            for event in events:
                if "Time" in event.keys():
                    event["Time"] = str(float(event["Time"]) + timeshift)
