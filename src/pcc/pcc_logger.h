// PCC (Performance Oriented Congestion Control) algorithm

#ifndef NET_QUIC_CORE_CONGESTION_CONTROL_PCC_EVENT_LOGGER_H_
#define NET_QUIC_CORE_CONGESTION_CONTROL_PCC_EVENT_LOGGER_H_

#include <vector>
#include <map>
#include "../core/common.h"
#include "../core/options.h"
#include <iostream>
#include <fstream>
#include <mutex>
#include <sstream>
#define QUIC_EXPORT_PRIVATE

struct PccLoggableEventValue {
  std::string value_name;
  std::string value;
};

class QUIC_EXPORT_PRIVATE PccLoggableEvent {
 public:
  PccLoggableEvent(const std::string& event_name, const std::string& flag);
  ~PccLoggableEvent();
  bool IsActive() const { return active; }
  template <typename ValueType>
  void AddValue(const std::string& flag, const ValueType& value);
  std::string name;
  std::vector<PccLoggableEventValue> values;
 private:
  bool active;
  static bool CheckFlag(const std::string& flag);
  static std::map<std::string, bool> flag_map;
};

class QUIC_EXPORT_PRIVATE PccEventLogger {
 public:
  PccEventLogger(const std::string& filename);
  ~PccEventLogger();
  void LogEvent(const PccLoggableEvent&);
 private:
  std::ofstream output_file_;
  bool first_line;
  uint64_t start_time;
  std::mutex log_lock;
};

template <typename ValueType>
void PccLoggableEvent::AddValue(const std::string& flag, const ValueType& value) {
    if (!active) {
        return;
    }
    PccLoggableEventValue event_val;
    event_val.value_name = flag;
    std::ostringstream sstream;
    sstream << value;
    event_val.value = sstream.str();
    values.push_back(event_val);
}

#endif
