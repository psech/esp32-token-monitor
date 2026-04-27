# Anthropic usage data — discovery notes

Captured 2026-04-26. Verified by direct `curl` from the host with the OAuth token from the macOS keychain.

## TL;DR

Rate-limit utilization is returned in **response headers** on every `/v1/messages` call, not in a separate endpoint. To refresh, the service makes one minimal `/v1/messages` request and parses the headers.

## Endpoint

```
POST https://api.anthropic.com/v1/messages
```

Headers we send:

| Header | Value |
| --- | --- |
| `Authorization` | `Bearer <accessToken>` from keychain item `Claude Code-credentials` → `claudeAiOauth.accessToken` |
| `anthropic-version` | `2023-06-01` |
| `anthropic-beta` | `oauth-2025-04-20` (required for OAuth-token auth, otherwise 401) |
| `Content-Type` | `application/json` |

Body — smallest possible, just enough to elicit a response:

```json
{
  "model": "claude-haiku-4-5",
  "max_tokens": 1,
  "messages": [{"role": "user", "content": "."}]
}
```

Cost note: this counts against the subscription's rate-limit pool (~9 input + 1 output tokens per refresh — negligible relative to weekly limits, but not free). Service caches responses for 60 s so ESP32's 10 s polls don't translate to 10 s upstream polls.

## Response headers we care about

```
anthropic-ratelimit-unified-5h-utilization: 0.06       # session %, 0..1
anthropic-ratelimit-unified-5h-reset:        1777210200 # unix seconds
anthropic-ratelimit-unified-5h-status:       allowed | warning | rejected

anthropic-ratelimit-unified-7d-utilization: 0.01       # week %, 0..1
anthropic-ratelimit-unified-7d-reset:        1777586400 # unix seconds
anthropic-ratelimit-unified-7d-status:       allowed | warning | rejected
```

Other headers seen, not used by us:
- `anthropic-ratelimit-unified-status` — overall worst-of
- `anthropic-ratelimit-unified-representative-claim` — which window is the binding constraint (`five_hour` or `seven_day`)
- `anthropic-ratelimit-unified-fallback-percentage` — fallback model availability
- `anthropic-ratelimit-unified-overage-status` / `…-overage-disabled-reason` — whether overage billing is enabled
- `anthropic-organization-id` — confirms which org we're hitting (matches `organizationUuid` from credentials)

## Service-internal mapping

```
session.percent = round(parseFloat(5h-utilization) * 100)
session.resetsAt = new Date(parseInt(5h-reset) * 1000).toISOString()
week.percent    = round(parseFloat(7d-utilization) * 100)
week.resetsAt   = new Date(parseInt(7d-reset) * 1000).toISOString()
```

## Why not other approaches

- **Aggregate from JSONL transcripts** (`~/.claude/projects/**/*.jsonl`): transcripts store `usage.input_tokens` etc. per assistant turn but **not** the server's `rate_limits` field. We'd have to know the absolute limits to compute %, and they vary per tier/account. Rejected.
- **Hit a dedicated `/usage` endpoint**: there isn't one — `/usage` in the TUI just consumes the headers from the most recent API response.
- **OpenTelemetry export**: works, but overkill — requires running a collector and configuring Claude Code to emit. The header approach is simpler.

## Token refresh

Credentials JSON includes `expiresAt` (unix ms) and a `refreshToken`. When the access token is within ~5 min of expiry, `UsageFetcher` exchanges the refresh token for a new pair (refresh endpoint discovered separately if/when needed — for v1, we surface a clear "run `bin/refresh-token.sh`" error and rely on the user re-extracting from keychain after `claude` itself refreshes). Refresh-flow automation is a follow-up.

## Corporate TLS interception

Outbound HTTPS goes through Netskope. Bundle: `/usr/local/share/netskope-cert-bundle.pem`. Docker compose mounts it into the container at `/etc/ssl/certs/netskope-ca.pem` and Node picks it up via `NODE_EXTRA_CA_CERTS`. Verified working with `--cacert` on a manual curl from the host.
