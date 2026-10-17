<script setup lang="ts">
// The device's screen in edit mode: clickable File/Mode/Bank/Patch navigation drawn as the hardware shows it.
import { computed } from 'vue'
import { useConfigStore } from '../stores/config'
import { useDeviceStore } from '../stores/device'
import { effectiveColourCode, resolveButtonFrom, rgbCss } from '../lib/buttons'
import { deviceGeometry, deviceScreenLayout } from '../lib/deviceLayout'
import { contrastingTextColour } from '../lib/colour'
import { bankPath, filePath, modePath, patchPath } from '../lib/selection'

const store = useConfigStore()
const deviceStore = useDeviceStore()
const screenLayout = computed(() => deviceScreenLayout(deviceStore.profile))
const geometry = computed(() => deviceGeometry(deviceStore.profile))
/** The screen module at its real size and position on the unit's face. */
const previewStyle = computed(() => {
  const s = geometry.value.screen
  return {
    left: `${s.xPct}%`,
    top: `${s.yPct}%`,
    width: `${s.widthPct}%`,
    aspectRatio: String(s.aspect),
    '--active-frac': s.activeFrac,
    '--active-aspect': s.activeAspect,
    '--lx': s.lx,
  }
})

// The File/Mode/Bank/Patch indices for the current selection, in scope
// regardless of how deep it actually drills (e.g. still resolved while
// editing a Button). Undefined when that level isn't reachable yet (e.g.
// bankIndex while no mode is selected) - the corresponding screen button
// stays disabled until then.
const navTargets = computed(() => {
  const p = store.parsed
  return {
    modeIndex: p.modeIndex,
    bankIndex: p.bankIndex,
    patchIndex: p.patchIndex,
  }
})

function goFile() {
  store.select(filePath())
}
function goMode() {
  const { modeIndex } = navTargets.value
  if (modeIndex == null) return
  store.select(modePath(modeIndex))
}
function goBank() {
  const { modeIndex, bankIndex } = navTargets.value
  if (modeIndex == null || bankIndex == null) return
  store.select(bankPath(modeIndex, bankIndex))
}
function goPatch() {
  const { modeIndex, bankIndex, patchIndex } = navTargets.value
  if (modeIndex == null || bankIndex == null || patchIndex == null) return
  store.select(patchPath(modeIndex, bankIndex, patchIndex))
}

const configName = computed(() =>
  (store.fileName ?? 'untitled').replace(/\.json$/i, ''),
)

// SubHeader bar (Bank / Patch names): background from the selected bank's
// subheaderColourCode, defaulting to [44, 44, 44] per schema.json when the
// bank doesn't set one. Text is black or white, whichever contrasts more —
// mirrors setBarColours() in MIDI-Toolbox/Screen.h.
const SUBHEADER_FALLBACK_RGB = [44, 44, 44]
const titlebarStyle = computed(() => {
  const rgb = store.activeBank?.subheaderColourCode
  const resolved = rgb && rgb.length >= 3 ? rgb : SUBHEADER_FALLBACK_RGB
  return {
    '--titlebar-bg': rgbCss(rgb, rgbCss(SUBHEADER_FALLBACK_RGB)),
    '--titlebar-text': contrastingTextColour(resolved),
  }
})

const context = computed(() => {
  const p = store.parsed
  const mode = p.modeIndex != null ? store.config.modes[p.modeIndex] : undefined
  const bank =
    mode && p.bankIndex != null ? mode.banks[p.bankIndex] : undefined
  const patch =
    bank && p.patchIndex != null ? bank.patches[p.patchIndex] : undefined
  const channel =
    patch?.midiChannel ??
    bank?.midiChannel ??
    mode?.midiChannel ??
    store.config.midiChannel ??
    1
  return {
    mode: mode?.name ?? '',
    bank: bank?.name ?? '',
    patch: patch?.name ?? '',
    channel,
  }
})

interface Cell {
  id: number
  col: number
  row: number
  label: string
  colour: string
  /** Black or white — whichever the hardware would draw the label in. */
  textColour: string
  active: boolean
  /** Resolved from a less-specific level than the one being edited — not yet overridden here. */
  inherited: boolean
}

// The fill an unconfigured cell shows (matches rgbCss()'s #333333 fallback and
// the CSS default), so its label colour is picked against the same background.
const CELL_FALLBACK_RGB = [0x33, 0x33, 0x33]

const cells = computed<Cell[]>(() => {
  const level = store.editingLevel
  const selected = store.selectedButton
  const bank = store.activeBank
  return screenLayout.value.cells.map(({ id, col, row }) => {
    const resolved = resolveButtonFrom(store.config, deviceStore.profile, level, store.parsed, id, store.currentPage)
    const button = resolved?.button
    const colourCode = effectiveColourCode(button, bank)
    const rgb =
      colourCode && colourCode.length >= 3 ? colourCode : CELL_FALLBACK_RGB
    return {
      id,
      col,
      row,
      label: button?.name?.trim() ?? '',
      colour: rgbCss(colourCode),
      textColour: contrastingTextColour(rgb),
      active: selected != null && selected.id === id,
      inherited: resolved != null && resolved.level !== level,
    }
  })
})
</script>

<template>
  <div class="device-preview" :style="previewStyle">
    <div class="device-screen">
      <div class="device-screen__statusbar">
        <button
          type="button"
          class="device-screen__config-name device-screen__linkbtn"
          @click="goFile"
        >{{ configName }}</button>
        <button
          type="button"
          class="device-screen__mode device-screen__linkbtn"
          :disabled="navTargets.modeIndex == null"
          @click="goMode"
        >{{ context.mode }}</button>
      </div>

      <div class="device-screen__titlebar" :style="titlebarStyle">
        <button
          type="button"
          class="device-screen__bank device-screen__linkbtn"
          :disabled="navTargets.bankIndex == null"
          @click="goBank"
        >{{ context.bank }}</button>
        <button
          type="button"
          class="device-screen__patch device-screen__linkbtn"
          :disabled="navTargets.patchIndex == null"
          @click="goPatch"
        >{{ context.patch }}</button>
      </div>

      <div class="device-screen__navrow">
        <span class="device-screen__channel">Ch. {{ context.channel }}</span>
        <div class="device-screen__navgroup">
          <button type="button" class="device-screen__navbtn" aria-label="Previous">&#9664;</button>
          <span class="device-screen__navlabel">Bank</span>
          <button type="button" class="device-screen__navbtn" aria-label="Next">&#9654;</button>
        </div>
      </div>

      <div
        class="device-screen__grid"
        :style="{ '--screen-columns': screenLayout.cols, '--screen-rows': screenLayout.rows }"
      >
        <button
          v-for="cell in cells"
          :key="cell.id"
          type="button"
          class="device-screen__cell"
          :class="{
            'device-screen__cell--active': cell.active,
            'device-screen__cell--inherited': cell.inherited,
          }"
          :style="{
            '--cell-color': cell.colour,
            '--cell-text-color': cell.textColour,
            gridColumn: `${cell.col} / span 2`,
            gridRow: cell.row,
          }"
          :title="cell.inherited ? 'Inherited — click to override at this level' : undefined"
          @click="store.selectButtonId(cell.id)"
        >
          <span class="device-screen__cell-label">{{ cell.label }}</span>
        </button>
      </div>
    </div>
  </div>
</template>
