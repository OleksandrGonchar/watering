#include "net.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

namespace net {

bool connect(uint32_t timeoutMs) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[net] connecting to %s", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println(" — timeout");
      return false;
    }
    delay(200);
    Serial.print('.');
  }
  Serial.printf(" OK (%s)\n", WiFi.localIP().toString().c_str());
  return true;
}

namespace {

bool isHttps(const String& url) { return url.startsWith("https://"); }

}  // namespace

int httpPost(const String& path,
             const String& body,
             const String& bearerToken,
             String& respBody) {
  String url;
  if (path.startsWith("http://") || path.startsWith("https://")) {
    url = path;
  } else {
    url = String(SERVER_URL) + path;
  }

  std::unique_ptr<WiFiClient> client;
  if (isHttps(url)) {
    auto* secure = new WiFiClientSecure();
#ifdef SERVER_CERT_FINGERPRINT
    secure->setFingerprint(SERVER_CERT_FINGERPRINT);
#else
    secure->setInsecure();
#endif
    client.reset(secure);
  } else {
    client.reset(new WiFiClient());
  }

  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(*client, url)) {
    Serial.println("[net] http.begin failed");
    return 0;
  }
  http.addHeader("Content-Type", "application/json");
  if (bearerToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + bearerToken);
  }

  int status = http.POST(body);
  respBody = http.getString();
  http.end();
  Serial.printf("[net] POST %s -> %d (%u bytes)\n",
                url.c_str(), status, (unsigned)respBody.length());
  return status;
}

}  // namespace net
