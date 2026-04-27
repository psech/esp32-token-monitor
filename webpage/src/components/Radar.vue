<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted } from "vue";
import { POLL_INTERVAL_MS } from "../composables/useUsage";

const props = defineProps<{
  lastFetchAt: number;
  fetchCounter: number;
}>();

const angle = ref(0);
const blipKey = ref(0);

let rafId: number | null = null;

function tick() {
  const elapsed = Date.now() - props.lastFetchAt;
  const fraction = (elapsed % POLL_INTERVAL_MS) / POLL_INTERVAL_MS;
  angle.value = fraction * 360;
  rafId = requestAnimationFrame(tick);
}

onMounted(() => { rafId = requestAnimationFrame(tick); });
onUnmounted(() => { if (rafId !== null) cancelAnimationFrame(rafId); });

watch(() => props.fetchCounter, () => { blipKey.value += 1; });
</script>

<template>
  <svg
    width="60"
    height="60"
    viewBox="0 0 60 60"
    role="img"
    aria-label="poll countdown radar"
  >
    <circle cx="30" cy="30" r="28" fill="none" :stroke="'var(--color-radar-ring)'" stroke-width="1" />
    <circle cx="30" cy="30" r="20" fill="none" :stroke="'var(--color-radar-ring)'" stroke-width="0.5" opacity="0.5" />
    <circle cx="30" cy="30" r="12" fill="none" :stroke="'var(--color-radar-ring)'" stroke-width="0.5" opacity="0.4" />
    <circle cx="30" cy="30" r="1.5" fill="var(--color-radar-arm)" />
    <g :style="{ transform: `rotate(${angle}deg)`, transformOrigin: '30px 30px' }">
      <line x1="30" y1="30" x2="30" y2="3" :stroke="'var(--color-radar-arm)'" stroke-width="1.5" stroke-linecap="round" />
    </g>
    <circle
      :key="blipKey"
      cx="30"
      cy="30"
      r="3"
      fill="none"
      :stroke="'var(--color-radar-arm)'"
      stroke-width="1"
      class="blip"
    />
  </svg>
</template>
