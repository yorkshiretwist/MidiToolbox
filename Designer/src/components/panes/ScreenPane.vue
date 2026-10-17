<script setup lang="ts">
// Step 4's sidebar pane: pick a screen module and orientation, with how well the on-screen button grid fits it.
import { computed } from 'vue'
import NumField from '../NumField.vue'
import Notice from '../Notice.vue'
import PaneShell from './PaneShell.vue'
import { SCREENS } from '../../lib/catalog'
import { screenDims } from '../../lib/geometry'
import { cellSize, gridCols, screenFits } from '../../lib/layout'
import { useDesignStore } from '../../stores/design'
import type { Orientation } from '../../types/device'

const store = useDesignStore()
const design = computed(() => store.design)
const spec = computed(() => store.screenSpec)
const grid = computed(() => store.analysis.grid)
const cols = computed(() => Math.max(1, gridCols(grid.value)))
const rows = computed(() => Math.max(1, grid.value.rows))
const orientation = computed(() => design.value.screen.orientation)
const dims = computed(() => screenDims(spec.value, orientation.value))

const rowsShown = computed(() =>
  SCREENS.filter((s) => store.showUntested || s.status === 'verified' || s.id === spec.value.id).map((s) => {
    const cell = cellSize(s, orientation.value, cols.value, rows.value)
    const [lx, ly] = screenDims(s, orientation.value).resolution
    const fits = screenFits(s, orientation.value, cols.value, rows.value)
    return {
      s,
      fits,
      selected: s.id === spec.value.id,
      touch: s.touch.type === 'none' ? 'no touch' : `${s.touch.type} touch`,
      resolution: `${lx} × ${ly}`,
      cell: `${Math.round(cell.w)}×${Math.round(cell.h)} px`,
    }
  }),
)

const findings = computed(() => store.analysis.checks.filter((c) => c.step === 4 && c.severity !== 'ok'))
const verified = computed(() => store.analysis.checks.find((c) => c.key === 'screen-status')?.severity === 'ok')

const ORIENTATIONS: { value: Orientation; label: string }[] = [
  { value: 'landscape', label: 'Landscape' },
  { value: 'portrait', label: 'Portrait' },
]
</script>

<template>
  <PaneShell :n="4" title="Screen">
    <div class="seg" role="radiogroup" aria-label="Orientation">
      <button
        v-for="o in ORIENTATIONS"
        :key="o.value"
        type="button"
        class="btn btn--toggle btn--blue"
        role="radio"
        :aria-checked="orientation === o.value"
        :aria-pressed="orientation === o.value"
        @click="store.setOrientation(o.value)"
      >
        {{ o.label }}
      </button>
    </div>
    <div class="field field--inline">
      <span class="pane__hint" style="margin-right: auto">
        The on-screen grid mirrors your performance buttons: {{ cols }} cells across, {{ rows }} {{ rows === 1 ? 'row' : 'rows' }}.
      </span>
      <button type="button" class="btn btn--blue btn--small" title="See the screen large enough to read (or double-click it on the unit)" @click="store.openZoom()">Zoom</button>
      <button type="button" class="btn btn--blue btn--small" @click="store.suggest()">Suggest</button>
    </div>

    <ul class="opt-list" role="radiogroup" aria-label="Screen">
      <li v-for="row in rowsShown" :key="row.s.id">
        <button
          type="button"
          class="opt"
          :class="{ 'opt--selected': row.selected }"
          role="radio"
          :aria-checked="row.selected"
          :disabled="!row.fits && !row.selected"
          @click="store.setScreen(row.s.id)"
        >
          <span class="opt__radio" />
          <span class="opt__main">
            <span class="opt__name">{{ row.s.name }}</span>
            <span v-if="row.fits || row.selected" class="opt__meta" :class="{ 'opt__meta--warn': !row.fits }">
              {{ row.resolution }} · {{ row.touch }} · cells {{ row.cell }}{{ row.fits ? '' : ' — too small' }}
            </span>
            <span v-else class="opt__meta opt__meta--warn">Too small: cells would be {{ row.cell }}</span>
          </span>
          <span v-if="row.fits && !row.selected" class="badge badge--suggest">★ Fits</span>
          <span class="badge" :class="row.s.status === 'verified' ? 'badge--ok' : 'badge--untested'">{{ row.s.status }}</span>
        </button>
      </li>
    </ul>
    <label class="toggle-field field">
      <input v-model="store.showUntested" class="toggle-dot" type="checkbox" />
      <span class="toggle-field__label">Show untested screens</span>
    </label>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <dl class="detail-grid">
      <dt>Cut-out</dt><dd>about {{ dims.module[0] }} × {{ dims.module[1] }} mm</dd>
      <dt>Display</dt><dd>{{ dims.active[0] }} × {{ dims.active[1] }} mm</dd>
      <dt>Resolution</dt><dd>{{ dims.resolution[0] }} × {{ dims.resolution[1] }} px</dd>
      <dt>Touch</dt><dd>{{ spec.touch.type === 'none' ? 'None' : `${spec.touch.type} (${spec.touch.controller})` }}</dd>
      <dt>Pins</dt><dd>{{ spec.pins.length }}</dd>
    </dl>
    <div class="field field--inline">
      <label class="field__label">Across</label>
      <NumField label="Screen across (mm)" :model-value="design.screen.x" :min="0" @update:model-value="store.moveScreen($event, design.screen.y, true)" />
      <span class="unit-suffix">mm from the left</span>
    </div>
    <div class="field field--inline">
      <label class="field__label">Down</label>
      <NumField label="Screen down (mm)" :model-value="design.screen.y" :min="0" @update:model-value="store.moveScreen(design.screen.x, $event, true)" />
      <span class="unit-suffix">mm from the top</span>
    </div>
    <p class="pane__hint">Drag the screen on the unit to move it, or double-click it (or press Zoom) to see the layout at a readable size. Position is the module’s top-left corner. Module sizes are approximate: check your own part.</p>

    <div style="display: flex; flex-direction: column; gap: var(--space-2)">
      <Notice v-for="check in findings" :key="check.key" :kind="check.severity === 'error' ? 'error' : 'warn'">{{ check.text }}.</Notice>
      <Notice v-if="verified && findings.length === 0" kind="ok">Firmware driver verified on hardware.</Notice>
    </div>
  </PaneShell>
</template>
