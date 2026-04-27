import Fastify from "fastify";
import { UsageFetcher } from "./usage-fetcher.js";
import { registerUsageRoute } from "./routes/usage.js";

const PORT = Number(process.env.PORT ?? 8787);
const HOST = process.env.HOST ?? "0.0.0.0";
const CREDENTIALS_FILE = process.env.CREDENTIALS_FILE ?? "./.secrets/credentials.json";
const CACHE_TTL_MS = Number(process.env.UPSTREAM_CACHE_TTL_MS ?? 60_000);
const LOG_LEVEL = process.env.LOG_LEVEL ?? "info";

const app = Fastify({ logger: { level: LOG_LEVEL } });

const fetcher = new UsageFetcher(CREDENTIALS_FILE, CACHE_TTL_MS, app.log);

app.get("/healthz", async () => ({ ok: true }));
registerUsageRoute(app, fetcher);

app.listen({ port: PORT, host: HOST }).catch((err) => {
  app.log.error(err);
  process.exit(1);
});
