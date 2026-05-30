#include "pump.h"

namespace pump {

void begin() {
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, LOW);
}

void run(uint32_t seconds) {
  Serial.printf("[pump] ON for %u s\n", seconds);
  digitalWrite(PIN, HIGH);
  uint32_t end = millis() + seconds * 1000UL;
  while ((int32_t)(end - millis()) > 0) {
    delay(50);
    yield();
  }
  digitalWrite(PIN, LOW);
  Serial.println("[pump] OFF");
}

}  // namespace pump
