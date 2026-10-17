<script setup lang="ts">
// The device's screen in Play mode: mirrors what the hardware would show, from the pending navigation cursor.
import { computed } from 'vue'
import { useConfigStore } from '../stores/config'
import { usePlayStore } from '../stores/play'
import { useDeviceStore } from '../stores/device'
import { effectiveColourCode, rgbCss } from '../lib/buttons'
import { deviceGeometry, deviceScreenLayout } from '../lib/deviceLayout'
import { contrastingTextColour } from '../lib/colour'

const configStore = useConfigStore()
const play = usePlayStore()
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

const configName = computed(() => (configStore.fileName ?? 'untitled').replace(/\.json$/i, ''))

// Always read from the pending cursor — when nothing is pending it equals the
// live selection, so this is also the correct readout while clean. See
// navigationPreview{Mode,Bank,Patch}Name() in MIDI-Toolbox/Navigation.h.
const previewMode = computed(() => play.config.modes[play.pendingModeIndex])
const previewBank = computed(() => previewMode.value?.banks[play.pendingBankIndex])
const previewPatch = computed(() => previewBank.value?.patches[play.pendingPatchIndex])

const patchLabel = computed(() => {
  const name = previewPatch.value?.name ?? '-'
  if (play.patchDirty) return name
  const pages = play.pageCount
  return pages > 1 ? `${name} / ${play.page}` : name
})

// ---- Live layout (Bank "screenLayout": "Live") — mirrors Screen.h's Live layout ----

const live = computed(() => play.liveLayout)

// The three font tiers in screen pixels (SCREEN_FONT_BIG / MID / SMALL in Screen.h).
const FONT_BIG = 48
const FONT_MID = 36
const FONT_SMALL = 24
// Rough average glyph width as a fraction of font size — the browser can't
// measure like LVGL, so this only approximates which tier the hardware picks.
const GLYPH_WIDTH_EM = 0.58
const LIVE_BORDER = 3
const LIVE_PAGE_W = 96

/** The largest of `tiers` (px) at which `text` is estimated to fit on one line in `maxWidth` px, else the smallest. */
function fitFontSize(text: string, maxWidth: number, tiers: number[]): number {
  for (const size of tiers) {
    if (text.length * size * GLYPH_WIDTH_EM <= maxWidth) return size
  }
  return tiers[tiers.length - 1]
}

const screenWidth = computed(() => geometry.value.screen.lx)
const showPage = computed(() => play.previewPageCount > 1)
const patchBoxWidth = computed(() => (showPage.value ? screenWidth.value - LIVE_PAGE_W - 4 : screenWidth.value))
const bankText = computed(() => previewBank.value?.name ?? '-')
const patchText = computed(() => previewPatch.value?.name ?? '-')
// A pending different patch always opens on page 1.
const pageText = computed(() => `${play.patchDirty ? 1 : play.page}/${play.previewPageCount}`)

const bankFont = computed(() => fitFontSize(bankText.value, screenWidth.value - 8 - LIVE_BORDER * 2, [FONT_MID, FONT_SMALL]))
const patchFont = computed(() =>
  fitFontSize(patchText.value, patchBoxWidth.value - 8 - LIVE_BORDER * 2, [FONT_BIG, FONT_MID, FONT_SMALL]),
)
const pageFont = computed(() =>
  fitFontSize(pageText.value, LIVE_PAGE_W - 8 - LIVE_BORDER * 2, [FONT_BIG, FONT_MID, FONT_SMALL]),
)

// The patch/page boxes are the bank colour at 65% brightness (SCREEN_LIVE_PATCH_DIM).
const patchBoxStyle = computed(() => {
  const base = play.bank?.subheaderColourCode
  const rgb = (base && base.length >= 3 ? base : [44, 44, 44]).slice(0, 3).map((c) => Math.round(c * 0.65))
  return { '--box-bg': rgbCss(rgb), '--box-text': contrastingTextColour(rgb) }
})

// The box the step arrows currently steer gets a white border.
const steersBank = computed(() => play.selectedParameter === 'Bank')
const steersPatch = computed(() => play.selectedParameter === 'Patch')
const steersPage = computed(() => play.selectedParameter === 'Page')

// ---- button-name overlay (either layout) ----

const toastStyle = computed(() => {
  const rgb = play.toastColour && play.toastColour.length >= 3 ? play.toastColour : [44, 44, 44]
  const size = fitFontSize(play.toastText, screenWidth.value - 56, [FONT_BIG, FONT_MID, FONT_SMALL])
  return {
    '--toast-bg': rgbCss(rgb),
    '--toast-text': contrastingTextColour(rgb),
    fontSize: `calc(var(--u) * ${size})`,
  }
})
// The flashing "Set?" preview always takes priority over the overlay.
const toastShown = computed(() => play.toastVisible && !play.dirty)

const SUBHEADER_FALLBACK_RGB = [44, 44, 44]
const titlebarStyle = computed(() => {
  const rgb = play.bank?.subheaderColourCode
  const resolved = rgb && rgb.length >= 3 ? rgb : SUBHEADER_FALLBACK_RGB
  return {
    '--titlebar-bg': rgbCss(rgb, rgbCss(SUBHEADER_FALLBACK_RGB)),
    '--titlebar-text': contrastingTextColour(resolved),
  }
})

interface Cell {
  id: number
  col: number
  row: number
  label: string
  colour: string
  textColour: string
  on: boolean
  bound: boolean
}

const CELL_FALLBACK_RGB = [0x33, 0x33, 0x33]

const cells = computed<Cell[]>(() =>
  screenLayout.value.cells.map(({ id, col, row }) => {
    const resolved = play.resolveAt(id)
    const button = resolved?.button
    const colourCode = effectiveColourCode(button, play.bank)
    const rgb = colourCode && colourCode.length >= 3 ? colourCode : CELL_FALLBACK_RGB
    return {
      id,
      col,
      row,
      label: button?.name?.trim() ?? '',
      colour: rgbCss(colourCode),
      textColour: contrastingTextColour(rgb),
      on: play.isOn(id),
      bound: button != null,
    }
  }),
)

function press(id: number) {
  play.pressPhysical(id)
}
function release(id: number) {
  play.releasePhysical(id)
}

// The on-screen middle/left/right parameter controls are a deliberately
// separate path from the physical footswitches 10-12: they always run the
// built-in advance/activate/step role directly, regardless of whether a
// config has overridden the matching physical id, so navigation is never
// lost. See docs/navigation.md and the
// nav-footswitches-built-in-roles project convention.
function onMiddleTap() {
  if (play.dirty) play.activateSelection()
  else play.cycleCurrentParameterSelection('Up')
}
function onStepDown() {
  play.navStepCurrentParameterValueResolved('Down')
}
function onStepUp() {
  play.navStepCurrentParameterValueResolved('Up')
}
</script>

<template>
  <div class="device-preview" :style="previewStyle">
    <div class="device-screen" :class="{ 'device-screen--live': live }">
      <div class="device-screen__statusbar">
        <span>{{ configName }}</span>
        <span :class="{ 'play-screen__flash': play.modeDirty }">{{ previewMode?.name ?? '-' }}</span>
      </div>

      <div v-if="!live" class="device-screen__titlebar" :style="titlebarStyle">
        <span :class="{ 'play-screen__flash': play.bankDirty }">{{ previewBank?.name ?? '-' }}</span>
        <span :class="{ 'play-screen__flash': play.patchDirty }">{{ patchLabel }}</span>
      </div>

      <div v-else class="live-body">
        <div
          class="live-box live-box--bank"
          :class="{ 'live-box--steered': steersBank }"
          :style="titlebarStyle"
        >
          <span class="live-box__caption">BANK</span>
          <span
            class="live-box__value"
            :class="{ 'play-screen__flash': play.bankDirty }"
            :style="{ fontSize: `calc(var(--u) * ${bankFont})` }"
          >{{ bankText }}</span>
        </div>
        <div class="live-row">
          <div
            class="live-box live-box--patch"
            :class="{ 'live-box--steered': steersPatch }"
            :style="patchBoxStyle"
          >
            <span class="live-box__caption">PATCH</span>
            <span
              class="live-box__value"
              :class="{ 'play-screen__flash': play.patchDirty }"
              :style="{ fontSize: `calc(var(--u) * ${patchFont})` }"
            >{{ patchText }}</span>
          </div>
          <div
            v-if="showPage"
            class="live-box live-box--page"
            :class="{ 'live-box--steered': steersPage }"
            :style="patchBoxStyle"
          >
            <span class="live-box__caption">PAGE</span>
            <span
              class="live-box__value"
              :class="{ 'play-screen__flash': play.patchDirty }"
              :style="{ fontSize: `calc(var(--u) * ${pageFont})` }"
            >{{ pageText }}</span>
          </div>
        </div>
      </div>

      <div class="device-screen__navrow">
        <span class="device-screen__channel" :class="{ 'play-screen__flash': play.channelDirty }">
          Ch. {{ play.pendingMidiChannel }}
        </span>
        <div class="device-screen__navgroup">
          <button
            type="button"
            class="device-screen__navbtn"
            aria-label="Previous"
            :disabled="play.parameterValueCount <= 1"
            @click="onStepDown"
          >&#9664;</button>
          <button
            type="button"
            class="device-screen__navlabel device-screen__linkbtn"
            @click="onMiddleTap"
          >{{ play.button10Label }}</button>
          <button
            type="button"
            class="device-screen__navbtn"
            aria-label="Next"
            :disabled="play.parameterValueCount <= 1"
            @click="onStepUp"
          >&#9654;</button>
        </div>
      </div>

      <div
        v-if="!live"
        class="device-screen__grid"
        :style="{ '--screen-columns': screenLayout.cols, '--screen-rows': screenLayout.rows }"
      >
        <button
          v-for="cell in cells"
          :key="cell.id"
          type="button"
          class="device-screen__cell"
          :class="{ 'device-screen__cell--active': cell.on }"
          :style="{
            '--cell-color': cell.colour,
            '--cell-text-color': cell.textColour,
            gridColumn: `${cell.col} / span 2`,
            gridRow: cell.row,
          }"
          :disabled="!cell.bound"
          @mousedown="press(cell.id)"
          @mouseup="release(cell.id)"
          @mouseleave="release(cell.id)"
        >
          <span class="device-screen__cell-label">{{ cell.label }}</span>
        </button>
      </div>

      <div v-if="toastShown" class="device-screen__toast" :style="toastStyle" role="status">
        {{ play.toastText }}
      </div>
    </div>
  </div>
</template>

<style scoped>
.play-screen__flash {
  animation: play-screen-flash 1s steps(1, jump-none) infinite;
}

@keyframes play-screen-flash {
  0%,
  79% {
    opacity: 1;
  }
  80%,
  100% {
    opacity: 0.3;
  }
}
</style>
