import json
import time

class PccEventLog():
    def __init__(self, filename, nonce=None, flow_id=0):
        self.filename = filename
        self.data = {}
        self.data["Log Version"] = "njay-1"
        self.data["Experiment Parameters"] = {}
        self.data["Events"] = []
        self.data["Flow ID"] = flow_id
        if nonce is not None:
            self.data["Nonce"] = nonce
            self.try_load()

    def log_event(self, event):
        if "Time" not in event.keys():
            event["Time"] = time.time()
        self.data["Events"].append(event)

    def file_matches_this_flow(self, file_data):
        return self.data["Flow ID"] == file_data["Flow ID"] and self.data["Nonce"] == file_data["Nonce"]

    def try_load(self):
        try:
            file_data = {}
            with open(self.filename) as f:
                file_data = json.load(f)
            if self.file_matches_this_flow(file_data):
                self.data = file_data
        except:
            pass

    def flush(self):
        if self.filename is not None:
            with open(self.filename, "w") as f:
                json.dump(self.data, f, indent=4)
