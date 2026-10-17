<script setup lang="ts">
// The device mock-up: screen plus footswitch board, in editing form or the live Play-mode form.
import { computed } from 'vue'
import DeviceScreen from './DeviceScreen.vue'
import FootswitchBoard from './FootswitchBoard.vue'
import PlayDeviceScreen from './PlayDeviceScreen.vue'
import PlayFootswitchBoard from './PlayFootswitchBoard.vue'
import { useDeviceStore } from '../stores/device'
import { usePlayStore } from '../stores/play'

const play = usePlayStore()
const device = useDeviceStore()
// The panel is the unit's face to scale: its aspect ratio is the unit's, and everything inside is placed in mm.
const panelStyle = computed(() => ({
  background: device.unitColour,
  aspectRatio: `${device.profile.unit.widthMm} / ${device.profile.unit.heightMm}`,
}))
</script>

<template>
  <section class="device-panel" aria-label="Device layout" :style="panelStyle">
    <template v-if="play.active">
      <PlayDeviceScreen />
      <PlayFootswitchBoard />
    </template>
    <template v-else>
      <DeviceScreen />
      <FootswitchBoard />
    </template>
  </section>
</template>
