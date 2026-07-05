#ifndef WATERING_ALERTS_H
#define WATERING_ALERTS_H

#include <Arduino.h>

// Blocking LED-indicator routines used while an alert is active. Both yield()
// periodically so the Wi-Fi / system stack stays alive during the busy-loop.

namespace alerts {

// Overflow: blink the RED LED forever until the acknowledge button is pressed.
// The device deliberately stays awake (no deep sleep) for the whole time.
void blinkRedUntilButton();

// Low water: blink the BLUE LED for at least durationMs, then turn it off and
// return so the caller can go back to deep sleep.
void blinkBlueFor(uint32_t durationMs);

}  // namespace alerts

#endif
