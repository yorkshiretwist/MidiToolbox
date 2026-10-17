<script setup lang="ts">
// The screen module drawn on the unit at its real size. Double-click it to open
// the zoomed preview.
import { computed } from 'vue'
import ScreenPreview from './ScreenPreview.vue'
import { screenDims } from '../lib/geometry'
import { useDesignStore } from '../stores/design'

const store = useDesignStore()

const dims = computed(() => screenDims(store.screenSpec, store.design.screen.orientation))
const label = computed(
  () => `${store.screenSpec.name} · ${dims.value.module[0]} × ${dims.value.module[1]} mm` + (store.analysis.screenConflict ? ' · does not fit' : ''),
)
</script>

<template>
  <div
    class="u-screen"
    :class="{ 'u-screen--conflict': store.analysis.screenConflict }"
    :style="{
      '--x': store.design.screen.x,
      '--y': store.design.screen.y,
      '--mw': dims.module[0],
      '--mh': dims.module[1],
      '--aw': dims.active[0],
      '--ah': dims.active[1],
    }"
    title="Double-click to zoom"
    data-screen
    @dblclick="store.openZoom()"
  >
    <div class="u-screen__active"><ScreenPreview /></div>
    <span class="u-screen__label">{{ label }}</span>
  </div>
</template>
