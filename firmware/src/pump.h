#ifndef WATERING_PUMP_H
#define WATERING_PUMP_H

#include <Arduino.h>
#include <functional>

namespace pump {

// GPIO12 / D6 — drives the base of the NPN low-side switch (via 220R).
// HIGH = transistor saturated = pump ON.
constexpr uint8_t PIN = 12;

void begin();

// Run the pump for the given number of seconds. Blocking. Yields periodically
// so the WiFi/system stack stays alive (although typically called pre-WiFi).
//
// If shouldAbort is provided and returns true at any point, the pump is cut
// immediately (used to stop watering the instant an overflow sensor triggers).
// Returns true if it was aborted early, false if it ran the full duration.
bool run(uint32_t seconds, std::function<bool()> shouldAbort = nullptr);

}  // namespace pump

#endif
