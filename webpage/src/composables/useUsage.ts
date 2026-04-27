import { ref, onMounted, onUnmounted, type Ref } from "vue";

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

export const POLL_INTERVAL_MS = 10_000;

export function useUsage(): {
  usage: Ref<Usage | null>;
  stale: Ref<boolean>;
  lastError: Ref<string | null>;
  lastFetchAt: Ref<number>;
  fetchCounter: Ref<number>;
} {
  const usage = ref<Usage | null>(null);
  const stale = ref<boolean>(false);
  const lastError = ref<string | null>(null);
  const lastFetchAt = ref<number>(Date.now());
  const fetchCounter = ref<number>(0);

  let intervalId: number | null = null;

  async function fetchOnce() {
    try {
      const res = await fetch("/api/usage", { cache: "no-store" });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = (await res.json()) as Usage;
      usage.value = data;
      stale.value = data.stale === true;
      lastError.value = data.stale ? "service returned stale data" : null;
    } catch (err) {
      stale.value = true;
      lastError.value = (err as Error).message;
    } finally {
      lastFetchAt.value = Date.now();
      fetchCounter.value += 1;
    }
  }

  onMounted(() => {
    fetchOnce();
    intervalId = window.setInterval(fetchOnce, POLL_INTERVAL_MS);
  });

  onUnmounted(() => {
    if (intervalId !== null) clearInterval(intervalId);
  });

  return { usage, stale, lastError, lastFetchAt, fetchCounter };
}
