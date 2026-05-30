#ifndef WATERING_PUMP_H
#define WATERING_PUMP_H

#include <Arduino.h>

namespace pump {

// GPIO12 / D6 — gate of the N-channel logic-level MOSFET.
constexpr uint8_t PIN = 12;

void begin();

// Run the pump for the given number of seconds. Blocking. Yields periodically
// so the WiFi/system stack stays alive (although typically called pre-WiFi).
void run(uint32_t seconds);

}  // namespace pump

#endif
