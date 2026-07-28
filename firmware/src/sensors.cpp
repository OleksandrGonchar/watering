#include "sensors.h"
#include "config.h"

#include <Wire.h>
#include <PCF8574.h>

#ifndef REED_CLOSED_MEANS_LOW_WATER
// 0 = float closes when tank is full (open contact = low water). Set to 1 in
// config.h if your float closes near the bottom when empty.
#define REED_CLOSED_MEANS_LOW_WATER 0
#endif

namespace {

PCF8574 expander(PCF8574_ADDR);

// A sensor "reads active" when its input line is at the configured level.
// Homemade plate sensors and reed switches usually pull the line to GND when
// triggered, so the default (SENSOR_ACTIVE_HIGH == 0) treats LOW as active.
bool readActive(uint8_t pin) {
  int v = expander.read(pin);
#if SENSOR_ACTIVE_HIGH
  return v == HIGH;
#else
  return v == LOW;
#endif
}

}  // namespace

namespace sensors {

void begin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  expander.begin();
  // Quasi-bidirectional PCF8574 pins are used as inputs by writing them HIGH
  // (weak pull-up); a closed contact then sinks the line to GND.
  expander.write(PIN_OVERFLOW_1, HIGH);
  expander.write(PIN_OVERFLOW_2, HIGH);
  expander.write(PIN_LOW_WATER, HIGH);
  expander.write(PIN_ACK_BUTTON, HIGH);
  expander.write(PIN_TOP_REED, HIGH);
  expander.write(PIN_BOTTOM_REED, HIGH);
  expander.write(PIN_LED_RED, LED_OFF_LEVEL);
  expander.write(PIN_MOTOR_DIR, LOW);
}

uint8_t overflowMask() {
  uint8_t mask = 0;
  if (readActive(PIN_OVERFLOW_1)) mask |= 0x01;
  if (readActive(PIN_OVERFLOW_2)) mask |= 0x02;
  return mask;
}

bool isLowWater() {
  const bool reedClosed = readActive(PIN_LOW_WATER);
#if REED_CLOSED_MEANS_LOW_WATER
  return reedClosed;
#else
  return !reedClosed;
#endif
}

bool isTopReed() { return readActive(PIN_TOP_REED); }

bool isBottomReed() { return readActive(PIN_BOTTOM_REED); }

bool buttonPressed() { return readActive(PIN_ACK_BUTTON); }

void setMotorDirection(bool up) {
  expander.write(PIN_MOTOR_DIR, up ? HIGH : LOW);
}

void logInputs() {
  Serial.printf(
      "[sensors] overflow=0x%02x lowWater=%d topReed=%d bottomReed=%d button=%d\n",
      overflowMask(), isLowWater() ? 1 : 0, isTopReed() ? 1 : 0,
      isBottomReed() ? 1 : 0, buttonPressed() ? 1 : 0);
}

void redLed(bool on) {
  expander.write(PIN_LED_RED, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

}  // namespace sensors
