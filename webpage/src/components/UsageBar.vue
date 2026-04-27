<script setup lang="ts">
import { computed } from "vue";

const props = defineProps<{
  label: string;
  percent: number;
  resetsAt: string;
}>();

const clamped = computed(() => Math.max(0, Math.min(100, props.percent)));

const fillColor = computed(() => {
  if (clamped.value >= 90) return "var(--color-bar-danger)";
  if (clamped.value >= 75) return "var(--color-bar-warn)";
  return "var(--color-bar-ok)";
});

const resetsLabel = computed(() => {
  const d = new Date(props.resetsAt);
  if (Number.isNaN(d.getTime())) return "";
  const now = new Date();
  const sameDay =
    d.getFullYear() === now.getFullYear() &&
    d.getMonth() === now.getMonth() &&
    d.getDate() === now.getDate();
  const hh = String(d.getHours()).padStart(2, "0");
  const mm = String(d.getMinutes()).padStart(2, "0");
  if (sameDay) return `resets at ${hh}:${mm}`;
  const month = d.toLocaleString(undefined, { month: "short" });
  return `resets ${month} ${d.getDate()} ${hh}:${mm}`;
});
</script>

<template>
  <div class="flex flex-col gap-1">
    <div class="flex items-baseline justify-between">
      <span class="uppercase tracking-wider text-[12px]">{{ label }}</span>
      <span class="tabular-nums text-[14px]">{{ clamped }}%</span>
    </div>
    <div
      class="h-3 w-full rounded-sm overflow-hidden"
      :style="{ background: 'var(--color-bar-track)' }"
    >
      <div
        class="h-full transition-[width] duration-300 ease-out bar-fill"
        :style="{
          width: `${clamped}%`,
          background: fillColor,
          '--bar-color': fillColor,
        }"
      />
    </div>
    <div
      class="text-[11px] uppercase tracking-wider"
      :style="{ color: 'var(--color-muted)' }"
    >
      {{ resetsLabel }}
    </div>
  </div>
</template>
