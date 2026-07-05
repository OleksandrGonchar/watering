#ifndef WATERING_SCHEDULE_H
#define WATERING_SCHEDULE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <vector>
#include <map>

namespace schedule {

// Which actuator a schedule/event drives.
enum class Kind : uint8_t { Watering = 0, Humidifier = 1 };

struct Schedule {
  int id;
  int hour;
  int minute;
  int durationSeconds;
  Kind kind = Kind::Watering;
};

struct ScheduleStateEntry {
  String lastRunDate;   // "YYYY-MM-DD"
  uint32_t lastRunEpoch;
};

struct PendingEvent {
  int scheduleId;
  uint32_t durationSeconds;
  String wateredAtIso;  // local "YYYY-MM-DDTHH:MM:SS"
  Kind kind = Kind::Watering;
};

// Filled in by runDueSchedules to report whether watering had to be aborted
// because an overflow sensor triggered.
struct RunOutcome {
  bool overflow = false;
  uint8_t overflowSensors = 0;  // bit0 = sensor #1, bit1 = sensor #2
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
//
// Overflow safety: before every pump run (and continuously during it) the
// overflow plate sensors are polled. If water is detected the pump is cut, the
// partial duration is still logged, remaining schedules are skipped, and
// outcome.overflow is set so the caller can raise the alert.
void runDueSchedules(const DateTime& now,
                     const std::vector<Schedule>& schedules,
                     std::map<int, ScheduleStateEntry>& state,
                     std::vector<PendingEvent>& pending,
                     RunOutcome& outcome);

}  // namespace schedule

#endif
