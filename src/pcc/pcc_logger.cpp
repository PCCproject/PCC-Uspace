#include "pcc_logger.h"

std::map<std::string, bool> PccLoggableEvent::flag_map;

PccLoggableEvent::PccLoggableEvent(const std::string& event_name, const std::string& flag) {
    active = CheckFlag(flag);
    if (!active) {
        return;
    }
    name = event_name;
}

PccLoggableEvent::~PccLoggableEvent() {
    // Nothing needed yet.
}

bool PccLoggableEvent::CheckFlag(const std::string& flag) {
    std::map<std::string, bool>::iterator map_entry = flag_map.find(flag);
    if (map_entry != flag_map.end()) {
        return map_entry->second;
    }
    bool result = (Options::Get(flag.c_str()) != NULL);
    flag_map.insert(std::make_pair(flag, result));
    return result;
}

PccEventLogger::PccEventLogger(const std::string& filename) {
    first_line = true;
    output_file_.open(filename);
    start_time = CTimer::getTime();
    output_file_ << "{\n\"log version\":\"njay-1\",\n";
    output_file_ << "\"Experiment Parameters\":{";
    bool needs_comma = false;
    output_file_ << "\n  \"PCC Args\":\"";
    for (int i = 0; i < Options::argc; ++i) {
        output_file_ << Options::argv[i] << " ";
    }
    output_file_ << "\"";
    needs_comma = true;
    const char* arg_experiment = Options::Get("-experiment=");
    if (arg_experiment != NULL) {
        output_file_ << "\n  \"Experiment\":\"" << arg_experiment << "\"";
        needs_comma = true;
    }
    const char* arg_npairs = Options::Get("-npairs=");
    if (arg_npairs != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Number of Pairs\":\"" << arg_npairs << "\"";
        needs_comma = true;
    }
    const char* arg_bandwidth = Options::Get("-bandwidth=");
    if (arg_bandwidth != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Bandwidth\":\"" << arg_bandwidth << "\"";
        needs_comma = true;
    }
    const char* arg_queue = Options::Get("-queue=");
    if (arg_queue != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Queue Length\":\"" << arg_queue << "\"";
        needs_comma = true;
    }
    const char* arg_delay = Options::Get("-delay=");
    if (arg_delay != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Latency\":\"" << arg_delay << "\"";
        needs_comma = true;
    }
    const char* arg_loss = Options::Get("-loss=");
    if (arg_loss != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Loss Rate\":\"" << arg_loss << "\"";
        needs_comma = true;
    }
    const char* arg_timeshift = Options::Get("-timeshift=");
    if (arg_timeshift != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Time Shift\":\"" << arg_timeshift << "\"";
        needs_comma = true;
    }
    const char* arg_flowid = Options::Get("-flowid=");
    if (arg_flowid != NULL) {
        if (needs_comma) {
            output_file_ << ",";
        }
        output_file_ << "\n  \"Flow ID\":\"" << arg_flowid << "\"";
        needs_comma = true;
    }
    output_file_ << "\n},\n\"Events\":[";
}
PccEventLogger::~PccEventLogger() {
    output_file_ << "\n]}" << std::endl;
    output_file_.close();
}
void PccEventLogger::LogEvent(const PccLoggableEvent& event) {
    if (!event.IsActive()) {
        return;
    }
    std::lock_guard<std::mutex> locked(log_lock);
    if (!first_line) {
        output_file_ << ",";
    } else {
        first_line = false;
    }
    float cur_time = ((float)(CTimer::getTime() - start_time)) / 1000000.0f;
    output_file_ << std::endl << "{" << std::endl;
    output_file_ << "    \"Name\": \"" << event.name << "\"," << std::endl;
    output_file_ << "    \"Time\": \"" << cur_time << "\"";
    bool needs_comma = true;
    for (const PccLoggableEventValue& val : event.values) {
        if (needs_comma) {
            output_file_ << ",";
        } else {
            needs_comma = true;
        }
        output_file_ << std::endl << "    \"" << val.value_name << "\": \"" << val.value << "\"";
    }
    output_file_ << std::endl << "}";
    output_file_.flush();
}
