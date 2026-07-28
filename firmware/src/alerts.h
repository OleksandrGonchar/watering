#ifndef WATERING_ALERTS_H
#define WATERING_ALERTS_H

#include <Arduino.h>

// Blocking LED-indicator routine used while an overflow alert is active.
// Yields periodically so the Wi-Fi / system stack stays alive.

namespace alerts {

// Overflow: blink the RED LED forever until the acknowledge button is pressed.
// The device deliberately stays awake (no deep sleep) for the whole time.
void blinkRedUntilButton();

}  // namespace alerts

#endif
