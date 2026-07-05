#include "humidifier.h"
#include "config.h"

namespace humidifier {

void begin() {
  pinMode(PIN_HUMIDIFIER, OUTPUT);
  digitalWrite(PIN_HUMIDIFIER, LOW);
}

bool run(uint32_t seconds, std::function<bool()> shouldAbort) {
  Serial.printf("[humidifier] ON for %u s\n", seconds);
  digitalWrite(PIN_HUMIDIFIER, HIGH);
  bool aborted = false;
  uint32_t end = millis() + seconds * 1000UL;
  while ((int32_t)(end - millis()) > 0) {
    if (shouldAbort && shouldAbort()) {
      aborted = true;
      break;
    }
    delay(50);
    yield();
  }
  digitalWrite(PIN_HUMIDIFIER, LOW);
  Serial.println(aborted ? "[humidifier] ABORTED (leak)" : "[humidifier] OFF");
  return aborted;
}

}  // namespace humidifier
