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
    output_file_ << "{\n\"log version\":\"njay-1\",\n\"events\":[";
}
PccEventLogger::~PccEventLogger() {
    output_file_ << "\n]}" << std::endl;
    output_file_.close();
}
void PccEventLogger::LogEvent(const PccLoggableEvent& event) {
    if (!event.IsActive()) {
        return;
    }
    if (!first_line) {
        output_file_ << ",";
    } else {
        first_line = false;
    }
    float cur_time = ((float)(CTimer::getTime() - start_time)) / 1000000.0f;
    output_file_ << std::endl << "{" << std::endl << "  \"" << event.name << "\": {";
    output_file_ << std::endl << "    \"Time\": \"" << cur_time << "\"";
    bool needs_comma = true;
    for (const PccLoggableEventValue& val : event.values) {
        if (needs_comma) {
            output_file_ << ",";
        } else {
            needs_comma = true;
        }
        output_file_ << std::endl << "    \"" << val.value_name << "\": \"" << val.value << "\"";
    }
    output_file_ << std::endl << "}}";
}
