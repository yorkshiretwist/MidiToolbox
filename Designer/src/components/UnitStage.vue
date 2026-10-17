<script setup lang="ts">
// The to-scale plan view of the unit. It is also where most of the design is
// done: drag buttons and the screen around (snapping to the layout grid), click
// an empty spot to add a button, drag the handles to resize the unit, and pick
// navigation buttons by clicking them.
import { computed, onMounted, onUnmounted, ref } from 'vue'
import MiniScreen from './MiniScreen.vue'
import { buttonDiameter, clamp, rulerTicks, screenDims, snapTo, tidy } from '../lib/geometry'
import { isLight, rgbToHex } from '../lib/colour'
import { confirmState } from '../lib/confirm'
import { navRoleOf, spotIsFree } from '../lib/layout'
import { useDesignStore } from '../stores/design'

const store = useDesignStore()
const unitEl = ref<HTMLElement | null>(null)

const unit = computed(() => store.design.unit)
const xTicks = computed(() => rulerTicks(unit.value.width, unit.value.pitch, 16))
const yTicks = computed(() => rulerTicks(unit.value.height, unit.value.pitch, 11))
const stageStyle = computed(() => ({
  '--w': unit.value.width,
  '--h': unit.value.height,
  '--pitch': unit.value.pitch,
  '--aspect': unit.value.width / unit.value.height,
  '--btn': buttonDiameter(store.design.purpose),
  '--unit-colour': rgbToHex(store.design.colour),
  '--unit-grid-line': isLight(store.design.colour) ? 'rgba(0, 0, 0, 0.12)' : 'rgba(255, 255, 255, 0.07)',
}))
const stageClass = computed(() => ({
  [`stage--step-${store.step}`]: true,
  'stage--picking': store.pickingRole !== null,
}))

const buttons = computed(() =>
  store.design.buttons.map((b) => ({
    b,
    nav: navRoleOf(store.design, b.uid),
    classes: {
      [`u-btn--led-${b.led}`]: true,
      'u-btn--nav': navRoleOf(store.design, b.uid) !== undefined,
      'u-btn--selected': store.step === 2 && store.selectedUid === b.uid,
      'u-btn--tight': store.analysis.tightUids.has(b.uid),
      'u-btn--conflict': store.analysis.conflictUids.has(b.uid),
    },
  })),
)
const ROLE_LABEL = { select: 'Select', down: '◀ Down', up: 'Up ▶' } as const

// ── pointer → millimetres ────────────────────────────────────────────────
function toMm(event: PointerEvent | MouseEvent): { x: number; y: number } {
  const el = unitEl.value!
  const rect = el.getBoundingClientRect()
  // children are positioned against the padding box, i.e. inside the border
  const left = rect.left + el.clientLeft
  const top = rect.top + el.clientTop
  return {
    x: ((event.clientX - left) / el.clientWidth) * unit.value.width,
    y: ((event.clientY - top) / el.clientHeight) * unit.value.height,
  }
}

type Drag =
  | { kind: 'button'; uid: number; dx: number; dy: number }
  | { kind: 'screen'; dx: number; dy: number }
  | { kind: 'resize'; axis: 'e' | 's' | 'se' }
const drag = ref<Drag | null>(null)
const ghost = ref<{ x: number; y: number; blocked: boolean } | null>(null)

function onButtonDown(event: PointerEvent, uid: number) {
  if (store.step === 3 && store.pickingRole) {
    store.setNav(store.pickingRole, uid)
    store.pickingRole = null
    return
  }
  if (store.step !== 2) return
  event.preventDefault()
  store.select(uid)
  const button = store.design.buttons.find((b) => b.uid === uid)!
  const at = toMm(event)
  drag.value = { kind: 'button', uid, dx: at.x - button.x, dy: at.y - button.y }
  ;(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId)
  ghost.value = null
}

function onScreenDown(event: PointerEvent) {
  if (store.step !== 4) return
  event.preventDefault()
  const at = toMm(event)
  drag.value = { kind: 'screen', dx: at.x - store.design.screen.x, dy: at.y - store.design.screen.y }
  ;(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId)
}

function onHandleDown(event: PointerEvent, axis: 'e' | 's' | 'se') {
  event.preventDefault()
  drag.value = { kind: 'resize', axis }
  ;(event.currentTarget as HTMLElement).setPointerCapture(event.pointerId)
}

function onMove(event: PointerEvent) {
  const at = toMm(event)
  const d = drag.value
  if (d?.kind === 'button') {
    store.moveButton(d.uid, at.x - d.dx, at.y - d.dy)
  } else if (d?.kind === 'screen') {
    const { module } = screenDims(store.screenSpec, store.design.screen.orientation)
    const snap = (v: number) => (store.snap ? snapTo(v, unit.value.pitch) : tidy(v))
    store.moveScreen(
      clamp(snap(at.x - d.dx), 0, Math.max(0, unit.value.width - module[0])),
      clamp(snap(at.y - d.dy), 0, Math.max(0, unit.value.height - module[1])),
      true,
    )
  } else if (d?.kind === 'resize') {
    const snap = (v: number) => (store.snap ? snapTo(v, unit.value.pitch) : tidy(v))
    const width = d.axis === 's' ? unit.value.width : snap(at.x)
    const height = d.axis === 'e' ? unit.value.height : snap(at.y)
    store.setUnitSize(width, height)
  } else if (store.step === 2) {
    const x = store.snap ? snapTo(at.x, unit.value.pitch) : tidy(at.x)
    const y = store.snap ? snapTo(at.y, unit.value.pitch) : tidy(at.y)
    ghost.value = { x, y, blocked: !spotIsFree(store.design, store.screenSpec, x, y) }
  }
}

function onUp() {
  drag.value = null
}

// Double-click on empty grid adds a button there. (A single click would add
// buttons by accident, and a double-click on a button or the screen is ignored.)
function onUnitDoubleClick(event: MouseEvent) {
  if (store.step !== 2 || event.target !== unitEl.value) return
  const at = toMm(event)
  store.addButton(at.x, at.y)
}

// Delete / Backspace removes the selected button. Only on the Buttons step,
// where the selection is visible, and never while typing in a field or while a
// confirmation is open.
function onKeydown(event: KeyboardEvent) {
  if (event.key !== 'Delete' && event.key !== 'Backspace') return
  if (store.step !== 2 || store.selectedUid === null || confirmState.open || store.zoomOpen) return
  const target = event.target as HTMLElement | null
  if (target?.closest('input, textarea, select, [contenteditable="true"]')) return
  event.preventDefault()
  store.deleteButton(store.selectedUid)
}

onMounted(() => window.addEventListener('keydown', onKeydown))
onUnmounted(() => window.removeEventListener('keydown', onKeydown))
</script>

<template>
  <section class="device-panel device-panel--stage" aria-label="Unit plan view">
    <div class="stage" :class="stageClass" :style="stageStyle">
      <div class="ruler-x" aria-hidden="true">
        <span v-for="at in xTicks" :key="at" class="ruler__tick" :style="{ '--at': at }">{{ at }}</span>
      </div>
      <div class="dim dim--x" aria-hidden="true">
        <span class="dim__line" /><b>{{ unit.width }} mm</b><span class="dim__line" />
      </div>
      <div class="ruler-y" aria-hidden="true">
        <span v-for="at in yTicks" :key="at" class="ruler__tick" :style="{ '--at': at }">{{ at }}</span>
      </div>
      <div class="dim dim--y" aria-hidden="true">
        <span class="dim__line" /><b>{{ unit.height }} mm</b><span class="dim__line" />
      </div>

      <div
        ref="unitEl"
        class="unit"
        :class="{ 'unit--adding': store.step === 2 }"
        @pointermove="onMove"
        @pointerup="onUp"
        @pointercancel="onUp"
        @pointerleave="ghost = null"
        @dblclick="onUnitDoubleClick"
      >
        <div
          v-if="ghost && store.step === 2 && !drag"
          class="unit__ghost"
          :class="{ 'unit__ghost--blocked': ghost.blocked }"
          :style="{ '--x': ghost.x, '--y': ghost.y }"
        >
          {{ ghost.blocked ? '×' : '+' }}
        </div>

        <MiniScreen @pointerdown="onScreenDown" />

        <button
          v-for="item in buttons"
          :key="item.b.uid"
          type="button"
          class="u-btn"
          :class="item.classes"
          :style="{ '--x': item.b.x, '--y': item.b.y }"
          :aria-label="`Button ${item.b.id + 1}`"
          @pointerdown="onButtonDown($event, item.b.uid)"
        >
          <span class="u-btn__dome">{{ item.b.id + 1 }}</span>
          <span class="u-btn__led" />
          <span v-if="item.nav" class="u-btn__role">{{ ROLE_LABEL[item.nav] }}</span>
        </button>

        <template v-if="store.step === 1">
          <button type="button" class="unit__handle unit__handle--e" aria-label="Resize width" @pointerdown="onHandleDown($event, 'e')" />
          <button type="button" class="unit__handle unit__handle--s" aria-label="Resize height" @pointerdown="onHandleDown($event, 's')" />
          <button type="button" class="unit__handle unit__handle--se" aria-label="Resize width and height" @pointerdown="onHandleDown($event, 'se')" />
        </template>
      </div>
    </div>
  </section>
</template>
