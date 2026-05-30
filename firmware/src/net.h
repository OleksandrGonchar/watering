#ifndef WATERING_NET_H
#define WATERING_NET_H

#include <Arduino.h>

namespace net {

// Connect to Wi-Fi using credentials from config.h. Returns true on success.
// Times out after the given milliseconds.
bool connect(uint32_t timeoutMs = 20000);

// HTTP request helpers that accept absolute or path-relative URLs (path
// relative is appended to SERVER_URL from config.h).
//
// Returns the HTTP status code (0 on transport-level failure). The response
// body is written into `respBody`.
int httpPost(const String& path,
             const String& body,
             const String& bearerToken,
             String& respBody);

}  // namespace net

#endif
