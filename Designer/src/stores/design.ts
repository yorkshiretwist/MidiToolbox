// The single source of truth: the design being edited, which step is open, the
// current selection, and the analysis (checks, pin plan) derived from them.

import { computed, ref, watch } from 'vue'
import { defineStore } from 'pinia'
import type {
  Design,
  DesignButton,
  LedPin,
  LedSide,
  NavRole,
  Numbering,
  Orientation,
  Purpose,
} from '../types/device'
import { SCREENS, requireScreen } from '../lib/catalog'
import { emptyPins } from '../lib/pins'
import { analyse } from '../lib/checks'
import { confirmAction } from '../lib/confirm'
import { parseDevice, serialiseDevice } from '../lib/deviceFile'
import {
  downloadDeviceFile,
  isFileSystemAccessSupported,
  openDeviceFile,
  saveDeviceFile,
  saveDeviceFileAs,
  type OpenedFile,
} from '../lib/fileIo'
import {
  GRID_PITCHES,
  MAX_UNIT_SIZE,
  MIN_UNIT_SIZE,
  clamp,
  screenDims,
  snapTo,
  tidy,
} from '../lib/geometry'
import {
  NAV_ROLES,
  applyNumbering,
  buttonByUid,
  firstFreeNode,
  gridCols,
  navRoleOf,
  performanceButtons,
  quickGridPositions,
  spotIsFree,
  suggestScreen,
  type QuickGridOptions,
} from '../lib/layout'
import { buildArtifacts } from '../lib/export/artifacts'
import { buildPreset, PRESETS } from '../lib/presets'
import { fileStem } from '../lib/export/xml'

const STORAGE_KEY = 'midi-toolbox-designer'

export const STEP_COUNT = 6

function loadSaved(): Design | null {
  try {
    const text = localStorage.getItem(STORAGE_KEY)
    if (!text) return null
    return parseDevice(text).design ?? null
  } catch {
    return null
  }
}

export const useDesignStore = defineStore('design', () => {
  const design = ref<Design>(loadSaved() ?? buildPreset(PRESETS[0]!.id))
  const step = ref(1)
  const selectedUid = ref<number | null>(design.value.buttons[0]?.uid ?? null)
  /** While set, the next button clicked on the unit takes this navigation role. */
  const pickingRole = ref<NavRole | null>(null)
  const snap = ref(true)
  const showUntested = ref(true)
  const previewTab = ref<'drawing' | 'schematic' | 'parts' | 'firmware'>('drawing')
  /** The zoomed screen preview is open. */
  const zoomOpen = ref(false)
  const file = ref<OpenedFile | null>(null)
  const baseline = ref(serialiseDevice(design.value))

  const analysis = computed(() => analyse(design.value))
  const screenSpec = computed(() => requireScreen(design.value.screen.catalogId))
  const selected = computed(() => (selectedUid.value === null ? undefined : buttonByUid(design.value, selectedUid.value)))
  const dirty = computed(() => serialiseDevice(design.value) !== baseline.value)
  /** Every generated file; only computed once something reads it (the export step). */
  const artifacts = computed(() => buildArtifacts(design.value, analysis.value))

  watch(
    design,
    () => {
      try {
        localStorage.setItem(STORAGE_KEY, serialiseDevice(design.value))
      } catch {
        /* storage unavailable: the design just isn't remembered */
      }
    },
    { deep: true },
  )

  // ── helpers ────────────────────────────────────────────────────────────
  /** A coordinate kept inside the unit; snapped to the grid unless `exact` or snapping is off. */
  function place(value: number, axis: 'x' | 'y', exact = false): number {
    const max = axis === 'x' ? design.value.unit.width : design.value.unit.height
    const snapped = snap.value && !exact ? snapTo(value, design.value.unit.pitch) : tidy(value)
    return clamp(snapped, 0, max)
  }

  function renumber() {
    applyNumbering(design.value)
  }

  /** An id no button holds yet (matters when numbering is manual). */
  function nextId(): number {
    return design.value.buttons.reduce((max, b) => Math.max(max, b.id), -1) + 1
  }

  function replaceDesign(next: Design, opened: OpenedFile | null = null) {
    design.value = next
    selectedUid.value = next.buttons[0]?.uid ?? null
    pickingRole.value = null
    file.value = opened
    baseline.value = serialiseDevice(next)
  }

  async function confirmDiscard(what: string): Promise<boolean> {
    if (!dirty.value) return true
    return confirmAction(`You have unsaved changes. ${what}`, { confirmLabel: 'Discard changes', cancelLabel: 'Keep editing' })
  }

  function openZoom() {
    zoomOpen.value = true
  }

  function closeZoom() {
    zoomOpen.value = false
  }

  // ── step navigation ────────────────────────────────────────────────────
  function goTo(next: number) {
    step.value = clamp(next, 1, STEP_COUNT)
    pickingRole.value = null
  }

  // ── unit ───────────────────────────────────────────────────────────────
  function setName(name: string) {
    design.value.name = name
  }

  function setPurpose(purpose: Purpose) {
    design.value.purpose = purpose
    // buttons are smaller for finger control, which changes how rows group
    renumber()
  }

  function setUnitSize(width: number, height: number) {
    design.value.unit.width = clamp(tidy(width), MIN_UNIT_SIZE, MAX_UNIT_SIZE)
    design.value.unit.height = clamp(tidy(height), MIN_UNIT_SIZE, MAX_UNIT_SIZE)
  }

  function setColour(colour: number[]) {
    design.value.colour = colour.slice(0, 3)
  }

  function setPitch(pitch: number) {
    if (GRID_PITCHES.includes(pitch)) design.value.unit.pitch = pitch
  }

  // ── buttons ────────────────────────────────────────────────────────────
  function select(uid: number | null) {
    selectedUid.value = uid
  }

  function addButton(x?: number, y?: number): number | null {
    const at = x !== undefined && y !== undefined ? { x: place(x, 'x'), y: place(y, 'y') } : firstFreeNode(design.value, screenSpec.value)
    if (!at) return null
    if (!spotIsFree(design.value, screenSpec.value, at.x, at.y)) return null
    const uid = design.value.nextUid++
    const side: LedSide = 'above'
    design.value.buttons.push({ uid, id: nextId(), x: at.x, y: at.y, led: side })
    renumber()
    selectedUid.value = uid
    return uid
  }

  /** Moves a button; `exact` keeps typed coordinates as given instead of snapping them. */
  function moveButton(uid: number, x: number, y: number, exact = false) {
    const button = buttonByUid(design.value, uid)
    if (!button) return
    button.x = place(x, 'x', exact)
    button.y = place(y, 'y', exact)
    renumber()
  }

  function setLed(uid: number, led: LedSide) {
    const button = buttonByUid(design.value, uid)
    if (button) button.led = led
  }

  /**
   * Any button can be deleted, wherever it is. A navigation button hands its role
   * to the nearest performance button, so there are always three; falling below 3
   * performance buttons is flagged by the checks (and blocks downloads), not here.
   */
  function deleteButton(uid: number) {
    const button = buttonByUid(design.value, uid)
    if (!button) return
    const role = navRoleOf(design.value, uid)
    design.value.buttons = design.value.buttons.filter((b) => b.uid !== uid)
    delete design.value.pins.buttons[uid]
    if (role) {
      const distance = (b: DesignButton) => Math.hypot(b.x - button.x, b.y - button.y)
      const heir = performanceButtons(design.value).sort((a, b) => distance(a) - distance(b))[0]
      if (heir) design.value.nav[role] = heir.uid
    }
    if (selectedUid.value === uid) selectedUid.value = design.value.buttons[0]?.uid ?? null
    renumber()
  }

  function setNumbering(numbering: Numbering) {
    design.value.numbering = numbering
    renumber()
  }

  /** Sets a button's id; another button already holding it swaps ids with it. */
  function setButtonId(uid: number, id: number) {
    if (design.value.numbering !== 'manual') return
    const button = buttonByUid(design.value, uid)
    if (!button || !Number.isInteger(id) || id < 0) return
    const other = design.value.buttons.find((b) => b.id === id && b.uid !== uid)
    if (other) other.id = button.id
    button.id = id
  }

  async function generateGrid(options: QuickGridOptions): Promise<void> {
    const ok = await confirmAction(
      `Replace the ${performanceButtons(design.value).length} performance buttons with a ${options.columns} × ${options.rows} grid? Navigation buttons and the screen stay where they are.`,
      { title: 'Replace performance buttons?', confirmLabel: 'Replace', cancelLabel: 'Cancel' },
    )
    if (!ok) return
    const { positions, unit } = quickGridPositions(design.value, options)
    const nav = new Set(NAV_ROLES.map((role) => design.value.nav[role]))
    design.value.unit = unit
    for (const b of design.value.buttons) if (!nav.has(b.uid)) delete design.value.pins.buttons[b.uid]
    design.value.buttons = design.value.buttons.filter((b) => nav.has(b.uid))
    for (const position of positions) {
      const uid = design.value.nextUid++
      design.value.buttons.push({ uid, id: nextId(), x: position.x, y: position.y, led: 'above' })
    }
    renumber()
    selectedUid.value = design.value.buttons.at(-1)?.uid ?? null
  }

  // ── navigation roles ───────────────────────────────────────────────────
  /** Gives `role` to `uid`; if that button already plays another role the two swap. */
  function setNav(role: NavRole, uid: number) {
    const nav = design.value.nav
    const current = nav[role]
    if (current === uid) return
    const other = NAV_ROLES.find((r) => r !== role && nav[r] === uid)
    if (other) nav[other] = current
    nav[role] = uid
  }

  function startPicking(role: NavRole) {
    pickingRole.value = pickingRole.value === role ? null : role
  }

  // ── screen ─────────────────────────────────────────────────────────────
  function setScreen(catalogId: string) {
    design.value.screen.catalogId = catalogId
    clampScreenIntoUnit()
  }

  function setOrientation(orientation: Orientation) {
    design.value.screen.orientation = orientation
    clampScreenIntoUnit()
  }

  function moveScreen(x: number, y: number, exact = false) {
    design.value.screen.x = place(x, 'x', exact)
    design.value.screen.y = place(y, 'y', exact)
  }

  /** Keeps the screen's top-left in the unit; an over-size screen is left for the checks to flag. */
  function clampScreenIntoUnit() {
    const { module } = screenDims(screenSpec.value, design.value.screen.orientation)
    const { width, height } = design.value.unit
    design.value.screen.x = clamp(design.value.screen.x, 0, Math.max(0, width - module[0]))
    design.value.screen.y = clamp(design.value.screen.y, 0, Math.max(0, height - module[1]))
  }

  function suggest() {
    const grid = analysis.value.grid
    setScreen(suggestScreen(SCREENS, design.value.screen.orientation, gridCols(grid), grid.rows).id)
  }

  // ── pins ───────────────────────────────────────────────────────────────
  // Passing null puts a pin back to its default.
  function setButtonPin(uid: number, pin: number | null) {
    if (pin === null) delete design.value.pins.buttons[uid]
    else design.value.pins.buttons[uid] = pin
  }

  function setLedPin(key: LedPin, pin: number | null) {
    if (pin === null) delete design.value.pins.led[key]
    else design.value.pins.led[key] = pin
  }

  function setScreenPin(role: string, pin: number | null) {
    if (pin === null) delete design.value.pins.screen[role]
    else design.value.pins.screen[role] = pin
  }

  /** Puts one group of pins, or all of them, back to their defaults. */
  function resetPins(group: 'buttons' | 'led' | 'screen' | 'all') {
    if (group === 'all') design.value.pins = emptyPins()
    else if (group === 'buttons') design.value.pins.buttons = {}
    else if (group === 'led') design.value.pins.led = {}
    else design.value.pins.screen = {}
  }

  // ── files and presets ──────────────────────────────────────────────────
  /** Replaces the design with a preset without asking (used on start-up and by loadPreset). */
  function startFrom(id: string) {
    replaceDesign(buildPreset(id))
    step.value = 1
  }

  async function loadPreset(id: string) {
    if (!(await confirmDiscard('Start from this example anyway?'))) return
    startFrom(id)
  }

  async function openFile() {
    if (!(await confirmDiscard('Open another file anyway?'))) return
    let opened: OpenedFile | null
    try {
      opened = await openDeviceFile()
    } catch {
      return // picker cancelled
    }
    if (!opened) return
    const result = parseDevice(opened.text)
    if (!result.design) {
      await confirmAction(`${opened.name} is not a usable device profile:\n${result.errors.join('\n')}`, {
        title: 'Could not open file',
        confirmLabel: 'OK',
        cancelLabel: 'Close',
      })
      return
    }
    replaceDesign(result.design, opened)
    step.value = 1
  }

  async function saveFile() {
    const text = serialiseDevice(design.value)
    const name = `${fileStem(design.value.name)}-design.json`
    try {
      if (file.value && isFileSystemAccessSupported()) await saveDeviceFile(file.value, text)
      else if (isFileSystemAccessSupported()) file.value = await saveDeviceFileAs(name, text)
      else downloadDeviceFile(name, text)
    } catch {
      return // save dialog cancelled
    }
    baseline.value = text
  }

  async function revert() {
    if (!dirty.value) return
    if (!(await confirmAction('Throw away your changes and go back to the last saved version?', { confirmLabel: 'Revert', cancelLabel: 'Keep editing' }))) return
    const result = parseDevice(baseline.value)
    if (result.design) replaceDesign(result.design, file.value)
  }

  return {
    design,
    step,
    selectedUid,
    selected,
    pickingRole,
    snap,
    showUntested,
    previewTab,
    zoomOpen,
    file,
    analysis,
    artifacts,
    screenSpec,
    dirty,
    goTo,
    openZoom,
    closeZoom,
    setName,
    setPurpose,
    setUnitSize,
    setColour,
    setPitch,
    select,
    addButton,
    moveButton,
    setLed,
    deleteButton,
    setNumbering,
    setButtonId,
    generateGrid,
    setButtonPin,
    setLedPin,
    setScreenPin,
    resetPins,
    setNav,
    startPicking,
    setScreen,
    setOrientation,
    moveScreen,
    suggest,
    startFrom,
    loadPreset,
    openFile,
    saveFile,
    revert,
  }
})
