#ifndef WATERING_HUMIDIFIER_H
#define WATERING_HUMIDIFIER_H

#include <Arduino.h>
#include <functional>

namespace humidifier {

// PIN_HUMIDIFIER (config.h) drives a MOSFET/relay for the humidifier. Runs only
// while the board is awake, so no latch is needed to hold state across sleep.
void begin();

// Run the humidifier for the given number of seconds. Blocking (the board
// stays awake for the whole run). If shouldAbort is provided and returns true
// (e.g. a leak sensor tripped), the output is cut immediately. Returns true if
// it was aborted early, false if it ran the full duration.
bool run(uint32_t seconds, std::function<bool()> shouldAbort = nullptr);

}  // namespace humidifier

#endif
