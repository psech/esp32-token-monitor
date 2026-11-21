#include "battery.h"

#include <Arduino.h>

namespace battery {

namespace {

// T-Display wiring: the supply rail feeds GPIO34 through a 100k/100k divider
// that only conducts while GPIO14 is driven high.
constexpr int PIN_ADC    = 34;
constexpr int PIN_ADC_EN = 14;
constexpr int DIVIDER    = 2;
constexpr int SAMPLES    = 32;

// The divider sits on the rail USB and the cell share, so with USB in it reads
// ~4.9 V instead of the cell. No LiPo gets near 4.5 V, which makes it a clean
// USB-present signal. The charger's status pin only drives the LED, so there
// is no direct "charging" input to read.
constexpr int USB_THRESHOLD_MV = 4500;

struct CurvePoint { int mv; int pct; };

// Single-cell LiPo under light load. 0% is set at 3.4 V rather than the cell's
// 3.0 V cut-off because the board's 3.3 V regulator drops out below that.
constexpr CurvePoint CURVE[] = {
  {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80}, {3980, 75},
  {3950, 70},  {3910, 65}, {3870, 60}, {3850, 55}, {3840, 50}, {3820, 45},
  {3800, 40},  {3790, 35}, {3770, 30}, {3750, 25}, {3730, 20}, {3710, 15},
  {3690, 10},  {3610, 5},  {3400, 0},
};

int sampleMillivolts() {
  digitalWrite(PIN_ADC_EN, HIGH);
  delay(5);
  uint32_t sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += analogReadMilliVolts(PIN_ADC);
  digitalWrite(PIN_ADC_EN, LOW);
  return (int)(sum / SAMPLES) * DIVIDER;
}

int percentFor(int mv) {
  constexpr int n = sizeof(CURVE) / sizeof(CURVE[0]);
  if (mv >= CURVE[0].mv) return 100;
  if (mv <= CURVE[n - 1].mv) return 0;
  for (int i = 1; i < n; i++) {
    if (mv >= CURVE[i].mv) {
      const CurvePoint& hi = CURVE[i - 1];
      const CurvePoint& lo = CURVE[i];
      return lo.pct + (mv - lo.mv) * (hi.pct - lo.pct) / (hi.mv - lo.mv);
    }
  }
  return 0;
}

} // namespace

void begin() {
  pinMode(PIN_ADC_EN, OUTPUT);
  digitalWrite(PIN_ADC_EN, LOW);
  analogSetPinAttenuation(PIN_ADC, ADC_11db);
}

BatteryReading read() {
  BatteryReading r;
  r.millivolts = sampleMillivolts();
  r.charging   = r.millivolts >= USB_THRESHOLD_MV;
  r.percent    = r.charging ? 100 : percentFor(r.millivolts);
  Serial.printf("[battery] mv=%d pct=%d charging=%d\n",
                r.millivolts, r.percent, r.charging ? 1 : 0);
  return r;
}

} // namespace battery
