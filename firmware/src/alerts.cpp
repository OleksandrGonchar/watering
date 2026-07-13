#include "alerts.h"
#include "sensors.h"
#include "config.h"

namespace alerts {

void blinkRedUntilButton() {
  Serial.println("[alerts] OVERFLOW — red LED blinking until button press");

  // If the button happens to be held at entry, wait for a clean release first
  // so we don't instantly treat a stuck/held button as an acknowledgement.
  while (sensors::buttonPressed()) {
    delay(20);
    yield();
  }

  bool ledOn = false;
  uint32_t lastToggle = 0;
  while (true) {
    uint32_t nowMs = millis();
    if (nowMs - lastToggle >= LED_BLINK_INTERVAL_MS) {
      lastToggle = nowMs;
      ledOn = !ledOn;
      sensors::redLed(ledOn);
    }
    if (sensors::buttonPressed()) break;
    delay(10);
    yield();
  }

  sensors::redLed(false);
  Serial.println("[alerts] overflow acknowledged by button");
}

void blinkBlueFor(uint32_t durationMs) {
  Serial.printf("[alerts] LOW WATER — blue LED blinking for %u ms\n", durationMs);

  bool ledOn = false;
  uint32_t lastToggle = 0;
  uint32_t start = millis();
  while (millis() - start < durationMs) {
    uint32_t nowMs = millis();
    if (nowMs - lastToggle >= LED_BLINK_INTERVAL_MS) {
      lastToggle = nowMs;
      ledOn = !ledOn;
      sensors::blueLed(ledOn);
    }
    delay(10);
    yield();
  }

  sensors::blueLed(false);
  // One more settle write — GPIO3 can be weakly pulled by the USB-UART.
  delay(20);
  sensors::blueLed(false);
}

}  // namespace alerts
