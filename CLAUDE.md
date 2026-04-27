# Repo guide for Claude Code

Three-folder project: a Node service on the laptop, ESP32 firmware that polls it, and a Vue webpage that mirrors the device.

```
service/    Node 24 + Fastify, Dockerised. Reads OAuth token from macOS Keychain, calls api.anthropic.com, exposes /api/usage on port 8787.
firmware/   PlatformIO project for TTGO T-Display ESP32. Polls the service, renders two progress bars.
webpage/    Vue 3 + Vite + Tailwind v4 SPA, served by nginx in its own compose stack at localhost:8080. Same /api/usage data as the device.
```

## Compose topology

The service and webpage run in **separate `docker compose` stacks** that share an external Docker network:

```
docker network create esp32-monitor-shared    # one-time
cd service  && docker compose up -d --build
cd webpage  && docker compose up -d --build
```

Both stacks declare the network as `external: true` with name `esp32-monitor-shared`. The webpage's nginx reverse-proxies `/api/*` to the service container by name (`http://monitor:8787/...`). The browser never sees the service origin → no CORS plumbing.

Don't merge the two compose files — the user explicitly wants independent lifecycles. Don't switch to `host.docker.internal` either — the shared-network design is the contract.

## How the data flows (the non-obvious bit)

Anthropic returns rate-limit utilization in **response headers** (`anthropic-ratelimit-unified-{5h,7d}-utilization`) on every `/v1/messages` call — not in a separate endpoint, not in the body. The service makes a 1-token Haiku ping every cache-miss to harvest those headers. See [service/docs/usage-endpoint.md](service/docs/usage-endpoint.md) for the full discovery notes — read this before changing the upstream call.

## Node version

Project requires Node 24.15. Host has nvm; CI/dev should `nvm use` (an `.nvmrc` is at the repo root). Direct binary path if needed: `~/.nvm/versions/node/v24.15.0/bin/node`.

## PlatformIO

Not on PATH by default. CLI lives at `~/.platformio/penv/bin/pio`. Common commands run from `firmware/`:

```
pio run                    # compile
pio run -t upload          # flash
pio device monitor         # serial @ 115200, with esp32_exception_decoder filter
```

`upload_speed = 460800` in [firmware/platformio.ini](firmware/platformio.ini) — TTGO T-Display drops packets at 921600. Don't bump it back up.

TFT_eSPI pin config is via `build_flags` in `platformio.ini`, **not** by editing `User_Setup.h`. Touching `User_Setup.h` won't take effect.

## Secrets — never commit

| Path | What | Source |
| --- | --- | --- |
| `firmware/include/secrets.h` | Wi-Fi creds + service host | `cp` from `secrets.h.example`, hand-edited |
| `service/.secrets/credentials.json` | Claude Code OAuth blob | `service/bin/refresh-token.sh` (pulls from keychain) |
| `service/.env` | Local dev overrides | `cp` from `.env.example` if needed |

All three are in `.gitignore`. The `.example` versions are the templates that get committed.

## Token refresh (manual for now)

The OAuth token expires (`expiresAt` in the credentials blob, unix ms). When the service starts logging upstream 401s, re-run `service/bin/refresh-token.sh` — it re-extracts from the keychain *after* `claude` itself has refreshed. Automating this in-process is a deferred follow-up.

## Netskope CA

Outbound HTTPS goes through corporate TLS interception. The Netskope cert bundle at `/usr/local/share/netskope-cert-bundle.pem` must be either passed via `NODE_EXTRA_CA_CERTS` (host dev) or mounted into the container at `/etc/ssl/certs/netskope-ca.pem` (Docker — already wired up in [service/docker-compose.yml](service/docker-compose.yml)). Without it, calls to `api.anthropic.com` fail with self-signed-cert errors.

## Cache + polling

- Service caches upstream responses for `UPSTREAM_CACHE_TTL_MS` (default 60 s). Don't drop this below ~10 s without thinking about token cost.
- ESP32 polls every `POLL_INTERVAL_MS` from `firmware/include/secrets.h` (default 10 s).
- Worst-case display lag = service cache + poll interval ≈ 70 s. This is by design.
- The 1 px countdown bar at the bottom of the LCD derives its step from `POLL_INTERVAL_MS / SCREEN_W` — change one and the other adjusts.

## On-device error signal

Persistent red 1 px bar at the **top** of the LCD = last fetch failed OR service returned `stale: true`. It only clears on the next fresh successful response. Don't make it transient — the user explicitly asked for a sticky indicator.

## Verifying changes

Service:
```
cd service
nvm use && npm run dev                              # host dev, picks up .env
docker compose up --build                           # full container path
curl -sS http://localhost:8787/api/usage | jq       # smoke test
```

Firmware (requires hardware on USB):
```
cd firmware
pio run -t upload && pio device monitor
```

Look for `[wifi] connected` then `[usage] session=N% week=N% stale=0` lines.

## House style

- TypeScript strict mode is on; keep it that way.
- No comments explaining what code does — only why if it's non-obvious. Existing modules follow this.
- Don't add libraries casually. Service has Fastify only; firmware has TFT_eSPI + ArduinoJson only.
- Don't add a "framework" abstraction for a second device or a second API consumer — there is one device and one consumer; YAGNI.

## Keep READMEs in sync

When you change code, update the corresponding README in the same change:

- Edits under `service/` → update [service/README.md](service/README.md) (env vars, endpoints, response shape, setup steps).
- Edits under `firmware/` → update [firmware/README.md](firmware/README.md) (config macros, layout, build/flash flow).
- Edits under `webpage/` → update [webpage/README.md](webpage/README.md) (env vars, theme, build/run).
- Architectural changes (data flow, components, scope) → update [README.md](README.md) at the repo root.

A change is incomplete if the README still describes the old behavior. Check both the root and the folder README for stale references before declaring done.

## Out of scope (don't add unless asked)

WebSocket push, OTA updates, TLS on the LAN, multi-device, history persistence, automatic OAuth token refresh in-process (it's a planned follow-up but not yet scoped).
