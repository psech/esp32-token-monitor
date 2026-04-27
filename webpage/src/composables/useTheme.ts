import { ref, watchEffect, onUnmounted, type Ref } from "vue";

export type ThemeMode = "auto" | "dark" | "light";

const STORAGE_KEY = "esp32-monitor.theme";

function readStored(): ThemeMode {
  const v = localStorage.getItem(STORAGE_KEY);
  return v === "dark" || v === "light" || v === "auto" ? v : "auto";
}

export function useTheme(): {
  mode: Ref<ThemeMode>;
  effective: Ref<"dark" | "light">;
  cycle: () => void;
} {
  const mode = ref<ThemeMode>(readStored());
  const mq = window.matchMedia("(prefers-color-scheme: dark)");
  const systemDark = ref<boolean>(mq.matches);

  const onMqChange = (e: MediaQueryListEvent) => { systemDark.value = e.matches; };
  mq.addEventListener("change", onMqChange);
  onUnmounted(() => mq.removeEventListener("change", onMqChange));

  const effective = ref<"dark" | "light">(systemDark.value ? "dark" : "light");

  watchEffect(() => {
    const eff = mode.value === "auto" ? (systemDark.value ? "dark" : "light") : mode.value;
    effective.value = eff;
    document.documentElement.classList.toggle("dark", eff === "dark");
    localStorage.setItem(STORAGE_KEY, mode.value);
  });

  function cycle() {
    mode.value = mode.value === "auto" ? "dark" : mode.value === "dark" ? "light" : "auto";
  }

  return { mode, effective, cycle };
}
