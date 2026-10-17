// Live-performance simulator for Play mode: ports the pending-cursor
// navigation model in MIDI-Toolbox/Navigation.h (see
// docs/navigation.md) so pressing an on-screen footswitch
// behaves like the physical unit — held notes/CCs only release at the exact
// moment a hierarchy change is activated, never while just cycling through
// candidates. Reads the same DeviceConfig tree the editor is editing (via
// useConfigStore) but keeps its own live/pending selection entirely separate
// from the editor's own File > Mode > Bank > Patch > Button drill-down, which
// has no hardware equivalent.
//
// Config-file switching (SetConfig/CycleConfig) has no meaning here — the
// editor only ever has one config loaded — so those two action types, and the
// 'Config' navigation parameter, are no-ops in play mode.

import { defineStore } from 'pinia'
import { useConfigStore } from './config'
import { useDeviceStore } from './device'
import {
  buttonOnPage,
  effectiveColourCode,
  buttonsContainerAt,
  resolveButtonFrom,
  type ResolvedButton,
  type ResolvedButtonLevel,
} from '../lib/buttons'
import { DEFAULT_VELOCITY } from '../lib/actionDefaults'
import { flattenActions } from '../lib/presets'
import { noteNameToMidi } from '../lib/notes'
import { sendControlChange, sendNoteOff, sendNoteOn, sendProgramChange } from '../lib/midi'
import type {
  Action,
  Bank,
  Button,
  CycleDirection,
  DeviceConfig,
  Mode,
  NavigationParameter,
  Patch,
  Rgb,
} from '../types/config'
import type { DeviceFile } from '../types/device'

const DEFAULT_PARAMETERS: NavigationParameter[] = ['Patch', 'Bank', 'Mode', 'MidiChannel', 'Page']

/** Notes stagger this many ms apart under SpreadUp/Down/Random (see NoteAction::OnPress in MIDI-Toolbox/NoteAction.h). */
const NOTE_SPREAD_DELAY_MS = 30

function navWrap(value: number, count: number): number {
  return ((value % count) + count) % count
}

function clampIndex(value: number, count: number): number {
  if (count <= 0) return 0
  if (value < 0) return 0
  if (value >= count) return count - 1
  return value
}

/** Mirrors ModeBank::resolveDefaultPatchIndex() in MIDI-Toolbox/ModeBank.h. */
function resolveDefaultPatchIndex(bank: Bank): number {
  const count = bank.patches.length
  if (count === 0) return -1
  return clampIndex(bank.defaultPatchIndex ?? 0, count)
}

/** Mirrors patchPageCount() in MIDI-Toolbox/Runtime.h. */
function patchPageCount(patch: Patch | undefined): number {
  if (!patch) return 1
  return patch.buttons.reduce((max, b) => Math.max(max, b.page ?? 1), 1)
}

/** Mirrors getCurrentParameters() in MIDI-Toolbox/Runtime.h, minus the Config parameter. */
function getParameters(
  patch: Patch | undefined,
  bank: Bank | undefined,
  mode: Mode | undefined,
  config: DeviceConfig,
): NavigationParameter[] {
  if (patch?.parameters?.length) return patch.parameters
  if (bank?.parameters?.length) return bank.parameters
  if (mode?.parameters?.length) return mode.parameters
  if (config.parameters?.length) return config.parameters
  return DEFAULT_PARAMETERS
}

/** Mirrors buttonSendsMidi() in MIDI-Toolbox/Navigation.h — any PlayNotes / ProgramChange / ControlChange action. */
function buttonSendsMidi(button: Button, config: DeviceConfig): boolean {
  return flattenActions(button.actions, config).some(
    (a) => a.type === 'PlayNotes' || a.type === 'ProgramChange' || a.type === 'ControlChange',
  )
}

/** How long the button-name overlay stays up at minimum (TOAST_MIN_MS in MIDI-Toolbox/Navigation.h). */
export const TOAST_MIN_MS = 500

/**
 * How many selection passes (applySelection / applyDefaultButtonStatuses) are running. Mirrors
 * selectionPassDepth in MIDI-Toolbox/Runtime.h: a Set patch for the patch that is already live
 * is a "reset" when pressed, but does nothing when fired from inside a pass (a default-on
 * button that selects its own patch would otherwise re-seed itself forever).
 */
let selectionPassDepth = 0

/** Pending minimum-display timer for the overlay; module-level because a timer handle isn't state. */
let toastTimer: ReturnType<typeof setTimeout> | undefined

/** Mirrors buttonIsPurePerformance() in MIDI-Toolbox/Runtime.h. */
function buttonIsPurePerformance(button: Button, config: DeviceConfig): boolean {
  const actions = flattenActions(button.actions, config)
  if (actions.length === 0) return false
  return actions.every(
    (a) => a.type === 'PlayNotes' || a.type === 'ProgramChange' || a.type === 'ControlChange',
  )
}

function findInLevelContainer(
  container: Button[] | undefined,
  level: ResolvedButtonLevel,
  id: number,
  page: number,
): Button | undefined {
  if (!container) return undefined
  if (level === 'patch') return container.find((b) => b.id === id && buttonOnPage(b, page))
  return container.find((b) => b.id === id)
}

function shuffle<T>(items: T[]): T[] {
  const result = [...items]
  for (let i = result.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1))
    ;[result[i], result[j]] = [result[j], result[i]]
  }
  return result
}

interface HeldNote {
  midi: number
  channel: number
}

interface State {
  active: boolean
  modeIndex: number
  bankIndex: number
  patchIndex: number
  midiChannel: number
  page: number
  pendingModeIndex: number
  pendingBankIndex: number
  pendingPatchIndex: number
  pendingMidiChannel: number
  parameterIndex: number
  buttonBeingPressed: number | null
  /** Runtime on/off state per Button object — never written onto the config tree itself. */
  buttonOn: Map<Button, boolean>
  /** Notes currently sounding per Button, so OnRelease sends the exact note/channel pairs OnPress sent. */
  heldNotes: Map<Button, HeldNote[]>
  /** The button-name overlay: shown after a button that sends MIDI is pressed. See showToast(). */
  toastVisible: boolean
  toastText: string
  toastColour: Rgb | undefined
  /** The Momentary button holding the overlay up, or null. */
  toastHoldId: number | null
  /** True once the minimum display time has passed. */
  toastMinElapsed: boolean
}

export const usePlayStore = defineStore('play', {
  state: (): State => ({
    active: false,
    modeIndex: 0,
    bankIndex: -1,
    patchIndex: -1,
    midiChannel: 1,
    page: 1,
    pendingModeIndex: 0,
    pendingBankIndex: -1,
    pendingPatchIndex: -1,
    pendingMidiChannel: 1,
    parameterIndex: 0,
    buttonBeingPressed: null,
    buttonOn: new Map(),
    heldNotes: new Map(),
    toastVisible: false,
    toastText: '',
    toastColour: undefined,
    toastHoldId: null,
    toastMinElapsed: true,
  }),

  getters: {
    config(): DeviceConfig {
      return useConfigStore().config
    },
    device(): DeviceFile {
      return useDeviceStore().profile
    },
    mode(): Mode | undefined {
      return this.config.modes[this.modeIndex]
    },
    bank(): Bank | undefined {
      return this.bankIndex >= 0 ? this.mode?.banks[this.bankIndex] : undefined
    },
    patch(): Patch | undefined {
      return this.patchIndex >= 0 ? this.bank?.patches[this.patchIndex] : undefined
    },
    /** The (modeIndex, bankIndex, patchIndex) shape resolveButtonFrom() expects — -1 reads as "not selected". */
    liveAt(): { modeIndex: number; bankIndex: number | undefined; patchIndex: number | undefined } {
      return {
        modeIndex: this.modeIndex,
        bankIndex: this.bankIndex >= 0 ? this.bankIndex : undefined,
        patchIndex: this.patchIndex >= 0 ? this.patchIndex : undefined,
      }
    },
    pageCount(): number {
      return patchPageCount(this.patch)
    },
    /** Page count of the patch the screen is previewing (the pending candidate) — the Live layout shows a Page box only when it's above 1. */
    previewPageCount(): number {
      return patchPageCount(this.config.modes[this.pendingModeIndex]?.banks[this.pendingBankIndex]?.patches[this.pendingPatchIndex])
    },
    /** True when the live bank asks for the large-type Live screen layout. */
    liveLayout(): boolean {
      return this.bank?.screenLayout === 'Live'
    },
    parameters(): NavigationParameter[] {
      return getParameters(this.patch, this.bank, this.mode, this.config)
    },
    selectedParameter(): NavigationParameter | undefined {
      const params = this.parameters
      if (params.length === 0) return undefined
      const index = this.parameterIndex >= 0 && this.parameterIndex < params.length ? this.parameterIndex : 0
      return params[index]
    },
    modeDirty(): boolean {
      return this.pendingModeIndex !== this.modeIndex
    },
    bankDirty(): boolean {
      return this.modeDirty || this.pendingBankIndex !== this.bankIndex
    },
    patchDirty(): boolean {
      return this.bankDirty || this.pendingPatchIndex !== this.patchIndex
    },
    channelDirty(): boolean {
      return this.pendingMidiChannel !== this.midiChannel
    },
    /** True iff the pending cursor differs from the live selection anywhere. */
    dirty(): boolean {
      return this.patchDirty || this.channelDirty
    },
    /** How many distinct values the selected parameter can step through — the on-screen arrows grey out at ≤ 1. */
    parameterValueCount(): number {
      switch (this.selectedParameter) {
        case 'Patch':
          return this.bank?.patches.length ?? 0
        case 'Bank':
          return this.mode?.banks.length ?? 0
        case 'Mode':
          return this.config.modes.length
        case 'MidiChannel':
          return 16
        case 'Page':
          return this.pageCount
        default:
          return 0
      }
    },
    /** The middle footswitch's label: "Set?" while dirty, else the selected parameter's name. */
    button10Label(): string {
      if (this.config.modes.length === 0) return '-'
      if (this.dirty) return 'Set?'
      return this.selectedParameter ?? '-'
    },
  },

  actions: {
    /** Resolves the effective Button for physical `id` against the live selection, Patch > Bank > Mode > File > built-in. */
    resolveAt(id: number, page?: number): ResolvedButton | undefined {
      return resolveButtonFrom(this.config, this.device, 'patch', this.liveAt, id, page ?? this.page)
    },

    isOn(id: number): boolean {
      const resolved = this.resolveAt(id)
      return resolved ? this.buttonOn.get(resolved.button) === true : false
    },

    /** Enter play mode: seeds the live selection at Mode 0's first Bank/default Patch and fires default-on buttons — mirrors firmware boot (resetRuntimeSelection() in ConfigStore.h). */
    enterPlayMode() {
      this.active = true
      this.hideToast()
      this.buttonOn = new Map()
      this.heldNotes = new Map()
      this.modeIndex = 0
      const mode = this.config.modes[0]
      if (mode && mode.banks.length > 0) {
        this.bankIndex = 0
        this.patchIndex = resolveDefaultPatchIndex(mode.banks[0])
      } else {
        this.bankIndex = -1
        this.patchIndex = -1
      }
      this.midiChannel = this.config.midiChannel ?? 1
      this.page = 1
      this.parameterIndex = 0
      this.autoSelectPageParameterIfMultiPage()
      this.applyDefaultButtonStatuses(false)
      this.resetPendingToLive()
    },

    /** Leave play mode: releases every held note/CC so nothing is left sounding. */
    exitPlayMode() {
      const patch = this.patch
      if (patch) this.turnOffAllButtonsInPatch(patch)
      this.turnOffAllActiveButtons()
      this.active = false
      this.hideToast()
      this.buttonOn = new Map()
      this.heldNotes = new Map()
    },

    resetPendingToLive() {
      this.pendingModeIndex = this.modeIndex
      this.pendingBankIndex = this.bankIndex
      this.pendingPatchIndex = this.patchIndex
      this.pendingMidiChannel = this.midiChannel
    },

    // ---- pending-cursor movement (NAVIGATE verbs) --------------------------

    navResetBankPatchForPendingMode() {
      if (this.pendingModeIndex === this.modeIndex) {
        this.pendingBankIndex = this.bankIndex
        this.pendingPatchIndex = this.patchIndex
        return
      }
      const mode = this.config.modes[this.pendingModeIndex]
      this.pendingBankIndex = mode && mode.banks.length > 0 ? 0 : -1
      this.pendingPatchIndex =
        mode && this.pendingBankIndex >= 0 ? resolveDefaultPatchIndex(mode.banks[0]) : -1
    },

    navSeedPendingPatchForBank() {
      if (this.pendingModeIndex === this.modeIndex && this.pendingBankIndex === this.bankIndex) {
        this.pendingPatchIndex = this.patchIndex
        return
      }
      const mode = this.config.modes[this.pendingModeIndex]
      const hasBank = mode != null && this.pendingBankIndex >= 0 && this.pendingBankIndex < mode.banks.length
      this.pendingPatchIndex = hasBank ? resolveDefaultPatchIndex(mode!.banks[this.pendingBankIndex]) : -1
    },

    pendingCycleMode(direction: CycleDirection) {
      const count = this.config.modes.length
      if (count === 0) return
      this.pendingModeIndex = navWrap(this.pendingModeIndex + (direction === 'Up' ? 1 : -1), count)
      this.navResetBankPatchForPendingMode()
    },

    pendingCycleBank(direction: CycleDirection) {
      const mode = this.config.modes[this.pendingModeIndex]
      const count = mode?.banks.length ?? 0
      if (count === 0) return
      this.pendingBankIndex = navWrap(this.pendingBankIndex + (direction === 'Up' ? 1 : -1), count)
      this.navSeedPendingPatchForBank()
    },

    pendingCyclePatch(direction: CycleDirection) {
      const bank = this.config.modes[this.pendingModeIndex]?.banks[this.pendingBankIndex]
      const count = bank?.patches.length ?? 0
      if (count === 0) return
      this.pendingPatchIndex = navWrap(this.pendingPatchIndex + (direction === 'Up' ? 1 : -1), count)
    },

    pendingCycleMidiChannel(direction: CycleDirection) {
      let channel = this.pendingMidiChannel + (direction === 'Up' ? 1 : -1)
      if (channel > 16) channel = 1
      if (channel < 1) channel = 16
      this.pendingMidiChannel = channel
    },

    pendingCycleCurrentParameterValue(direction: CycleDirection) {
      switch (this.selectedParameter) {
        case 'Patch':
          this.pendingCyclePatch(direction)
          break
        case 'Bank':
          this.pendingCycleBank(direction)
          break
        case 'Mode':
          this.pendingCycleMode(direction)
          break
        case 'MidiChannel':
          this.pendingCycleMidiChannel(direction)
          break
        // Config has no meaning with a single loaded config; Page is always
        // immediate and never reaches here (see navStepCurrentParameterValueResolved).
      }
    },

    // ---- page navigation — always immediate (docs/navigation.md) ----

    applyPageStep(direction: CycleDirection) {
      const count = this.pageCount
      if (count <= 1) return
      this.page = navWrap(this.page - 1 + (direction === 'Up' ? 1 : -1), count) + 1
    },

    navSetPage(page: number) {
      const count = this.pageCount
      if (page < 1 || page > count) return
      this.page = page
    },

    navStepCurrentParameterValueResolved(direction: CycleDirection) {
      if (this.selectedParameter === 'Page') {
        this.applyPageStep(direction)
        return
      }
      this.pendingCycleCurrentParameterValue(direction)
    },

    // ---- activation — the one place the live selection changes ------------

    /** Mirrors applySelection() in MIDI-Toolbox/Navigation.h. */
    applySelection(modeIndex: number, bankIndex: number, patchIndex: number, midiChannel: number) {
      selectionPassDepth++
      try {
        this.runApplySelection(modeIndex, bankIndex, patchIndex, midiChannel)
      } finally {
        selectionPassDepth--
      }
    },

    runApplySelection(modeIndex: number, bankIndex: number, patchIndex: number, midiChannel: number) {
      const outgoingPatch = this.patch
      const outgoingBank = this.bank
      const sameBankPatchChange =
        modeIndex === this.modeIndex && bankIndex === this.bankIndex && patchIndex !== this.patchIndex

      let maintainedButtonIds: number[] = []
      if (sameBankPatchChange && outgoingPatch && outgoingBank?.maintainActiveButtonOnPatchChange) {
        maintainedButtonIds = outgoingPatch.buttons
          .filter((b) => this.buttonOn.get(b))
          .map((b) => b.id)
      }

      // 1. stop the outgoing selection's held notes/CCs — the only moment held sound stops.
      if (outgoingPatch) this.turnOffAllButtonsInPatch(outgoingPatch)
      this.turnOffAllActiveButtons()

      if (this.config.modes.length === 0) {
        this.resetPendingToLive()
        return
      }

      // 2. point the live selection at the target, clamped to the tree.
      this.modeIndex = clampIndex(modeIndex, this.config.modes.length)
      const mode = this.config.modes[this.modeIndex]
      if (mode.banks.length > 0) {
        this.bankIndex = clampIndex(bankIndex, mode.banks.length)
        const bank = mode.banks[this.bankIndex]
        this.patchIndex = bank.patches.length > 0 ? clampIndex(patchIndex, bank.patches.length) : -1
      } else {
        this.bankIndex = -1
        this.patchIndex = -1
      }
      if (midiChannel >= 1 && midiChannel <= 16) this.midiChannel = midiChannel
      this.page = 1
      this.autoSelectPageParameterIfMultiPage()

      // 3. fire the new selection's default-on buttons.
      this.applyDefaultButtonStatuses(sameBankPatchChange)

      // 3b. re-trigger any buttons carried over by maintainActiveButtonOnPatchChange.
      for (const id of maintainedButtonIds) {
        const resolved = this.resolveAt(id)
        if (resolved && !this.buttonOn.get(resolved.button)) this.turnButtonOn(resolved.button)
      }

      // 4. pending == live again.
      this.resetPendingToLive()
    },

    activateSelection() {
      if (!this.dirty) return
      // A channel-only change strands nothing, so it applies directly — no teardown.
      if (this.channelDirty && !this.patchDirty) {
        this.midiChannel = this.pendingMidiChannel
        this.resetPendingToLive()
        return
      }
      this.applySelection(this.pendingModeIndex, this.pendingBankIndex, this.pendingPatchIndex, this.pendingMidiChannel)
    },

    // ---- direct Set* verbs — apply immediately, full teardown -------------

    /**
     * Mirrors navSetPatch() in MIDI-Toolbox/Navigation.h. With only `index` the patch is in the
     * current bank; `bankIndex` picks another bank of the current mode, and with `modeIndex` too,
     * a bank of another mode. A bare `modeIndex` is ignored.
     */
    navSetPatch(index: number, bankIndex?: number, modeIndex?: number) {
      const hasBank = bankIndex !== undefined
      const targetMode = hasBank && modeIndex !== undefined ? modeIndex : this.modeIndex
      const targetBank = hasBank ? bankIndex : this.bankIndex
      const bank = this.config.modes[targetMode]?.banks[targetBank]
      if (!bank || index < 0 || index >= bank.patches.length) return
      // The patch we are already on is a RESET when pressed (the same teardown and re-seed as a
      // change: held notes released, defaults re-applied, page 1, pending discarded).
      if (this.landsOnLiveSelection(targetMode, targetBank, index)) return
      this.applySelection(targetMode, targetBank, index, this.midiChannel)
    },

    /**
     * Mirrors navSetLandsOnLiveSelection(): true when a Set* action should do nothing because it
     * lands exactly on the live selection AND is running inside a selection pass (a default-on
     * button selecting what is being seeded would otherwise re-seed itself forever). Pressed by a
     * player, landing on the live selection is a deliberate reset and goes ahead.
     */
    landsOnLiveSelection(mode: number, bank: number, patch: number): boolean {
      return selectionPassDepth > 0 && mode === this.modeIndex && bank === this.bankIndex && patch === this.patchIndex
    },

    /**
     * Mirrors navSetBank(): `modeIndex` picks a bank of another mode; it lands on that bank's
     * default patch — so choosing the bank you are in goes "home" to that patch, or resets it.
     */
    navSetBank(index: number, modeIndex?: number) {
      const targetMode = modeIndex ?? this.modeIndex
      const mode = this.config.modes[targetMode]
      if (!mode || index < 0 || index >= mode.banks.length) return
      const landingPatch = resolveDefaultPatchIndex(mode.banks[index])
      if (this.landsOnLiveSelection(targetMode, index, landingPatch)) return
      this.applySelection(targetMode, index, landingPatch, this.midiChannel)
    },

    /** Mirrors navSetMode(): lands on the mode's first bank and its default patch (back there, or a reset, if already in the mode). */
    navSetMode(index: number) {
      if (index < 0 || index >= this.config.modes.length) return
      const mode = this.config.modes[index]
      // a mode with no banks has no landing bank or patch (-1, -1), which is also where applySelection() leaves it
      const landingBank = mode.banks.length > 0 ? 0 : -1
      const landingPatch = mode.banks.length > 0 ? resolveDefaultPatchIndex(mode.banks[0]) : -1
      if (this.landsOnLiveSelection(index, landingBank, landingPatch)) return
      this.applySelection(index, Math.max(landingBank, 0), Math.max(landingPatch, 0), this.midiChannel)
    },

    /** Mirrors applyCurrentParameterValue() in MIDI-Toolbox/Navigation.h — sets the selected parameter's value immediately, by name (Patch/Bank/Mode) or number (MidiChannel/Page). */
    applyCurrentParameterValue(value: string) {
      switch (this.selectedParameter) {
        case 'Patch': {
          const index = this.bank?.patches.findIndex((p) => p.name === value) ?? -1
          if (index !== -1) this.applySelection(this.modeIndex, this.bankIndex, index, this.midiChannel)
          break
        }
        case 'Bank': {
          const mode = this.mode
          const index = mode?.banks.findIndex((b) => b.name === value) ?? -1
          if (mode && index !== -1) {
            this.applySelection(this.modeIndex, index, resolveDefaultPatchIndex(mode.banks[index]), this.midiChannel)
          }
          break
        }
        case 'Mode': {
          const index = this.config.modes.findIndex((m) => m.name === value)
          if (index !== -1) {
            const mode = this.config.modes[index]
            const patchIndex = mode.banks.length > 0 ? resolveDefaultPatchIndex(mode.banks[0]) : 0
            this.applySelection(index, 0, patchIndex, this.midiChannel)
          }
          break
        }
        case 'MidiChannel': {
          const channel = Number(value)
          if (Number.isFinite(channel) && channel >= 1 && channel <= 16) {
            this.midiChannel = channel
            this.resetPendingToLive()
          }
          break
        }
        case 'Page':
          this.navSetPage(Number(value))
          break
        // Config has no meaning with a single loaded config — a no-op.
      }
    },

    // ---- the parameter selector ("which level the arrows steer") ----------

    setCurrentParameter(parameter: NavigationParameter) {
      const index = this.parameters.indexOf(parameter)
      if (index !== -1) this.parameterIndex = index
    },

    cycleCurrentParameterSelection(direction: CycleDirection) {
      const count = this.parameters.length
      if (count === 0) return
      this.parameterIndex = navWrap(this.parameterIndex + (direction === 'Up' ? 1 : -1), count)
    },

    autoSelectPageParameterIfMultiPage() {
      if (this.pageCount <= 1) return
      const index = this.parameters.indexOf('Page')
      if (index !== -1) this.parameterIndex = index
    },

    // ---- action dispatch (IAction::OnPress / OnRelease equivalents) -------

    resolveMidiChannel(actionChannel: number | undefined): number {
      if (actionChannel != null && actionChannel >= 1 && actionChannel <= 16) return actionChannel
      if (this.patch?.midiChannel != null) return this.patch.midiChannel
      if (this.bank?.midiChannel != null) return this.bank.midiChannel
      if (this.mode?.midiChannel != null) return this.mode.midiChannel
      return this.midiChannel
    },

    runActionPress(button: Button, action: Action) {
      const p = action.actionParameters ?? {}
      switch (action.type) {
        case 'PlayNotes': {
          const channel = this.resolveMidiChannel(p.midiChannel)
          const velocity = p.velocity ?? DEFAULT_VELOCITY
          const midiNumbers = (p.noteNames ?? [])
            .map(noteNameToMidi)
            .filter((n): n is number => n != null)
          const spread = p.noteSpread ?? 'NoSpread'
          const order = spread === 'SpreadDown' ? [...midiNumbers].reverse() : spread === 'SpreadRandom' ? shuffle(midiNumbers) : midiNumbers
          const held = this.heldNotes.get(button) ?? []
          order.forEach((midi, i) => {
            held.push({ midi, channel })
            if (spread === 'NoSpread') sendNoteOn(midi, velocity, channel)
            else setTimeout(() => sendNoteOn(midi, velocity, channel), i * NOTE_SPREAD_DELAY_MS)
          })
          this.heldNotes.set(button, held)
          break
        }
        case 'ProgramChange':
          sendProgramChange(p.programNumber ?? 0, this.resolveMidiChannel(p.midiChannel))
          break
        case 'ControlChange':
          sendControlChange(p.controllerNumber ?? 0, p.controlValue ?? 127, this.resolveMidiChannel(p.midiChannel))
          break
        case 'SetPatch':
          if (p.patchIndex != null) this.navSetPatch(p.patchIndex, p.bankIndex, p.modeIndex)
          break
        case 'CyclePatch':
          this.pendingCyclePatch(p.direction ?? 'Up')
          break
        case 'SetBank':
          if (p.bankIndex != null) this.navSetBank(p.bankIndex, p.modeIndex)
          break
        case 'CycleBank':
          this.pendingCycleBank(p.direction ?? 'Up')
          break
        case 'SetMode':
          if (p.modeIndex != null) this.navSetMode(p.modeIndex)
          break
        case 'CycleMode':
          this.pendingCycleMode(p.direction ?? 'Up')
          break
        case 'SetPage':
          if (p.page != null) this.navSetPage(p.page)
          break
        case 'CyclePage':
          this.applyPageStep(p.direction ?? 'Up')
          break
        case 'SetParameter':
          if (p.parameterName) this.setCurrentParameter(p.parameterName as NavigationParameter)
          break
        case 'CycleParameter':
          this.cycleCurrentParameterSelection(p.direction ?? 'Up')
          break
        case 'CycleActivateParameter':
          if (this.dirty) this.activateSelection()
          else this.cycleCurrentParameterSelection('Up')
          break
        case 'SetParameterValue':
          if (p.parameterValue != null) this.applyCurrentParameterValue(p.parameterValue)
          break
        case 'CycleParameterValue':
          this.navStepCurrentParameterValueResolved(p.direction ?? 'Up')
          break
        case 'SetConfig':
        case 'CycleConfig':
          // No meaning with a single loaded config in the editor.
          break
      }
    },

    runActionRelease(button: Button, action: Action) {
      const p = action.actionParameters ?? {}
      switch (action.type) {
        case 'PlayNotes': {
          const held = this.heldNotes.get(button) ?? []
          for (const note of held) sendNoteOff(note.midi, note.channel)
          this.heldNotes.delete(button)
          break
        }
        case 'ControlChange':
          sendControlChange(p.controllerNumber ?? 0, p.releaseValue ?? 0, this.resolveMidiChannel(p.midiChannel))
          break
        default:
          // Program changes and navigation actions are one-shot triggers — nothing to undo on release.
          break
      }
    },

    turnButtonOn(button: Button) {
      this.buttonOn.set(button, true)
      for (const action of flattenActions(button.actions, this.config)) this.runActionPress(button, action)
    },

    turnButtonOff(button: Button) {
      for (const action of flattenActions(button.actions, this.config)) this.runActionRelease(button, action)
      this.buttonOn.set(button, false)
    },

    /** Mirrors applyDefaultButtonStatuses() in MIDI-Toolbox/Runtime.h. */
    applyDefaultButtonStatuses(patchLevelOnly: boolean) {
      selectionPassDepth++
      try {
        this.runApplyDefaultButtonStatuses(patchLevelOnly)
      } finally {
        selectionPassDepth--
      }
    },

    runApplyDefaultButtonStatuses(patchLevelOnly: boolean) {
      const patch = this.patch
      const pages = patchPageCount(patch)
      const savedPage = this.page
      for (let p = 1; p <= pages; p++) {
        this.page = p
        for (const id of useDeviceStore().buttonIds) {
          if (patchLevelOnly && !patch?.buttons.some((b) => b.id === id && buttonOnPage(b, p))) continue
          const resolved = this.resolveAt(id, p)
          if (resolved && resolved.button.defaultStatus === 'On' && !this.buttonOn.get(resolved.button)) {
            this.turnButtonOn(resolved.button)
          }
        }
      }
      this.page = savedPage
    },

    /** Mirrors turnOffAllActiveButtons() in MIDI-Toolbox/Runtime.h. */
    turnOffAllActiveButtons() {
      for (const id of useDeviceStore().buttonIds) {
        if (id === this.buttonBeingPressed) continue
        const resolved = this.resolveAt(id)
        if (resolved && this.buttonOn.get(resolved.button)) this.turnButtonOff(resolved.button)
      }
    },

    /** Mirrors turnOffAllButtonsInPatch() in MIDI-Toolbox/Runtime.h — sweeps every page, not just the current one. */
    turnOffAllButtonsInPatch(patch: Patch) {
      for (const button of patch.buttons) {
        const isBeingPressed = button.id === this.buttonBeingPressed && buttonOnPage(button, this.page)
        if (isBeingPressed) continue
        if (this.buttonOn.get(button)) this.turnButtonOff(button)
      }
    },

    // ---- physical/on-screen footswitch input -------------------------------

    /** Mirrors triggerButton() in MIDI-Toolbox/Runtime.h — the Momentary/Latched/Exclusive dispatch. */
    triggerButtonPress(resolved: ResolvedButton) {
      const button = resolved.button
      const previous = this.buttonBeingPressed
      this.buttonBeingPressed = button.id

      if (button.buttonMode === 'Momentary') {
        this.turnButtonOn(button)
      } else if (button.buttonMode === 'Latched') {
        if (this.buttonOn.get(button)) this.turnButtonOff(button)
        else this.turnButtonOn(button)
      } else if (button.buttonMode === 'Exclusive') {
        if (this.buttonOn.get(button)) {
          this.turnButtonOff(button)
        } else {
          const container = resolved.level === 'builtin' ? undefined : buttonsContainerAt(this.config, resolved.level, this.liveAt)
          for (const siblingId of button.buttonSet ?? []) {
            if (siblingId === button.id) continue
            const sibling = findInLevelContainer(container, resolved.level, siblingId, this.page)
            if (sibling && this.buttonOn.get(sibling)) this.turnButtonOff(sibling)
          }
          this.turnButtonOn(button)
        }
      }

      this.buttonBeingPressed = previous
    },

    /** Mirrors performanceButtonPress() in MIDI-Toolbox/Navigation.h — a footswitch/on-screen press. */
    pressPhysical(id: number) {
      if (!this.active) return
      let resolved = this.resolveAt(id)
      if (!resolved) return
      if (this.dirty && buttonIsPurePerformance(resolved.button, this.config)) {
        this.activateSelection()
        // Activation may have changed the live patch — re-resolve against it.
        resolved = this.resolveAt(id)
        if (!resolved) return
      }
      const button = resolved.button
      const sendsMidi = buttonSendsMidi(button, this.config)
      this.triggerButtonPress(resolved)
      if (sendsMidi && button.name?.trim()) {
        // Latched/Exclusive show the state they've just moved to ("Wah ON" / "Wah OFF").
        const toggles = button.buttonMode === 'Latched' || button.buttonMode === 'Exclusive'
        const text = toggles ? `${button.name.trim()} ${this.buttonOn.get(button) ? 'ON' : 'OFF'}` : button.name.trim()
        this.showToast(text, effectiveColourCode(button, this.bank), button.buttonMode === 'Momentary' ? id : null)
      }
    },

    // ---- button-name overlay (mirrors the toast* functions in MIDI-Toolbox/Navigation.h) ----

    /** Shows the overlay for at least TOAST_MIN_MS, and for as long as `holdId` (a Momentary button) stays held. */
    showToast(text: string, colour: Rgb | undefined, holdId: number | null) {
      clearTimeout(toastTimer)
      this.toastVisible = true
      this.toastText = text
      this.toastColour = colour
      this.toastHoldId = holdId
      this.toastMinElapsed = false
      toastTimer = setTimeout(() => {
        this.toastMinElapsed = true
        if (this.toastHoldId === null) this.toastVisible = false
      }, TOAST_MIN_MS)
    },

    hideToast() {
      clearTimeout(toastTimer)
      this.toastVisible = false
      this.toastHoldId = null
      this.toastMinElapsed = true
    },

    /** A held Momentary button was released: the overlay now ends at its minimum time (straight away if that has passed). */
    toastOnRelease(id: number) {
      if (this.toastHoldId !== id) return
      this.toastHoldId = null
      if (this.toastMinElapsed) this.toastVisible = false
    },

    /** Mirrors releaseButton() in MIDI-Toolbox/Runtime.h — only Momentary buttons act on release. */
    releasePhysical(id: number) {
      if (!this.active) return
      this.toastOnRelease(id)
      const resolved = this.resolveAt(id)
      if (!resolved || resolved.button.buttonMode !== 'Momentary') return
      this.turnButtonOff(resolved.button)
    },
  },
})
