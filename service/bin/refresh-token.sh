#!/usr/bin/env bash
# Extracts the Claude Code OAuth credentials blob from the macOS Keychain
# into service/.secrets/credentials.json (gitignored), where the service
# (and the docker container, which mounts this file) can read it.
#
# Re-run this whenever `claude` itself has refreshed the token (i.e. when
# the service starts logging "token expired"). One day we'll teach the
# service to refresh in-process; for v1 this is the manual step.

set -euo pipefail

KEYCHAIN_ITEM="Claude Code-credentials"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${SCRIPT_DIR}/../.secrets/credentials.json"

mkdir -p "$(dirname "$OUT")"

if ! security find-generic-password -s "$KEYCHAIN_ITEM" -w >/dev/null 2>&1; then
  echo "error: keychain item '$KEYCHAIN_ITEM' not found" >&2
  echo "       (have you signed into Claude Code on this machine?)" >&2
  exit 1
fi

security find-generic-password -s "$KEYCHAIN_ITEM" -w > "$OUT.tmp"
mv "$OUT.tmp" "$OUT"
chmod 600 "$OUT"

EXPIRES_MS=$(python3 -c "import json,sys; print(json.load(open('$OUT'))['claudeAiOauth']['expiresAt'])")
EXPIRES_HUMAN=$(python3 -c "import datetime; print(datetime.datetime.fromtimestamp($EXPIRES_MS/1000).isoformat())")
echo "wrote $OUT (token expires $EXPIRES_HUMAN)"
