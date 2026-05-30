#ifndef WATERING_SCHEDULE_H
#define WATERING_SCHEDULE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <vector>
#include <map>

namespace schedule {

struct Schedule {
  int id;
  int hour;
  int minute;
  int durationSeconds;
};

struct ScheduleStateEntry {
  String lastRunDate;   // "YYYY-MM-DD"
  uint32_t lastRunEpoch;
};

struct PendingEvent {
  int scheduleId;
  uint32_t durationSeconds;
  String wateredAtIso;  // local "YYYY-MM-DDTHH:MM:SS"
};

// Parse "/config.json" payload into a vector of Schedules. configVersion is
// also extracted into out_version. Returns false if the doc is malformed.
bool parseConfig(const JsonDocument& doc, std::vector<Schedule>& out, int& out_version);

// Serialize {configVersion, schedules:[...]} into doc for saving back.
void serializeConfig(JsonDocument& doc, int version, const std::vector<Schedule>& schedules);

// Read "/state.json" into a map keyed by scheduleId.
void parseState(const JsonDocument& doc, std::map<int, ScheduleStateEntry>& out);
void serializeState(JsonDocument& doc, const std::map<int, ScheduleStateEntry>& state);

void parsePending(const JsonDocument& doc, std::vector<PendingEvent>& out);
void serializePending(JsonDocument& doc, const std::vector<PendingEvent>& events);

// For each schedule, run it if (now >= time) AND (lastRunDate != today) AND
// (now - lastRunEpoch >= 6h circuit-breaker). Records a PendingEvent and
// updates state in-place. Calls pump::run() while the GPIO is HIGH.
//
// This logic is the catch-up rule: if the device was offline at the scheduled
// minute, it still waters when it next wakes up.
void runDueSchedules(const DateTime& now,
                     const std::vector<Schedule>& schedules,
                     std::map<int, ScheduleStateEntry>& state,
                     std::vector<PendingEvent>& pending);

}  // namespace schedule

#endif
