import { readFile } from "node:fs/promises";

export type UsageWindow = {
  percent: number;
  resetsAt: string;
  status: "allowed" | "warning" | "rejected" | "unknown";
};

export type Usage = {
  session: UsageWindow;
  week: UsageWindow;
  fetchedAt: string;
  stale: boolean;
};

type Credentials = {
  claudeAiOauth: {
    accessToken: string;
    refreshToken: string;
    expiresAt: number;
  };
  organizationUuid: string;
};

type CacheEntry = { value: Usage; expiresAt: number };

const ANTHROPIC_URL = "https://api.anthropic.com/v1/messages";
const REFRESH_BODY = {
  model: "claude-haiku-4-5",
  max_tokens: 1,
  messages: [{ role: "user", content: "." }],
};

export class UsageFetcher {
  private cache: CacheEntry | null = null;
  private inflight: Promise<Usage> | null = null;

  constructor(
    private readonly credentialsFile: string,
    private readonly cacheTtlMs: number,
    private readonly log: { info: (...a: unknown[]) => void; warn: (...a: unknown[]) => void; error: (...a: unknown[]) => void },
  ) {}

  async get(): Promise<Usage> {
    const now = Date.now();
    if (this.cache && this.cache.expiresAt > now) return this.cache.value;
    if (this.inflight) return this.inflight;

    this.inflight = this.refresh().finally(() => {
      this.inflight = null;
    });
    return this.inflight;
  }

  private async refresh(): Promise<Usage> {
    const creds = await this.readCredentials();
    try {
      const fresh = await this.callAnthropic(creds.claudeAiOauth.accessToken);
      this.cache = { value: fresh, expiresAt: Date.now() + this.cacheTtlMs };
      return fresh;
    } catch (err) {
      this.log.warn("upstream refresh failed:", (err as Error).message);
      if (this.cache) {
        const stale = { ...this.cache.value, stale: true };
        return stale;
      }
      throw err;
    }
  }

  private async readCredentials(): Promise<Credentials> {
    const raw = await readFile(this.credentialsFile, "utf8");
    return JSON.parse(raw) as Credentials;
  }

  private async callAnthropic(accessToken: string): Promise<Usage> {
    const res = await fetch(ANTHROPIC_URL, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${accessToken}`,
        "anthropic-version": "2023-06-01",
        "anthropic-beta": "oauth-2025-04-20",
        "Content-Type": "application/json",
      },
      body: JSON.stringify(REFRESH_BODY),
    });

    if (!res.ok) {
      const body = await res.text().catch(() => "");
      throw new Error(`anthropic ${res.status}: ${body.slice(0, 200)}`);
    }
    await res.body?.cancel();

    const session = readWindow(res.headers, "5h");
    const week = readWindow(res.headers, "7d");
    return {
      session,
      week,
      fetchedAt: new Date().toISOString(),
      stale: false,
    };
  }
}

function readWindow(h: Headers, prefix: "5h" | "7d"): UsageWindow {
  const util = h.get(`anthropic-ratelimit-unified-${prefix}-utilization`);
  const reset = h.get(`anthropic-ratelimit-unified-${prefix}-reset`);
  const status = h.get(`anthropic-ratelimit-unified-${prefix}-status`);
  if (util === null || reset === null) {
    throw new Error(`missing rate-limit headers for ${prefix} window`);
  }
  return {
    percent: Math.round(parseFloat(util) * 100),
    resetsAt: new Date(parseInt(reset, 10) * 1000).toISOString(),
    status: normalizeStatus(status),
  };
}

function normalizeStatus(s: string | null): UsageWindow["status"] {
  if (s === "allowed" || s === "warning" || s === "rejected") return s;
  return "unknown";
}
