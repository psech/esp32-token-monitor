# esp32-token-monitor — service

Tiny Node 24 + Fastify service that exposes Claude Code's rate-limit utilization on the LAN so the ESP32 firmware can render it as two progress bars.

See [docs/usage-endpoint.md](docs/usage-endpoint.md) for how the upstream call works.

## API

- `GET /healthz` → `{ ok: true }`
- `GET /api/usage` → live values, served from a 60 s in-memory cache
  ```json
  {
    "session": { "percent": 7,  "resetsAt": "2026-04-26T13:30:00.000Z", "status": "allowed" },
    "week":    { "percent": 1,  "resetsAt": "2026-05-03T18:00:00.000Z", "status": "allowed" },
    "fetchedAt": "2026-04-26T11:35:00.000Z",
    "stale": false
  }
  ```
  `stale: true` means the upstream call failed and the previous cached value is being served.

## One-time setup

1. **Install deps** (host, for `npm run dev`):
   ```
   npm install
   ```
2. **Extract OAuth credentials from the keychain** (host):
   ```
   bin/refresh-token.sh
   ```
   Writes `service/.secrets/credentials.json` (gitignored, mode 600). See [Token expiry](#token-expiry) for how to keep it fresh.

## Run locally (host)

```
npm run dev
curl localhost:8787/api/usage
```

## Run in Docker

```
bin/refresh-token.sh
docker compose up --build
curl localhost:8787/api/usage
```

The compose file mounts:
- `./.secrets/` → `/run/secrets/` (rw — directory, not single file, so that atomic `mv` in `bin/refresh-token.sh` is visible to the container without a restart)
- `/usr/local/share/netskope-cert-bundle.pem` → `/etc/ssl/certs/netskope-ca.pem` (ro, picked up via `NODE_EXTRA_CA_CERTS`)

Without the cert mount, outbound HTTPS to `api.anthropic.com` fails with a self-signed-cert error (Netskope intercepts).

## Token expiry

The OAuth access token in `credentials.json` has a short lifetime (the `expiresAt` field, unix ms — typically a few hours after issue). When it expires, every upstream call returns 401 and the service starts logging `upstream refresh failed` with the Anthropic error body. `/api/usage` keeps responding 200 with `stale: true` from the last good cache, and the ESP32 shows the persistent red bar at the top of the screen.

There is no automatic refresh today. To recover:

1. **Open the Claude Code app or run `claude` once on the host.** This is what actually mints a new access token from the stored refresh token and writes it back to the macOS Keychain. Without this step, the next step just re-copies the same expired blob.
2. **Re-run the extraction script:**
   ```
   bin/refresh-token.sh
   ```
   The service re-reads `credentials.json` on every cache miss, so no restart is needed — the next `/api/usage` request will succeed.

In-process refresh (skipping the `claude` step entirely by calling Anthropic's OAuth refresh endpoint directly) is a planned follow-up.

## Cost note

Each upstream refresh sends one ~10-token Haiku request. At a 60 s cache TTL that's ~1 call/min. Negligible against weekly limits, but not zero.

## Configuration

| Env var | Default | Notes |
| --- | --- | --- |
| `PORT` | `8787` | HTTP listen port |
| `HOST` | `0.0.0.0` | Bind address |
| `CREDENTIALS_FILE` | `./.secrets/credentials.json` | Path to the keychain-extracted blob |
| `UPSTREAM_CACHE_TTL_MS` | `60000` | How long to cache the rate-limit headers |
| `NODE_EXTRA_CA_CERTS` | _unset_ | Set to the Netskope cert path inside the container |
| `LOG_LEVEL` | `info` | Fastify logger level |
