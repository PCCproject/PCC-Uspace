import json
import time

class PccEventLog():
    def __init__(self, filename):
        self.filename = filename
        self.data = {}
        self.data["Log Version"] = "njay-1"
        self.data["Experiment Parameters"] = {}
        self.data["Events"] = []

    def log_event(self, event):
        if "Time" not in event.keys():
            event["Time"] = time.time()
        self.data["Events"].append(event)

    def flush(self):
        if self.filename is not None:
            with open(self.filename, "w") as f:
                json.dump(self.data, f, indent=4)
