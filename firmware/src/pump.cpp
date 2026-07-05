#include "pump.h"

namespace pump {

void begin() {
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, LOW);
}

bool run(uint32_t seconds, std::function<bool()> shouldAbort) {
  Serial.printf("[pump] ON for %u s\n", seconds);
  digitalWrite(PIN, HIGH);
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
  digitalWrite(PIN, LOW);
  Serial.println(aborted ? "[pump] ABORTED (overflow)" : "[pump] OFF");
  return aborted;
}

}  // namespace pump
