#include "humidifier.h"
#include "sensors.h"
#include "config.h"

namespace humidifier {

namespace {
bool motorDirUp = false;

void applyMotorDirection(bool up) {
  motorDirUp = up;
  sensors::setMotorDirection(up);
}

// Pick initial direction from dead-point reeds when already at a limit
// (survives power loss). Mid-travel → default DOWN until the first hit.
void syncDirectionFromReeds() {
  if (sensors::isTopReed()) {
    applyMotorDirection(false);
  } else if (sensors::isBottomReed()) {
    applyMotorDirection(true);
  } else {
    applyMotorDirection(false);
  }
}
}  // namespace

void begin() {
  pinMode(PIN_HUMIDIFIER, OUTPUT);
  digitalWrite(PIN_HUMIDIFIER, LOW);
  syncDirectionFromReeds();
}

bool run(uint32_t seconds, std::function<bool()> shouldAbort) {
  syncDirectionFromReeds();
  Serial.printf("[humidifier] ON for %u s (motor dir=%s)\n", seconds,
                motorDirUp ? "UP" : "DOWN");
  digitalWrite(PIN_HUMIDIFIER, HIGH);
  bool aborted = false;
  bool prevBottom = sensors::isBottomReed();
  bool prevTop = sensors::isTopReed();
  uint32_t end = millis() + seconds * 1000UL;
  while ((int32_t)(end - millis()) > 0) {
    if (shouldAbort && shouldAbort()) {
      aborted = true;
      break;
    }

    bool curBottom = sensors::isBottomReed();
    bool curTop = sensors::isTopReed();

    if (curBottom && !prevBottom) {
      applyMotorDirection(true);
      Serial.println("[humidifier] bottom dead point -> motor UP");
    } else if (curTop && !prevTop) {
      applyMotorDirection(false);
      Serial.println("[humidifier] top dead point -> motor DOWN");
    }
    prevBottom = curBottom;
    prevTop = curTop;

    delay(50);
    yield();
  }
  digitalWrite(PIN_HUMIDIFIER, LOW);
  Serial.println(aborted ? "[humidifier] ABORTED (leak)" : "[humidifier] OFF");
  return aborted;
}

}  // namespace humidifier
