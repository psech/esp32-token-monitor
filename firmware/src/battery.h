#pragma once

struct BatteryReading {
  int  millivolts;
  int  percent;    // 0..100; meaningless while charging
  bool charging;   // USB present
};

namespace battery {

void begin();

// Sample the cell. Call with the radio off: Wi-Fi TX bursts sag the voltage.
BatteryReading read();

} // namespace battery
