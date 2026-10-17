<script setup lang="ts">
// A replica of the device's screen, laid out in the firmware's own pixel
// measurements (Screen.h): a 28 px header, a 40 px sub-header, a 34 px parameter
// row, then the button grid with 4 px gutters. The grid mirrors the performance
// buttons, arranged as on the unit's face: every cell is two half-columns wide,
// so a staggered row sits half a cell along from its neighbours.
//
// Every length is expressed in screen pixels via `--u` (one screen pixel as a
// length), so the same component draws the tiny screen on the unit and the big
// zoomed preview. It must sit inside a container-type element that has the
// screen's aspect ratio.
import { computed } from 'vue'
import { screenDims } from '../lib/geometry'
import { performanceButtons } from '../lib/layout'
import { useDesignStore } from '../stores/design'

const store = useDesignStore()

const SAMPLE = [
  '--sample-brown',
  '--sample-indigo',
  '--sample-crimson',
  '--sample-purple',
  '--sample-teal',
  '--sample-navy',
  '--sample-plum',
  '--sample-forest',
]

const resolution = computed(() => screenDims(store.screenSpec, store.design.screen.orientation).resolution)
const grid = computed(() => store.analysis.grid)
const cells = computed(() => {
  const perf = performanceButtons(store.design)
  const lowest = Math.min(...perf.map((b) => b.id))
  const placed = new Map(grid.value.cells.map((c) => [c.uid, c]))
  return perf.map((b) => ({
    key: b.uid,
    name: String(b.id + 1),
    colour: SAMPLE[b.id % SAMPLE.length]!,
    on: b.id === lowest,
    col: `${placed.get(b.uid)!.halfCol + 1} / span 2`,
    row: placed.get(b.uid)!.row + 1,
  }))
})
</script>

<template>
  <div class="sp" :style="{ '--lx': resolution[0], '--ly': resolution[1] }">
    <div class="sp__bar sp__bar--header"><span>Config</span><span>Mode</span></div>
    <div class="sp__bar sp__bar--sub"><span>Bank</span><span>Patch</span></div>
    <div class="sp__params">
      <span class="sp__pbtn sp__pbtn--arrow">◀</span>
      <span class="sp__pbtn sp__pbtn--name">Bank</span>
      <span class="sp__pbtn sp__pbtn--arrow">▶</span>
      <span class="sp__channel">Ch. 1</span>
    </div>
    <div
      class="sp__grid"
      :style="{
        gridTemplateColumns: `repeat(${grid.halfCols}, minmax(0, 1fr))`,
        gridTemplateRows: `repeat(${grid.rows}, minmax(0, 1fr))`,
      }"
    >
      <div
        v-for="cell in cells"
        :key="cell.key"
        class="sp__cell"
        :class="{ 'sp__cell--on': cell.on }"
        :style="{ '--cell-color': `var(${cell.colour})`, gridColumn: cell.col, gridRow: cell.row }"
      >
        {{ cell.name }}
      </div>
    </div>
  </div>
</template>
