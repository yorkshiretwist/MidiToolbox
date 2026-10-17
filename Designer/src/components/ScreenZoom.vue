<script setup lang="ts">
// The zoomed screen preview: the device screen at a size where the layout can
// actually be read. Opened by double-clicking the screen on the unit or by the
// Zoom button in the Screen pane; closed with Esc, a click outside, or Close.
import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import Notice from './Notice.vue'
import ScreenPreview from './ScreenPreview.vue'
import { confirmState } from '../lib/confirm'
import { screenDims } from '../lib/geometry'
import { cellSize, gridCols, screenFits } from '../lib/layout'
import { useDesignStore } from '../stores/design'

const store = useDesignStore()
const closeButton = ref<HTMLButtonElement | null>(null)

const dims = computed(() => screenDims(store.screenSpec, store.design.screen.orientation))
const cols = computed(() => Math.max(1, gridCols(store.analysis.grid)))
const rows = computed(() => Math.max(1, store.analysis.grid.rows))
const cell = computed(() => cellSize(store.screenSpec, store.design.screen.orientation, cols.value, rows.value))
const fits = computed(() => screenFits(store.screenSpec, store.design.screen.orientation, cols.value, rows.value))
const aspect = computed(() => dims.value.resolution[0] / dims.value.resolution[1])

function onKeydown(event: KeyboardEvent) {
  if (event.key === 'Escape' && store.zoomOpen && !confirmState.open) {
    event.preventDefault()
    store.closeZoom()
  }
}
onMounted(() => window.addEventListener('keydown', onKeydown))
onUnmounted(() => window.removeEventListener('keydown', onKeydown))
watch(
  () => store.zoomOpen,
  (open) => {
    if (open) nextTick(() => closeButton.value?.focus())
  },
)
</script>

<template>
  <Teleport to="body">
    <div v-if="store.zoomOpen" class="overlay overlay--zoom">
      <div class="overlay__scrim" @click="store.closeZoom()" />
      <div class="zoom" role="dialog" aria-modal="true" aria-labelledby="zoom-title">
        <div id="zoom-title" class="dialog__titlebar">
          Screen preview · {{ store.screenSpec.name }} · {{ dims.resolution[0] }} × {{ dims.resolution[1] }} px · {{ store.design.screen.orientation }}
        </div>
        <div class="zoom__body">
          <div class="zoom__screen" :style="{ '--aspect': aspect }">
            <ScreenPreview />
          </div>
          <p class="zoom__facts">
            {{ cols }} cells across and {{ rows }} {{ rows === 1 ? 'row' : 'rows' }}, one cell for each performance button, arranged as on the unit
            (a row offset by half a button spacing is drawn offset by half a cell). Each cell is {{ Math.round(cell.w) }} × {{ Math.round(cell.h) }} px.
            The names, colours and text are samples: what you see here is the layout.
          </p>
          <Notice v-if="!fits" kind="warn">
            Cells this small (under 44 × 40 px) are hard to read on the real screen. Try a bigger screen, fewer columns or rows, or the other orientation.
          </Notice>
          <div class="zoom__actions">
            <button ref="closeButton" type="button" class="btn btn--revert" @click="store.closeZoom()">Close</button>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>
