#include <Arduino.h>

#include "display.h"
#include "net.h"
#include "secrets.h"

namespace {

unsigned long lastFetchMs   = 0;
unsigned long lastTickMs    = 0;
int           lastSession   = -1;
int           lastWeek      = -1;
bool          haveData      = false;
bool          errorState    = false;

// Step size for the 1px countdown bar: one pixel per (interval / screenWidth) ms.
// Recomputed once at boot from POLL_INTERVAL_MS. SCREEN_W is 240 (matches display.cpp).
constexpr unsigned long countdownStepMs() {
  return POLL_INTERVAL_MS / 240UL;
}

void doFetch() {
  UsageSnapshot snap;
  if (!net::fetchUsage(snap)) {
    errorState = true;
    display::setErrorIndicator(true);
    return;
  }

  if (snap.stale) {
    errorState = true;
    display::setErrorIndicator(true);
  } else if (errorState) {
    errorState = false;
    display::setErrorIndicator(false);
  }

  if (!haveData || snap.sessionPct != lastSession || snap.weekPct != lastWeek) {
    display::drawBars(snap.sessionPct, snap.weekPct);
    lastSession = snap.sessionPct;
    lastWeek    = snap.weekPct;
    haveData    = true;
    Serial.printf("[usage] session=%d%% week=%d%% stale=%d\n",
                  snap.sessionPct, snap.weekPct, snap.stale ? 1 : 0);
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] esp32-token-monitor");

  display::begin();
  display::drawSplash("connecting", "wifi");

  net::connectWiFi();

  display::drawSplash("fetching", "usage");
  doFetch();
  lastFetchMs = millis();
  lastTickMs  = millis();
}

void loop() {
  const unsigned long now = millis();

  if (now - lastFetchMs >= POLL_INTERVAL_MS) {
    doFetch();
    lastFetchMs = now;
    lastTickMs  = now;
    display::drawCountdown(0.0f);
    return;
  }

  if (now - lastTickMs >= countdownStepMs()) {
    lastTickMs = now;
    float progress = (float)(now - lastFetchMs) / (float)POLL_INTERVAL_MS;
    display::drawCountdown(progress);
  }

  delay(5);
}
