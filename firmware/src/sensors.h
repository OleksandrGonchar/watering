#ifndef WATERING_SENSORS_H
#define WATERING_SENSORS_H

#include <Arduino.h>

// Water-safety inputs (2 overflow plates + float / low-water reed + ack button
// + red LED) and humidifier motion I/O (motor direction + top/bottom dead-point
// reeds) on a mandatory PCF8574 I2C expander. See config.h for the pin map.
//
// Humidifier + motor power stays on ESP D8 (boot-safe). All other discrete I/O
// for these subsystems goes through this module so the rest of the firmware
// never talks to the expander directly.

namespace sensors {

void begin();

// Bit0 = overflow sensor #1, Bit1 = overflow sensor #2. 0 == both dry.
uint8_t overflowMask();

// True while the float / reed on P2 reports low water.
bool isLowWater();

// True while the top / bottom dead-point reed is closed.
bool isTopReed();
bool isBottomReed();

// True while the acknowledge button is held down.
bool buttonPressed();

// Drive the 2-channel polarity relay on P5 (HIGH = motor UP).
void setMotorDirection(bool up);

// Print overflow / float / dead-point / button state.
void logInputs();

void redLed(bool on);

}  // namespace sensors

#endif
