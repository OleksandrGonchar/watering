#include "sensors.h"
#include "config.h"

#ifndef REED_CLOSED_MEANS_LOW_WATER
// 0 = float closes when tank is full (open contact = low water). Set to 1 in
// config.h if your float closes near the bottom when empty.
#define REED_CLOSED_MEANS_LOW_WATER 0
#endif

// ---------------------------------------------------------------------------
// Backend 1: PCF8574 I2C expander (recommended).
// ---------------------------------------------------------------------------
#ifdef WATERING_USE_PCF8574

#include <Wire.h>
#include <PCF8574.h>

namespace {

PCF8574 expander(PCF8574_ADDR);

// A sensor "reads active" when its input line is at the configured level.
// Homemade plate sensors and the reed switch usually pull the line to GND when
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
  expander.write(PIN_LED_RED, LED_OFF_LEVEL);
  expander.write(PIN_LED_BLUE, LED_OFF_LEVEL);
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

bool buttonPressed() { return readActive(PIN_ACK_BUTTON); }

void logInputs() {
  Serial.printf(
      "[sensors] overflow=0x%02x lowWater=%d button=%d reedClosed=%d\n",
      overflowMask(), isLowWater() ? 1 : 0, buttonPressed() ? 1 : 0,
      readActive(PIN_LOW_WATER) ? 1 : 0);
}

void redLed(bool on) {
  expander.write(PIN_LED_RED, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

void blueLed(bool on) {
  expander.write(PIN_LED_BLUE, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

}  // namespace sensors

// ---------------------------------------------------------------------------
// Backend 2: direct NodeMCU GPIOs (fallback, no extra IC).
// ---------------------------------------------------------------------------
#else

namespace {

bool readActive(uint8_t pin) {
  int v = digitalRead(pin);
#if SENSOR_ACTIVE_HIGH
  return v == HIGH;
#else
  return v == LOW;
#endif
}

#ifdef REED_BUTTON_ON_ADC
// Reed switch and acknowledge button share A0 through a resistor ladder.
// A single analogRead is decoded into two flags; a closed contact pulls the
// line toward GND, so lower readings mean "active".
struct AdcState {
  int adc;
  bool reed;
  bool button;
};

AdcState readAdcState() {
  int adc = analogRead(A0);
  AdcState s{adc, false, false};
  if (adc < ADC_BOTH_MAX) {
    s.reed = true;
    s.button = true;
  } else if (adc < ADC_BUTTON_MAX) {
    s.button = true;
  } else if (adc < ADC_REED_MAX) {
    s.reed = true;
  }
  return s;
}
#endif
}  // namespace

namespace sensors {

void begin() {
  pinMode(PIN_OVERFLOW_1, INPUT_PULLUP);
  pinMode(PIN_OVERFLOW_2, INPUT_PULLUP);
#ifndef REED_BUTTON_ON_ADC
  pinMode(PIN_LOW_WATER, INPUT_PULLUP);
  pinMode(PIN_ACK_BUTTON, INPUT_PULLUP);
#endif
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);
  redLed(false);
  blueLed(false);
}

uint8_t overflowMask() {
  uint8_t mask = 0;
  if (readActive(PIN_OVERFLOW_1)) mask |= 0x01;
  if (readActive(PIN_OVERFLOW_2)) mask |= 0x02;
  return mask;
}

bool isLowWater() {
#ifdef REED_BUTTON_ON_ADC
  const bool reedClosed = readAdcState().reed;
#else
  const bool reedClosed = readActive(PIN_LOW_WATER);
#endif
#if REED_CLOSED_MEANS_LOW_WATER
  return reedClosed;
#else
  return !reedClosed;
#endif
}

bool buttonPressed() {
#ifdef REED_BUTTON_ON_ADC
  return readAdcState().button;
#else
  return readActive(PIN_ACK_BUTTON);
#endif
}

void logInputs() {
#ifdef REED_BUTTON_ON_ADC
  AdcState s = readAdcState();
  const bool lowWater = isLowWater();
  const char* decode = "idle (open)";
  if (s.reed && s.button) decode = "reed+button closed";
  else if (s.button) decode = "button closed";
  else if (s.reed) decode = "reed closed";

  Serial.printf("[sensors] A0 raw=%d / 1023  contact=%s  (thresholds: both<%d btn<%d reed<%d)\n",
                s.adc, decode, ADC_BOTH_MAX, ADC_BUTTON_MAX, ADC_REED_MAX);
  Serial.printf("[sensors] lowWater=%d (reedClosed=%d, REED_CLOSED_MEANS_LOW_WATER=%d) button=%d overflow=0x%02x\n",
                lowWater ? 1 : 0, s.reed ? 1 : 0, REED_CLOSED_MEANS_LOW_WATER,
                s.button ? 1 : 0, overflowMask());
#else
  Serial.printf(
      "[sensors] overflow=0x%02x lowWater=%d button=%d\n", overflowMask(),
      isLowWater() ? 1 : 0, buttonPressed() ? 1 : 0);
#endif
}

void redLed(bool on) {
  digitalWrite(PIN_LED_RED, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

void blueLed(bool on) {
  digitalWrite(PIN_LED_BLUE, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

}  // namespace sensors

#endif
