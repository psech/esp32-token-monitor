<script setup lang="ts">
import { computed } from "vue";
import type { ThemeMode } from "../composables/useTheme";

const props = defineProps<{
  mode: ThemeMode;
  effective: "dark" | "light";
}>();

const emit = defineEmits<{
  (e: "cycle"): void;
}>();

const glyph = computed(() => {
  if (props.mode === "auto") return "◐";
  if (props.mode === "dark") return "☾";
  return "☀";
});

const label = computed(() => `theme: ${props.mode}`);
</script>

<template>
  <button
    type="button"
    class="px-2 py-1 rounded-sm border text-[11px] uppercase tracking-wider flex items-center gap-2"
    :style="{ color: 'var(--color-fg)', borderColor: 'var(--color-card-border)' }"
    :title="`current: ${effective} (mode: ${mode})`"
    @click="emit('cycle')"
  >
    <span>{{ glyph }}</span>
    <span>{{ label }}</span>
  </button>
</template>
