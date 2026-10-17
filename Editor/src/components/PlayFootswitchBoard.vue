<script setup lang="ts">
// The footswitch board in Play mode: pressable footswitches that drive the simulator in stores/play.ts.
import { computed } from 'vue'
import { usePlayStore } from '../stores/play'
import { useDeviceStore } from '../stores/device'
import { effectiveColourCode, navButtonDefaultLabel, rgbCss } from '../lib/buttons'
import { deviceBoardLayout, deviceGeometry, type BoardSlot } from '../lib/deviceLayout'

const play = usePlayStore()
const deviceStore = useDeviceStore()

const boardLayout = computed(() => deviceBoardLayout(deviceStore.profile))

const geometry = computed(() => deviceGeometry(deviceStore.profile))
const btnD = computed(() => `${(geometry.value.buttonMm / geometry.value.widthMm) * 100}cqw`)
/** Places a footswitch's centre at its real position on the unit's face. */
function posStyle(id: number) {
  const b = geometry.value.buttons.find((x) => x.id === id)!
  return { left: `${b.xPct}%`, top: `${b.yPct}%` }
}

interface Unit {
  slot: BoardSlot
  label: string
  colour: string
  bound: boolean
  on: boolean
}

const units = computed<Unit[]>(() =>
  boardLayout.value.slots.map((slot) => {
    const resolved = play.resolveAt(slot.id)
    const button = resolved?.button
    const isBuiltIn = resolved?.level === 'builtin'
    const defaultLabel = isBuiltIn ? navButtonDefaultLabel(deviceStore.profile, slot.id) : undefined
    return {
      slot,
      label: defaultLabel ?? (button?.name?.trim() || String(slot.id + 1)),
      colour: rgbCss(effectiveColourCode(button, play.bank)),
      bound: button != null,
      on: play.isOn(slot.id),
    }
  }),
)

function press(id: number) {
  play.pressPhysical(id)
}
function release(id: number) {
  play.releasePhysical(id)
}
</script>

<template>
  <div
    class="button-board"
    role="group"
    aria-label="Footswitches (play mode)"
    :style="{ '--btn-d': btnD }"
  >
    <button
      v-for="unit in units"
      :key="unit.slot.id"
      type="button"
      class="footswitch"
      :class="{ 'footswitch--active': unit.on, 'footswitch--unbound': !unit.bound }"
      :style="{
        '--button-color': unit.colour,
        ...posStyle(unit.slot.id),
      }"
      :data-button-id="unit.slot.id"
      :data-led="unit.slot.led"
      :disabled="!unit.bound"
      :aria-pressed="unit.on"
      @mousedown="press(unit.slot.id)"
      @mouseup="release(unit.slot.id)"
      @mouseleave="release(unit.slot.id)"
    >
      <template v-if="unit.slot.led === 'below'">
        <span class="footswitch__dome" aria-hidden="true"></span>
        <span class="footswitch__led" aria-hidden="true"></span>
      </template>
      <template v-else>
        <span class="footswitch__led" aria-hidden="true"></span>
        <span class="footswitch__dome" aria-hidden="true"></span>
      </template>
      <span class="footswitch__label">{{ unit.label }}</span>
    </button>
  </div>
</template>

<style scoped>
.footswitch--unbound .footswitch__label {
  opacity: 0.5;
}
</style>
