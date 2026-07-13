#ifndef WATERING_SENSORS_H
#define WATERING_SENSORS_H

#include <Arduino.h>

// Water-safety inputs (2 overflow plate sensors + a magnetic float / reed
// low-water switch) and the two indicator LEDs + acknowledge button.
//
// The physical wiring is chosen at compile time in config.h:
//   * default (bare GPIO)          -> pins driven directly from NodeMCU GPIOs;
//     reed + button share A0 via a resistor ladder (REED_BUTTON_ON_ADC).
//   * WATERING_USE_PCF8574 defined  -> everything is routed through an I2C
//     PCF8574 expander (frees A0 + Serial RX for future expansion).
//
// All the rest of the firmware only talks to this abstraction, so swapping the
// wiring never touches main.cpp / schedule.cpp / alerts.cpp.

namespace sensors {

void begin();

// Bit0 = overflow sensor #1, Bit1 = overflow sensor #2. 0 == both dry.
uint8_t overflowMask();

// True while the reed switch reports a low water level (float near bottom).
bool isLowWater();

// True while the acknowledge button is held down.
bool buttonPressed();

// Print overflow / reed / button (and A0 ADC when using the resistor ladder).
void logInputs();

void redLed(bool on);
void blueLed(bool on);

}  // namespace sensors

#endif
