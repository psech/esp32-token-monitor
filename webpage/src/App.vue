<script setup lang="ts">
import { computed } from "vue";
import UsageBar from "./components/UsageBar.vue";
import Radar from "./components/Radar.vue";
import StaleBadge from "./components/StaleBadge.vue";
import ThemeToggle from "./components/ThemeToggle.vue";
import { useUsage } from "./composables/useUsage";
import { useTheme } from "./composables/useTheme";

const { usage, stale, lastError, lastFetchAt, fetchCounter } = useUsage();
const { mode, effective, cycle } = useTheme();

const sessionPct = computed(() => usage.value?.session.percent ?? 0);
const weekPct    = computed(() => usage.value?.week.percent ?? 0);
const sessionResetsAt = computed(() => usage.value?.session.resetsAt ?? "");
const weekResetsAt    = computed(() => usage.value?.week.resetsAt ?? "");
const fetchedAt       = computed(() => usage.value?.fetchedAt ?? null);
</script>

<template>
  <main class="min-h-dvh w-full flex items-center justify-center p-4">
    <section
      class="w-full max-w-[640px] rounded-md border p-6 flex flex-col gap-6"
      :style="{ background: 'var(--color-card)', borderColor: 'var(--color-card-border)' }"
    >
      <header class="flex items-baseline justify-between">
        <h1 class="text-[14px] uppercase tracking-[0.25em]">ESP32 Token Monitor</h1>
        <span
          class="text-[11px] uppercase tracking-wider"
          :style="{ color: 'var(--color-muted)' }"
        >
          /usage mirror
        </span>
      </header>

      <UsageBar
        label="Current session"
        :percent="sessionPct"
        :resets-at="sessionResetsAt"
      />
      <UsageBar
        label="Current week (all models)"
        :percent="weekPct"
        :resets-at="weekResetsAt"
      />

      <footer class="flex items-end justify-between gap-4 mt-2">
        <div class="flex items-end gap-2">
          <StaleBadge
            v-if="stale"
            :message="lastError"
            :fetched-at="fetchedAt"
          />
          <ThemeToggle :mode="mode" :effective="effective" @cycle="cycle" />
        </div>
        <Radar :last-fetch-at="lastFetchAt" :fetch-counter="fetchCounter" />
      </footer>
    </section>
  </main>
</template>
