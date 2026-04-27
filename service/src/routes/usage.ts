import type { FastifyInstance } from "fastify";
import type { UsageFetcher } from "../usage-fetcher.js";

export function registerUsageRoute(app: FastifyInstance, fetcher: UsageFetcher): void {
  app.get("/api/usage", async (_req, reply) => {
    try {
      const usage = await fetcher.get();
      reply.header("Cache-Control", "no-store");
      return usage;
    } catch (err) {
      app.log.error({ err }, "usage fetch failed");
      reply.code(502);
      return { error: "upstream_unavailable", message: (err as Error).message };
    }
  });
}
