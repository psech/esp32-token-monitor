# esp32-token-monitor — firmware

PlatformIO project for the **TTGO T-Display ESP32** (240×135 ST7789 LCD). Polls the laptop service every `POLL_INTERVAL_MS` and draws two progress bars, a Wi-Fi + battery status row and a 1 px countdown row at the bottom. Optionally reports usage and battery to [Home Assistant](#home-assistant) over MQTT. Built to run off a single-cell LiPo; see [Battery](#battery).

## Layout

```
┌──────────────────────────────────────────────┐
│ Current session                          NN% │
│ ████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│                                              │
│ Current week (all models)                NN% │
│ ████████████████████████░░░░░░░░░░░░░░░░░░░░ │
│                                              │
│ ▂▄▆█ -62 dBm                      87% [███ ]▪│ ← Wi-Fi / battery
│ █████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │ ← 1px countdown
└──────────────────────────────────────────────┘
```

A red 1 px line appears at the **top** when the most recent fetch failed or returned `stale: true`. It clears on the next successful fresh response.

Bar fill colour shifts based on percentage: magenta → amber at 75% → red at 90%.

The status row has Wi-Fi on the left and battery on the right.

**Wi-Fi.** Four signal bars and the signal strength in dBm, measured when the last poll connected. The radio is off between polls, so that's the freshest reading there is.

| Signal | Bars |
| --- | --- |
| -55 dBm or stronger | 4 |
| -56 to -65 dBm | 3 |
| -66 to -75 dBm | 2 |
| weaker than -75 dBm | 1 |

If the connect fails, the bars turn into red outlines and the text reads `No Wi-Fi`. This tells you which part failed when the red top bar is lit: red Wi-Fi bars mean the board couldn't join the network; normal Wi-Fi bars with the red top bar mean the service failed or returned stale data.

**Battery** shows one of two states:

- **On battery (discharging):** the charge percentage and a battery icon filled to match. Both are grey, turning amber below 20% and red below 10%.
- **On USB (charging):** a green lightning bolt and `Charging`.

## One-time setup

1. **Install PlatformIO** — either:
   - VS Code extension: `platformio.platformio-ide` (recommended, also installs the `pio` CLI), or
   - `pip install platformio` on the host.
2. **Create your secrets file**:
   ```
   cp include/secrets.h.example include/secrets.h
   $EDITOR include/secrets.h    # set WIFI_*, SERVICE_HOST, etc.
   ```
   `secrets.h` is gitignored.
3. Find the laptop's LAN IP and put it in `SERVICE_HOST`.

## Build, upload, monitor

```
pio run                    # compile
pio run -t upload          # flash over USB
pio device monitor         # serial @ 115200, with esp32_exception_decoder
```

Or, in VS Code: open the `firmware/` folder, then click PlatformIO's checkmark / arrow / plug icons in the bottom bar.

## Configuration knobs

All in `include/secrets.h`:

| Macro | Default | Notes |
| --- | --- | --- |
| `WIFI_SSID` | — | Wi-Fi network name |
| `WIFI_PASSWORD` | — | Wi-Fi password |
| `SERVICE_HOST` | — | IP/hostname of the laptop running the service |
| `SERVICE_PORT` | `8787` | HTTP port |
| `POLL_INTERVAL_MS` | `60000` | Fetch cadence; the countdown bar fills over this duration regardless of value. 60 s matches the service cache TTL |
| `BACKLIGHT_PERCENT` | `30` | LCD backlight brightness, 0–100. Biggest constant load on battery. Optional; defaults to 30 if missing |
| `MQTT_HOST` | unset | Mosquitto broker used by Home Assistant. Leave undefined to turn [Home Assistant](#home-assistant) reporting off |
| `MQTT_PORT` | `1883` | Broker port |
| `MQTT_USER` | — | Broker username |
| `MQTT_PASSWORD` | — | Broker password |
| `HA_DEVICE_NAME` | `Token Monitor` | Device name in HA; entity IDs are derived from it (`sensor.token_monitor_battery`) |
| `HA_DISCOVERY_PREFIX` | `homeassistant` | Only change if HA's MQTT integration uses a different discovery prefix |

## Home Assistant

When `MQTT_HOST` is set, every poll also publishes to Home Assistant's MQTT broker, in the same radio session as the GET ([src/ha.cpp](src/ha.cpp)). The device uses [MQTT discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery), so a **Token Monitor** device appears under the MQTT integration without any HA config.

| Entity | Type | Notes |
| --- | --- | --- |
| Session usage | sensor, % | 5-hour window |
| Week usage | sensor, % | 7-day window (all models) |
| Session resets | sensor, timestamp | |
| Week resets | sensor, timestamp | |
| Usage stale | binary sensor (problem), diagnostic | The service served cached data because its upstream call failed |
| Battery | sensor, % | `unknown` while on USB (see below) |
| Battery voltage | sensor, V, diagnostic | `unknown` while on USB |
| USB power | binary sensor (plug) | On whenever USB is connected; the firmware can't tell charging from charged |
| Wi-Fi signal | sensor, dBm, diagnostic | |

Topics, with `<id>` = `tokenmon-` plus the last three bytes of the MAC:

- `homeassistant/<sensor|binary_sensor>/<id>/<key>/config` — discovery, retained. Sent once per boot.
- `<id>/usage` — `{"session":7,"week":1,"session_resets":"…","week_resets":"…","stale":false}`. Only published when the GET succeeded.
- `<id>/device` — `{"battery":87,"voltage":3.91,"usb":false,"rssi":-62}`. Published every poll that joins Wi-Fi, even if the service is down.

Behaviour worth knowing:

- **No last will.** The device is only connected for a moment each poll, so there's no live connection for an MQTT last will to watch. Instead, every entity has `expire_after` set to three poll intervals (180 s by default). If three polls in a row go unreported, HA marks the entities unavailable. The usage entities also go unavailable if the service is down for that long.
- **State isn't retained.** With `expire_after`, HA would replay a retained value as fresh after its own restart, however old it was. After an HA restart the entities show `unknown` until the next poll.
- **Battery is `unknown` on USB.** With USB in, the ADC reads the ~4.9 V USB rail rather than the cell, so neither the voltage nor a percentage means anything (see [Battery](#battery)).
- **Delivery is confirmed before the radio goes off.** QoS 0 publishes return as soon as lwIP has queued the bytes, and turning Wi-Fi off discards anything not yet sent. Discovery is several KB, so in testing most of it was lost this way. The firmware sends `DISCONNECT` itself and waits up to 3.5 s (a bit over one lwIP retransmit) for the broker to close the connection. Mosquitto does that only after reading everything sent before it. Serial shows `[mqtt] published ok`, or `FAILED` if the close never came. If discovery isn't confirmed, it is sent again on the next poll.

Removing the device from HA also needs its retained discovery configs cleared from the broker, or HA recreates it:

```
mosquitto_pub -h <broker> -u <user> -P <pass> -r -n -t 'homeassistant/sensor/<id>/battery/config'   # repeat per entity
```


## Battery

The board runs off a 3.7 V LiPo on its battery connector, and charges it whenever USB is plugged in. **Check the polarity before plugging a pack in.** Generic packs often have the wires the opposite way round to the `+`/`-` marked on the board, and reversing them kills it.

How the firmware saves power:

- **CPU at 80 MHz** instead of 240 MHz. Wi-Fi and the 40 MHz display SPI bus both still work.
- **Light sleep between countdown ticks.** The Arduino core has automatic power management turned off (`CONFIG_PM_ENABLE` is not set), so `delay()` never lets the chip sleep. The main loop calls `esp_light_sleep_start()` instead, which keeps RAM, pin states and the LCD contents.
- **Wi-Fi off between polls.** Each poll powers up Wi-Fi, associates, makes the GET, publishes to Home Assistant if configured, and shuts Wi-Fi down again. That takes about 0.4 s per poll measured on serial (`[net] radio on Nms`), about 0.8 s with Home Assistant reporting, and about 3 s for the first poll after boot. A failed connect gives up after 10 s instead of blocking. A lost packet still costs a 3 s TCP retransmit with the radio on, or a missed publish. In testing, 3 of the first 4 polls after one flash had trouble (one failed broker connect, two lost or delayed packets), then 7 polls in a row after the next flash were clean.
- **Dimmable backlight.** The backlight PWM is clocked from the RTC 8 MHz oscillator, which keeps running during light sleep. The default APB clock would stop and leave the backlight stuck fully on or off.

How the battery indicator works ([src/battery.cpp](src/battery.cpp)):

- **Where it reads:** the board's supply rail reaches GPIO34 through a 100k/100k divider. That divider only conducts while GPIO14 is high, so the firmware switches it on just for the reading.
- **When it reads:** once per poll, before Wi-Fi powers up, because transmit bursts briefly pull the voltage down. It averages 32 samples.
- **Charging detection:** with USB plugged in, the rail reads about 4.9 V instead of the cell voltage. No LiPo goes above 4.2 V, so any reading over 4.5 V means USB, which the display shows as charging. The charger chip's status pin only drives the blue LED and isn't wired to the ESP32, so the voltage is the only signal available.
- **Limits of that approach:** the cell's own charge level can't be read while USB is connected. A fully charged battery on USB still shows `Charging`, and so does a board running on USB with no battery fitted.
- **Percentage:** voltage is mapped onto a typical LiPo discharge curve. 0% is set at 3.4 V, not the cell's 3.0 V minimum, because the board's 3.3 V regulator stops working below about 3.4 V.

Two details in [src/net.cpp](src/net.cpp) are there for this cycle, so don't remove them:

- Connection is judged by the IP address as well as `WiFi.status()`. Turning Wi-Fi off and then sleeping leaves a stale `WL_CONNECTED` from the previous session.
- There's a 100 ms pause after DHCP, a 1 s TCP connect timeout and one retry of the GET. Without them, the first packets after joining the network were often dropped, and each drop cost a 3–5 s retransmit with the radio on.

Estimated runtime on a 3000 mAh cell, using about 2700 mAh before the 3.3 V regulator drops out. These are estimates from typical ESP32 and board figures plus the measured radio-on time, not a current measurement:

| | Average draw | Runtime |
| --- | --- | --- |
| Old firmware: 240 MHz, Wi-Fi always on, 10 s poll, backlight 100% | ~70 mA | ~1.5 days |
| Current, `BACKLIGHT_PERCENT` 100 | ~25 mA | ~4.5 days |
| Current, `BACKLIGHT_PERCENT` 30 (default) | ~11 mA | ~10 days |
| Current, `BACKLIGHT_PERCENT` 10 | ~7 mA | ~16 days |

Polling every 10 s instead of 60 s adds roughly 4 mA, about 3 days less at the default brightness.

Home Assistant reporting roughly doubles the radio-on time per poll (about 0.4 s → 0.8 s), which by the same estimate adds about 0.8 mA, roughly 0.7 days less at the default brightness. The table above is without it.

## Files

- [platformio.ini](platformio.ini) — board, framework, TFT_eSPI pin config (no manual `User_Setup.h` editing)
- [src/main.cpp](src/main.cpp) — setup + loop, fetch scheduling, countdown ticking, light sleep between ticks
- [src/net.cpp](src/net.cpp) — per-poll Wi-Fi up/down + HTTP GET + ArduinoJson parse (filtered), RSSI of the last connect
- [src/ha.cpp](src/ha.cpp) — Home Assistant MQTT discovery + state publishing (PubSubClient), confirmed before the radio goes off
- [src/display.cpp](src/display.cpp) — bar/Wi-Fi/battery/countdown/error rendering, layout constants, backlight PWM
- [src/battery.cpp](src/battery.cpp) — battery voltage sampling, LiPo voltage-to-percent curve, USB (charging) detection
- [include/secrets.h.example](include/secrets.h.example) — template for `secrets.h`
