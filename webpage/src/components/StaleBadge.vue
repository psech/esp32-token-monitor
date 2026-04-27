<script setup lang="ts">
import { ref } from "vue";

const props = defineProps<{
  message: string | null;
  fetchedAt: string | null;
}>();

const expanded = ref(false);

function toggle() {
  if (props.message) expanded.value = !expanded.value;
}
</script>

<template>
  <div class="flex flex-col items-start gap-1">
    <button
      type="button"
      class="px-2 py-1 rounded-sm border text-[11px] uppercase tracking-wider flex items-center gap-1"
      :style="{ color: 'var(--color-stale)', borderColor: 'var(--color-stale)' }"
      @click="toggle"
    >
      <span>STALE DATA</span>
      <span v-if="message" class="text-[10px]">{{ expanded ? "▴" : "▾" }}</span>
    </button>
    <div
      v-if="expanded && message"
      class="px-2 py-1 rounded-sm border text-[11px] max-w-[420px]"
      :style="{ color: 'var(--color-stale)', borderColor: 'var(--color-stale)' }"
    >
      <div class="break-words">{{ message }}</div>
      <div v-if="fetchedAt" class="opacity-70 mt-1">last fetched {{ fetchedAt }}</div>
    </div>
  </div>
</template>
