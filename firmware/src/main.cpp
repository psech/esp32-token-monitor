#include <Arduino.h>
#include <esp_sleep.h>

#include "display.h"
#include "net.h"
#include "secrets.h"

namespace {

unsigned long lastFetchMs   = 0;
int           lastSession   = -1;
int           lastWeek      = -1;
bool          haveData      = false;
bool          errorState    = false;

// One countdown pixel per (interval / screenWidth) ms. SCREEN_W is 240 (matches display.cpp).
constexpr unsigned long COUNTDOWN_STEP_MS =
    POLL_INTERVAL_MS >= 240UL ? POLL_INTERVAL_MS / 240UL : 1UL;

void doFetch() {
  UsageSnapshot snap;
  if (!net::fetchUsage(snap)) {
    errorState = true;
    if (!haveData) display::drawSplash("offline", "retrying");
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

// The Arduino core ships without CONFIG_PM_ENABLE, so delay() never lets the
// chip sleep. Light sleep keeps RAM, GPIO levels and the LCD contents, and
// cuts the idle draw from tens of mA to under 1 mA.
void lightSleep(unsigned long ms) {
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  esp_light_sleep_start();
}

} // namespace

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[boot] esp32-token-monitor");

  display::begin();
  display::drawSplash("fetching", "usage");

  lastFetchMs = millis();
  doFetch();
}

void loop() {
  if (millis() - lastFetchMs >= POLL_INTERVAL_MS) {
    lastFetchMs = millis();
    doFetch();
  }

  const unsigned long elapsed = millis() - lastFetchMs;
  display::drawCountdown((float)elapsed / (float)POLL_INTERVAL_MS);
  if (elapsed >= POLL_INTERVAL_MS) return;

  const unsigned long toNextStep  = COUNTDOWN_STEP_MS - elapsed % COUNTDOWN_STEP_MS;
  const unsigned long toNextFetch = POLL_INTERVAL_MS - elapsed;
  lightSleep(toNextStep < toNextFetch ? toNextStep : toNextFetch);
}
