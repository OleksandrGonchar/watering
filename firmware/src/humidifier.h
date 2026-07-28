#ifndef WATERING_HUMIDIFIER_H
#define WATERING_HUMIDIFIER_H

#include <Arduino.h>
#include <functional>

namespace humidifier {

// PIN_HUMIDIFIER (ESP D8) drives a MOSFET/relay for the humidifier + motor
// power. Motor direction (PCF P5) and top/bottom dead-point reeds (PCF P6/P7)
// are handled via sensors::*.
void begin();

// Run the humidifier for the given number of seconds. Blocking (the board
// stays awake for the whole run). During the run, dead-point reeds are polled
// every 50 ms; when triggered, the motor direction relay is set so the
// humidifier reverses. On start, a closed top/bottom reed picks the initial
// direction (cold-start safe); otherwise defaults to DOWN until the first
// dead point. If shouldAbort is provided and returns true (e.g. a leak), the
// output is cut immediately.
// Returns true if it was aborted early, false if it ran the full duration.
bool run(uint32_t seconds, std::function<bool()> shouldAbort = nullptr);

}  // namespace humidifier

#endif
