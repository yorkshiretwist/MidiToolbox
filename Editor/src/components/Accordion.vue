<script setup lang="ts">
// The right-hand drill-down: breadcrumb rows for File > Mode > Bank > Patch, with the active level shown as an open pane.
import { computed } from 'vue'
import { useConfigStore } from '../stores/config'
import {
  bankPath,
  filePath,
  modePath,
  patchPath,
  type PaneLevel,
} from '../lib/selection'
import FilePane from './panes/FilePane.vue'
import SimplePane from './panes/SimplePane.vue'
import PatchPane from './panes/PatchPane.vue'
import ButtonPane from './panes/ButtonPane.vue'
import PresetPane from './panes/PresetPane.vue'

const store = useConfigStore()

// Container levels only ('button' and 'preset' are never crumbs — they're the active pane).
const LEVELS: Exclude<PaneLevel, 'button' | 'preset'>[] = ['file', 'mode', 'bank', 'patch']

interface Crumb {
  level: PaneLevel
  label: string
  value: string
  select: () => void
}

const configName = computed(() =>
  (store.fileName ?? 'untitled').replace(/\.json$/i, ''),
)

const crumbs = computed<Crumb[]>(() => {
  const p = store.parsed
  // A button's crumbs go one level deeper than its owner's own pane would
  // show — e.g. a Bank-owned button shows File/Mode/Bank, same depth as the
  // Patch pane itself, since editing a Bank's button is "one level in" from
  // browsing that Bank. A container pane shows crumbs only above itself.
  // A preset is a child of the File, so only the File crumb shows above it.
  const activeIndex =
    p.level === 'preset'
      ? 1
      : p.level === 'button'
        ? LEVELS.indexOf(p.buttonOwnerLevel!) + 1
        : LEVELS.indexOf(p.level)
  const mode = p.modeIndex != null ? store.config.modes[p.modeIndex] : undefined
  const bank =
    mode && p.bankIndex != null ? mode.banks[p.bankIndex] : undefined
  const patch =
    bank && p.patchIndex != null ? bank.patches[p.patchIndex] : undefined

  const all: Crumb[] = [
    {
      level: 'file',
      label: 'File:',
      value: configName.value,
      select: () => store.select(filePath()),
    },
    {
      level: 'mode',
      label: 'Mode:',
      value: mode?.name ?? '',
      select: () => p.modeIndex != null && store.select(modePath(p.modeIndex)),
    },
    {
      level: 'bank',
      label: 'Bank:',
      value: bank?.name ?? '',
      select: () =>
        p.modeIndex != null &&
        p.bankIndex != null &&
        store.select(bankPath(p.modeIndex, p.bankIndex)),
    },
    {
      level: 'patch',
      label: 'Patch:',
      value: patch?.name ?? '',
      select: () =>
        p.modeIndex != null &&
        p.bankIndex != null &&
        p.patchIndex != null &&
        store.select(patchPath(p.modeIndex, p.bankIndex, p.patchIndex)),
    },
  ]
  return all.slice(0, activeIndex)
})

const activeLevel = computed(() => store.parsed.level)
</script>

<template>
  <div class="pane-stack">
    <div
      v-for="crumb in crumbs"
      :key="crumb.level"
      class="pane-crumb-row"
      :data-level="crumb.level"
    >
      <button
        type="button"
        class="pane-crumb"
        :data-level="crumb.level"
        @click="crumb.select()"
      >
        <span class="pane-crumb__label">{{ crumb.label }}</span>
        <span class="pane-crumb__value">{{ crumb.value }}</span>
      </button>

      <!-- Patch pager: only while the Patch editor is minimised (a button is
           being edited) and the patch actually has more than one page. -->
      <div
        v-if="crumb.level === 'patch' && store.pageCount > 1"
        class="crumb-pager"
        role="group"
        aria-label="Button page"
      >
        <button
          type="button"
          class="crumb-pager__arrow"
          aria-label="Previous page"
          @click="store.cyclePage(-1)"
        >
          &#9664;
        </button>
        <span class="crumb-pager__value">{{ store.currentPage }} / {{ store.pageCount }}</span>
        <button
          type="button"
          class="crumb-pager__arrow"
          aria-label="Next page"
          @click="store.cyclePage(1)"
        >
          &#9654;
        </button>
      </div>
    </div>

    <!-- Distinct keys so switching level always remounts the pane rather than
         Vue reusing one SimplePane instance across the v-if/v-else-if branches
         (a reused instance can keep showing a stale child list after the last
         child of the level below is deleted). -->
    <FilePane v-if="activeLevel === 'file'" key="pane-file" />
    <SimplePane v-else-if="activeLevel === 'mode'" key="pane-mode" level="mode" />
    <SimplePane v-else-if="activeLevel === 'bank'" key="pane-bank" level="bank" />
    <PatchPane v-else-if="activeLevel === 'patch'" />
    <ButtonPane v-else-if="activeLevel === 'button'" />
    <PresetPane v-else-if="activeLevel === 'preset'" key="pane-preset" />
  </div>
</template>

<style scoped>
.pane-crumb-row {
  flex: none;
  display: flex;
  align-items: center;
}

/* Keep the pager on the same coloured bar as the collapsed Patch crumb. */
.pane-crumb-row[data-level='patch'] {
  background: var(--color-pane-patch);
}

.pane-crumb-row > .pane-crumb {
  flex: 1 1 auto;
  width: auto;
}

.crumb-pager {
  flex: none;
  display: flex;
  align-items: center;
  gap: var(--space-1);
  padding-right: var(--sidebar-padding-x);
}

.crumb-pager__arrow {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 1.75rem;
  height: 1.75rem;
  padding: 0;
  border: 0;
  border-radius: var(--radius-sm);
  background: var(--color-accent-info);
  color: var(--color-button-text);
  font-size: var(--font-size-sm);
  line-height: 1;
  cursor: pointer;
}

.crumb-pager__value {
  min-width: 3.5ch;
  text-align: center;
  font-size: var(--font-size-sm);
  font-variant-numeric: tabular-nums;
  color: var(--color-text);
}
</style>
