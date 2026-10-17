// The single source of truth: the parsed config object, the current selection
// path, dirty state, and live validation.

import { defineStore } from 'pinia'
import {
  downloadConfigFile,
  isFileSystemAccessSupported,
  openConfigFile,
  saveConfigFile,
  saveConfigFileAs,
  type OpenedFile,
} from '../lib/fileIo'
import { confirmAction, confirmDelete } from '../lib/confirm'
import { applyStructureChange, findAffectedActions, type ItemPath } from '../lib/targetRemap'
import { validateConfig, type ValidationResult } from '../lib/validate'
import {
  actionsForNewPreset,
  canCreatePresetFrom,
  isPresetNameTaken,
  nextPresetId,
  presetName,
  presetUsers,
  removePreset,
  uniquePresetName,
} from '../lib/presets'
import { promptText } from '../lib/prompt'
import { fillActionDefaults, newPlayNotesAction } from '../lib/actionDefaults'
import {
  BUTTON_LEVEL_LABEL,
  buttonsContainerAt,
  findButtonIndexById,
  findPageConflict,
  inheritedButtons,
  isCosmeticButton,
  navButtonDefaultRole,
  pruneCosmeticButtons,
  remapButtonSetIds,
  resolveButtonFrom,
  type ResolvedButton,
} from '../lib/buttons'
import {
  bankButtonPath,
  bankPath,
  buttonPath,
  fileButtonPath,
  filePath,
  modeButtonPath,
  modePath,
  parseSelection,
  patchPath,
  presetPath,
  type ButtonOwnerLevel,
  type ParsedSelection,
} from '../lib/selection'
import { liveRigConfig, liveRigFileName } from '../data/liveRig'
import { useDeviceStore } from './device'
import type { Bank, Button, DeviceConfig, Mode, Patch, Preset, SelectionPath } from '../types/config'

function startingConfig(): DeviceConfig {
  return structuredClone(liveRigConfig)
}

function emptyConfig(): DeviceConfig {
  return {
    parameters: ['Patch', 'Bank', 'Mode', 'MidiChannel', 'Config'],
    modes: [
      {
        name: 'Mode 1',
        banks: [
          {
            name: 'Bank 1',
            patches: [{ name: 'Patch 1', buttons: [] }],
          },
        ],
      },
    ],
  }
}

// FX / Stompboxes / Pedalboard — the context the button-edit work assumes.
const initialSelection: SelectionPath = patchPath(1, 0, 0)

interface State {
  config: DeviceConfig
  fileName: string | null
  openedFile: OpenedFile | null
  selection: SelectionPath
  /** Current page within a multi-page patch (1-based). */
  page: number
  /**
   * Working copy of the selected Exclusive button's `buttonSet` while the
   * "Button set" toggle is engaged. Non-null means the footswitch board is in
   * set-selection mode: a footswitch press toggles that id here instead of
   * selecting a button, and "Save" commits this back onto the button. Navigating
   * away discards it.
   */
  buttonSetDraft: number[] | null
  dirty: boolean
  parseError: string | null
}

export const useConfigStore = defineStore('config', {
  state: (): State => ({
    config: startingConfig(),
    fileName: liveRigFileName,
    openedFile: null,
    selection: initialSelection,
    page: 1,
    buttonSetDraft: null,
    dirty: false,
    parseError: null,
  }),

  getters: {
    validation(state): ValidationResult {
      return validateConfig(state.config, useDeviceStore().profile)
    },
    isValid(): boolean {
      return this.validation.valid
    },
    canSaveInPlace(state): boolean {
      return isFileSystemAccessSupported() && state.openedFile?.handle != null
    },
    /**
     * JSON as it will be written to disk — cosmetic-only buttons removed, and every Play notes
     * action carrying its velocity and every control change its On and Off value (a file that left
     * them out is written with the defaults spelled out).
     */
    json(state): string {
      const out = pruneCosmeticButtons(state.config)
      fillActionDefaults(out)
      return JSON.stringify(out, null, 2) + '\n'
    },

    parsed(state): ParsedSelection {
      return parseSelection(state.selection)
    },

    /** The mode in scope for the current selection (mode level or deeper). */
    activeMode(state): Mode | undefined {
      const p = parseSelection(state.selection)
      if (p.modeIndex == null) return undefined
      return state.config.modes[p.modeIndex]
    },

    /** The bank in scope for the current selection (bank level or deeper). */
    activeBank(state): Bank | undefined {
      const p = parseSelection(state.selection)
      if (p.modeIndex == null || p.bankIndex == null) return undefined
      return state.config.modes[p.modeIndex]?.banks[p.bankIndex]
    },

    /** The patch in scope for the current selection (patch or button level). */
    activePatch(state): Patch | undefined {
      const p = parseSelection(state.selection)
      if (p.modeIndex == null || p.bankIndex == null || p.patchIndex == null)
        return undefined
      return state.config.modes[p.modeIndex]?.banks[p.bankIndex]?.patches[
        p.patchIndex
      ]
    },

    /** The preset open in the Preset pane, if one is selected. */
    selectedPreset(state): Preset | undefined {
      const p = parseSelection(state.selection)
      return p.level === 'preset' && p.presetIndex != null ? state.config.presets?.[p.presetIndex] : undefined
    },

    selectedButton(state): Button | undefined {
      const p = parseSelection(state.selection)
      if (p.level !== 'button' || p.buttonIndex == null) return undefined
      return buttonsContainerAt(state.config, p.buttonOwnerLevel!, p)?.[p.buttonIndex]
    },

    /**
     * Which container the device board/screen and the accordion's button
     * editing currently act on: the active pane's own level, or — while a
     * Button is open — the level that button itself belongs to. E.g. editing
     * a Bank-owned button still keeps the board scoped to that Bank.
     */
    editingLevel(): ButtonOwnerLevel {
      const p = this.parsed
      // A preset belongs to the File, and its actions have no more specific location.
      if (p.level === 'preset') return 'file'
      return p.level === 'button' ? p.buttonOwnerLevel! : p.level
    },

    /**
     * How many pages of buttons the active patch defines — the highest
     * Button.page in it, at least 1 (page 0 = "every page" never raises it).
     * Mirrors patchPageCount() in MIDI-Toolbox/Runtime.h.
     */
    pageCount(): number {
      const patch = this.activePatch
      if (!patch) return 1
      return patch.buttons.reduce((max, b) => Math.max(max, b.page ?? 1), 1)
    },

    /** The visible page, always clamped into [1, pageCount] however `page` drifted. */
    currentPage(): number {
      return Math.min(Math.max(1, this.page), this.pageCount)
    },

    /** True while the "Button set" toggle is engaged (an unsaved draft exists). */
    buttonSetEditing(state): boolean {
      return state.buttonSetDraft !== null
    },

    /**
     * The button list for a Mode/Bank/Patch/File pane's own "Buttons"
     * section: `own` is that level's own buttons array (empty if it hasn't
     * defined any), `inherited` is every button it inherits from a
     * less-specific level (Bank/Mode/Config) that it has not itself
     * overridden — see inheritedButtons() for how those are resolved. Used
     * by SimplePane / FilePane / PatchPane to render both sections.
     */
    buttonEditor() {
      return (level: ButtonOwnerLevel) => {
        const p = this.parsed
        const own = (buttonsContainerAt(this.config, level, p) ?? []).map(
          (button, index) => ({ button, index }),
        )
        return { own, inherited: inheritedButtons(this.config, level, p, this.currentPage) }
      }
    },
  },

  actions: {
    /**
     * Move the selection. Any button that was created by selecting an empty
     * footswitch but never given an action is dropped on the way out, so the
     * config never carries cosmetic-only buttons.
     */
    select(path: SelectionPath) {
      const before = this.parsed
      const next = parseSelection(path)
      // Moving to a different patch resets the page cursor; moving within the
      // same patch (e.g. picking one of its buttons) keeps the current page.
      if (
        before.modeIndex !== next.modeIndex ||
        before.bankIndex !== next.bankIndex ||
        before.patchIndex !== next.patchIndex
      ) {
        this.page = 1
      }
      // Button-set editing is bound to one button; leaving it discards the draft.
      this.buttonSetDraft = null
      this.selection = this.dropDraftButtons(path)
    },

    /**
     * The "Button set" toggle. Off → on starts a draft from the selected
     * Exclusive button's current set (always including itself). On → off
     * ("Save") commits the draft back onto the button. A non-Exclusive selection just clears any draft.
     */
    toggleButtonSetEditing() {
      if (this.buttonSetDraft !== null) {
        // Save: write the draft (or remove an emptied set) back onto the button.
        const button = this.selectedButton
        if (button) {
          button.buttonSet = [...this.buttonSetDraft]
          this.dirty = true
        }
        this.buttonSetDraft = null
        return
      }
      if (this.selectedButton?.buttonMode !== 'Exclusive') return
      // The button itself is always a member of its own set.
      const set = new Set(this.selectedButton.buttonSet ?? [])
      set.add(this.selectedButton.id)
      this.buttonSetDraft = [...set].sort((a, b) => a - b)
    },

    /** Discard an in-progress "Button set" draft without saving it. */
    discardButtonSetDraft() {
      this.buttonSetDraft = null
    },

    /** Add / remove a footswitch id from the working button-set draft. */
    toggleButtonInSet(id: number) {
      if (this.buttonSetDraft === null) return
      // The edited button can't be removed from its own set.
      if (id === this.selectedButton?.id) return
      const set = new Set(this.buttonSetDraft)
      if (set.has(id)) set.delete(id)
      else set.add(id)
      this.buttonSetDraft = [...set].sort((a, b) => a - b)
    },

    /**
     * A footswitch press on the board (or a click on the matching device-
     * screen cell): toggles set membership while "Button set" mode is on,
     * otherwise activates that id at the current editingLevel — see
     * activateButtonAtLevel().
     */
    pressFootswitch(id: number) {
      if (this.buttonSetEditing) {
        this.toggleButtonInSet(id)
        return
      }
      return this.selectButtonId(id)
    },

    /** Step the visible page within the active patch, wrapping in [1, pageCount]. */
    cyclePage(step: 1 | -1) {
      const count = this.pageCount
      if (count <= 1) {
        this.page = 1
        return
      }
      this.page = ((this.currentPage - 1 + step + count) % count) + 1
    },

    /** Jump the visible page to `n`, clamped into [1, pageCount]. */
    setDisplayPage(n: number) {
      this.page = Math.min(Math.max(1, n), this.pageCount)
    },

    /**
     * Prunes cosmetic (action-less) buttons from every container at every
     * level — File, each Mode, each Bank, each Patch — on the way to
     * `nextPath`, except the button `nextPath` itself is about to land on
     * (which may itself be a fresh, still action-less button). Returns
     * `nextPath` adjusted for any index shift that pruning caused ahead of
     * its own target button in the same container.
     */
    dropDraftButtons(nextPath: SelectionPath): SelectionPath {
      const next = parseSelection(nextPath)
      const adjusted: SelectionPath = [...nextPath]

      const prune = (
        buttons: Button[] | undefined,
        isTargetContainer: boolean,
        pathIndexPos: number,
      ) => {
        if (!buttons) return
        for (let i = buttons.length - 1; i >= 0; i--) {
          if (!isCosmeticButton(buttons[i])) continue
          const isTarget = isTargetContainer && next.buttonIndex === i
          if (isTarget) continue
          buttons.splice(i, 1)
          if (isTargetContainer && next.buttonIndex != null && next.buttonIndex > i) {
            adjusted[pathIndexPos] = (adjusted[pathIndexPos] as number) - 1
          }
        }
      }

      prune(this.config.buttons, next.level === 'button' && next.buttonOwnerLevel === 'file', 1)

      this.config.modes.forEach((mode, mi) => {
        prune(
          mode.buttons,
          next.level === 'button' && next.buttonOwnerLevel === 'mode' && next.modeIndex === mi,
          3,
        )
        mode.banks.forEach((bank, bi) => {
          prune(
            bank.buttons,
            next.level === 'button' &&
              next.buttonOwnerLevel === 'bank' &&
              next.modeIndex === mi &&
              next.bankIndex === bi,
            5,
          )
          bank.patches.forEach((patch, pi) => {
            prune(
              patch.buttons,
              next.level === 'button' &&
                next.buttonOwnerLevel === 'patch' &&
                next.modeIndex === mi &&
                next.bankIndex === bi &&
                next.patchIndex === pi,
              7,
            )
          })
        })
      })
      return adjusted
    },

    /**
     * The `buttons` array `level` owns directly, creating it (as `[]`) if it
     * doesn't exist yet. Returns undefined when `level` isn't reachable from
     * the current selection (e.g. the Bank level while no bank is selected).
     */
    ensureButtonsArray(level: ButtonOwnerLevel): Button[] | undefined {
      const p = this.parsed
      if (level === 'file') {
        if (!this.config.buttons) this.config.buttons = []
        return this.config.buttons
      }
      if (p.modeIndex == null) return undefined
      const mode = this.config.modes[p.modeIndex]
      if (level === 'mode') {
        if (!mode.buttons) mode.buttons = []
        return mode.buttons
      }
      if (p.bankIndex == null) return undefined
      const bank = mode.banks[p.bankIndex]
      if (level === 'bank') {
        if (!bank.buttons) bank.buttons = []
        return bank.buttons
      }
      if (p.patchIndex == null) return undefined
      return bank.patches[p.patchIndex].buttons
    },

    /** Build the selection path for the button at `index` within `level`'s own array. */
    buttonPathAt(level: ButtonOwnerLevel, index: number): SelectionPath {
      const p = this.parsed
      switch (level) {
        case 'file':
          return fileButtonPath(index)
        case 'mode':
          return modeButtonPath(p.modeIndex!, index)
        case 'bank':
          return bankButtonPath(p.modeIndex!, p.bankIndex!, index)
        case 'patch':
          return buttonPath(p.modeIndex!, p.bankIndex!, p.patchIndex!, index)
      }
    },

    /** Open the Button pane for an already-own button at `level`, index `index`. */
    selectButtonAtLevel(level: ButtonOwnerLevel, index: number) {
      if (level === 'patch') {
        // Bring the button's own page into view, like selectButtonAt() does.
        const patch = this.activePatch
        this.setDisplayPage(patch?.buttons[index]?.page ?? 1)
      }
      this.select(this.buttonPathAt(level, index))
    },

    /**
     * Activates physical footswitch `id` at `level` (the board/screen's
     * current editingLevel): if `level` already defines it, opens the Button
     * pane on it. If it's only defined at a less-specific level (Bank/Mode/
     * Config), asks to confirm overriding it here — copying the full
     * definition down to `level`, since overriding replaces rather than
     * merges (see resolveButtonContainer() in MIDI-Toolbox/Runtime.h). If
     * nothing defines it anywhere, creates a blank button at `level` (dropped
     * again unless it gets an action — see dropDraftButtons()).
     */
    async activateButtonAtLevel(level: ButtonOwnerLevel, id: number) {
      const resolved = resolveButtonFrom(
        this.config,
        useDeviceStore().profile,
        level,
        this.parsed,
        id,
        this.currentPage,
      )
      if (resolved && resolved.level === level) {
        this.selectButtonAtLevel(level, resolved.index)
        return
      }
      if (resolved) {
        if (!(await this.confirmOverride(level, resolved))) return
        this.overrideButtonAtLevel(level, resolved.button)
        return
      }
      this.createButtonAtLevel(level, id)
    },

    /**
     * Extracted for testability and reuse; the actual confirmation prompt for
     * an override. A `resolved.level === 'builtin'` result (one of the active
     * device's three navigation ids only — see builtInNavButtons() in
     * lib/buttons.ts) gets a deliberately stronger, more specific warning
     * than an ordinary Bank/Mode/File override, since it's replacing one of
     * the physical device's built-in navigation controls rather than another
     * config author's own button.
     */
    confirmOverride(level: ButtonOwnerLevel, resolved: ResolvedButton): Promise<boolean> {
      if (resolved.level === 'builtin') {
        const role = navButtonDefaultRole(useDeviceStore().profile, resolved.button.id) ?? 'a built-in navigation control'
        return confirmAction(
          `Footswitch ${resolved.button.id + 1} is currently built in: it ${role}. Overriding it ` +
            `at ${BUTTON_LEVEL_LABEL[level]} level replaces that on the PHYSICAL footswitch only — ` +
            `the on-screen parameter buttons on the device screen will keep working exactly as ` +
            `before, so navigation is never lost. This cannot be undone from here once saved; you'd ` +
            `need to delete the override to restore the built-in behaviour. Override footswitch ` +
            `${resolved.button.id + 1}?`,
          { title: 'Override built-in navigation control', confirmLabel: 'Override', cancelLabel: 'Cancel' },
        )
      }
      const label = resolved.button.name?.trim() || `footswitch ${resolved.button.id + 1}`
      return confirmAction(
        `“${label}” is defined at ${BUTTON_LEVEL_LABEL[resolved.level]} level. Override it at ` +
          `${BUTTON_LEVEL_LABEL[level]} level? This copies its full definition here — edits will ` +
          `only apply at ${BUTTON_LEVEL_LABEL[level]} level, and it will stop following the ` +
          `${BUTTON_LEVEL_LABEL[resolved.level]}-level button.`,
        { title: 'Override button', confirmLabel: 'Override', cancelLabel: 'Cancel' },
      )
    },

    /** Create a fresh, action-less button for `id` directly on `level` and select it. */
    createButtonAtLevel(level: ButtonOwnerLevel, id: number) {
      const buttons = this.ensureButtonsArray(level)
      if (!buttons) return
      const button: Button = { id, name: '', buttonMode: 'Momentary', actions: [] }
      if (level === 'patch') button.page = this.currentPage
      buttons.push(button)
      this.selectButtonAtLevel(level, buttons.length - 1)
    },

    /**
     * Copy `source` (a button defined at a less-specific level) onto `level`
     * as a new, independent button, and select it for editing. `page` is only
     * meaningful on a Patch button — the source never has one (only Patch
     * buttons do), and the copy is scoped to the page currently in view.
     */
    overrideButtonAtLevel(level: ButtonOwnerLevel, source: Button) {
      const buttons = this.ensureButtonsArray(level)
      if (!buttons) return
      const clone: Button = JSON.parse(JSON.stringify(source))
      if (level === 'patch') clone.page = this.currentPage
      else delete clone.page
      buttons.push(clone)
      this.dirty = true
      this.selectButtonAtLevel(level, buttons.length - 1)
    },

    /**
     * Select the button bound to physical footswitch `id` at the current
     * editingLevel (opening the Button pane), creating a blank one — or
     * offering to override an inherited one — if nothing of its own is bound
     * there yet. See activateButtonAtLevel().
     */
    selectButtonId(id: number) {
      return this.activateButtonAtLevel(this.editingLevel, id)
    },

    /**
     * Select a button by its index in the active patch's `buttons` array
     * (used by the Patch pane's all-pages list) and bring its page into view.
     */
    selectButtonAt(index: number) {
      const p = this.parsed
      if (p.modeIndex == null || p.bankIndex == null || p.patchIndex == null)
        return
      const patch =
        this.config.modes[p.modeIndex].banks[p.bankIndex].patches[p.patchIndex]
      const button = patch.buttons[index]
      if (!button) return
      this.setDisplayPage(button.page ?? 1)
      this.select(buttonPath(p.modeIndex, p.bankIndex, p.patchIndex, index))
    },

    /**
     * Move the selected button to `page`. Refused (returns the clashing button)
     * when another button in the patch already holds this id on that page,
     * since (id, page) is a composite key.
     */
    setButtonPage(page: number): { ok: boolean; conflict?: Button } {
      const p = this.parsed
      if (
        p.level !== 'button' ||
        p.modeIndex == null ||
        p.bankIndex == null ||
        p.patchIndex == null ||
        p.buttonIndex == null
      )
        return { ok: false }
      const patch =
        this.config.modes[p.modeIndex].banks[p.bankIndex].patches[p.patchIndex]
      const conflict = findPageConflict(patch.buttons, p.buttonIndex, page)
      if (conflict) return { ok: false, conflict }
      patch.buttons[p.buttonIndex].page = page
      this.dirty = true
      this.setDisplayPage(page)
      return { ok: true }
    },

    /**
     * Swap the physical slot of two buttons in the active patch: their `id` and
     * `page` trade places, so each button lands where the other one was, even
     * when that is a different page. Drives the Patch pane's drag-and-drop.
     *
     * (id, page) is a composite key and both slots are already occupied, so the
     * swap can never create a conflict. Only the slot moves — each button keeps
     * its own name, colour, mode and actions.
     *
     * Exclusive `buttonSet` arrays list sibling button *ids* (resolved within
     * the patch, see triggerButton() in MIDI-Toolbox/Runtime.h). When the two
     * buttons trade ids, every set in the patch that named one is rewritten to
     * name the other, so an exclusive group keeps following its buttons rather
     * than the footswitches they sit on.
     */
    swapButtonSlots(indexA: number, indexB: number) {
      const p = this.parsed
      if (p.modeIndex == null || p.bankIndex == null || p.patchIndex == null)
        return
      if (indexA === indexB) return
      const patch =
        this.config.modes[p.modeIndex].banks[p.bankIndex].patches[p.patchIndex]
      const a = patch.buttons[indexA]
      const b = patch.buttons[indexB]
      if (!a || !b) return
      const aId = a.id
      const bId = b.id
      const aHadPage = 'page' in a
      const bHadPage = 'page' in b
      const aPage = a.page ?? 1
      const bPage = b.page ?? 1
      a.id = bId
      b.id = aId
      // Each button takes the other's page. Only spell out `page` when it is not
      // the default 1, or when this button already carried an explicit `page` —
      // so a swap does not churn the representation of either style of patch.
      // Page 0 ("every page") is a real value and always kept.
      if (bPage === 1 && !aHadPage) delete a.page
      else a.page = bPage
      if (aPage === 1 && !bHadPage) delete b.page
      else b.page = aPage
      // Follow the id swap through every buttonSet in the patch (no-op for a
      // pure page swap, where no id changed).
      remapButtonSetIds(patch.buttons, aId, bId)
      this.dirty = true
    },

    /**
     * Swap the buttons on footswitches `idA` and `idB` of the current page —
     * the device board's drag-and-drop. Unlike swapButtonSlots this works in
     * physical-id space on one page (the board only ever shows one), and either
     * slot may be empty: an empty target simply receives the dragged button.
     * `buttonSet` references are rewired the same way.
     *
     * Callers are expected to have already warned the user when `idA` or `idB`
     * is one of the active device's navigation footswitches; this method just
     * performs the move.
     */
    swapFootswitchButtons(idA: number, idB: number) {
      const p = this.parsed
      if (p.modeIndex == null || p.bankIndex == null || p.patchIndex == null)
        return
      if (idA === idB) return
      const patch =
        this.config.modes[p.modeIndex].banks[p.bankIndex].patches[p.patchIndex]
      const page = this.currentPage
      const ia = findButtonIndexById(patch.buttons, idA, page)
      const ib = findButtonIndexById(patch.buttons, idB, page)
      if (ia === -1 && ib === -1) return
      // Page is untouched — a page-0 ("every page") button stays page 0, it just
      // moves footswitch.
      if (ia !== -1) patch.buttons[ia].id = idB
      if (ib !== -1) patch.buttons[ib].id = idA
      remapButtonSetIds(patch.buttons, idA, idB)
      this.dirty = true
    },

    deleteSelectedButton() {
      const p = this.parsed
      if (p.level !== 'button' || p.buttonIndex == null) return
      const buttons = buttonsContainerAt(this.config, p.buttonOwnerLevel!, p)
      if (!buttons) return
      buttons.splice(p.buttonIndex, 1)
      this.dirty = true
      switch (p.buttonOwnerLevel) {
        case 'patch':
          // Deleting the last button on a page can shrink pageCount.
          this.page = this.currentPage
          this.select(patchPath(p.modeIndex!, p.bankIndex!, p.patchIndex!))
          break
        case 'bank':
          this.select(bankPath(p.modeIndex!, p.bankIndex!))
          break
        case 'mode':
          this.select(modePath(p.modeIndex!))
          break
        case 'file':
          this.select(filePath())
          break
      }
    },

    /** Adds a preset (starting with one empty Play notes action, like a new button's) and opens it. */
    addPreset() {
      this.mutate((c) => {
        const presets = (c.presets ??= [])
        presets.push({
          id: nextPresetId(c),
          name: uniquePresetName(c, 'New preset'),
          actions: [newPlayNotesAction()],
        })
      })
      this.select(presetPath(this.config.presets!.length - 1))
    },

    /**
     * Moves every action of the open button into a new preset, after asking for the preset's name,
     * and leaves the button with a single "Execute preset" action that runs it. Preset names must
     * be unique, so the dialog will not accept one that is taken. Only possible for a button that
     * has actions and runs no preset (a preset can't run another preset). Resolves true if a
     * preset was created; false if the dialog was cancelled or there was nothing to move.
     */
    async createPresetFromSelectedButton(): Promise<boolean> {
      const button = this.selectedButton
      if (!button || !canCreatePresetFrom(button)) return false

      const name = await promptText(
        "All of this button's actions will move into a new preset, and the button will run that preset instead.",
        {
          title: 'Create preset',
          initial: button.name?.trim() ?? '',
          placeholder: 'Preset name',
          confirmLabel: 'Create',
          problem: (text) =>
            isPresetNameTaken(this.config, text) ? 'A preset with this name already exists. Choose a different name.' : '',
        },
      )
      if (name === null) return false
      // the player may have moved to another button, or the presets may have changed, while the dialog was open
      if (this.selectedButton !== button || isPresetNameTaken(this.config, name)) return false

      this.mutate((c) => {
        const id = nextPresetId(c)
        // taken before the button's own list is replaced
        const actions = actionsForNewPreset(button)
        ;(c.presets ??= []).push({ id, name, actions })
        button.actions = [{ type: 'ExecutePreset', actionParameters: { presetId: id } }]
      })
      return true
    },

    /**
     * Deletes the preset at `index` after confirming. Every button action that
     * runs it is removed too, and a button left with no actions goes with it —
     * the dialog says how many buttons that affects. Resolves true if deleted.
     */
    async deletePreset(index: number): Promise<boolean> {
      const preset = this.config.presets?.[index]
      if (!preset) return false
      const label = `the preset “${presetName(preset)}”`
      const users = presetUsers(this.config, preset.id)
      const confirmed =
        users.length === 0
          ? await confirmDelete(label)
          : await confirmAction(
              `Are you sure you want to delete ${label}? ` +
                `It will be removed from ${users.length === 1 ? 'this button' : 'these buttons'} ` +
                '(a button left with no actions is removed too):',
              {
                title: 'Delete preset?',
                details: users.map((u) => `“${u.button}” (${u.location})`),
              },
            )
      if (!confirmed) return false
      // Leave the preset pane first: its path is about to point at nothing.
      if (this.parsed.level === 'preset') this.select(filePath())
      this.mutate((c) => {
        removePreset(c, preset.id)
      })
      return true
    },

    /** Apply an edit to the current config and flag it dirty. */
    mutate(fn: (config: DeviceConfig) => void) {
      fn(this.config)
      this.dirty = true
    },

    /**
     * Deletes the mode/bank/patch at `path` after confirming. If any button's
     * Set patch/bank/mode action points at it (or anything inside it), the
     * dialog lists them and says they will be removed; confirming deletes the
     * item and removes those actions (and any button left with none), while
     * every other Set action is renumbered to keep pointing at the same thing.
     * Resolves true if it was deleted.
     */
    async removeItem(path: ItemPath, label: string): Promise<boolean> {
      const change = { kind: 'remove', path } as const
      const affected = findAffectedActions(this.config, change)
      const confirmed =
        affected.length === 0
          ? await confirmDelete(label)
          : await confirmAction(
              `Are you sure you want to delete ${label}? ` +
                `${affected.length === 1 ? 'This button action points' : 'These button actions point'} at it, or at something inside it, ` +
                `and will be removed from ${affected.length === 1 ? 'its button' : 'their buttons'} ` +
                '(a button left with no actions is removed too):',
              {
                title: 'Delete and remove actions?',
                details: affected.map((a) => `“${a.button}” (${a.location}): ${a.action} → ${a.target}`),
              },
            )
      if (!confirmed) return false
      this.mutate((c) => {
        applyStructureChange(c, change)
      })
      return true
    },

    /**
     * Moves the child at `from` so it ends up at index `to` in the list under
     * `parent` ([] = the modes, [mode] = its banks, [mode, bank] = its patches),
     * the ones in between shifting by one — keeping every Set patch/bank/mode
     * action and bank default patch pointing at what it pointed at before.
     */
    moveItem(parent: ItemPath, from: number, to: number) {
      if (from === to || from < 0 || to < 0) return
      this.mutate((c) => {
        applyStructureChange(c, { kind: 'move', parent, from, to })
      })
    },

    markDirty() {
      this.dirty = true
    },

    revert() {
      if (this.openedFile) {
        try {
          this.config = JSON.parse(this.openedFile.text) as DeviceConfig
        } catch (err) {
          this.parseError = err instanceof Error ? err.message : String(err)
          return
        }
      } else {
        this.config = startingConfig()
      }
      this.selection = this.selection.length ? initialSelection : []
      this.buttonSetDraft = null
      this.dirty = false
    },

    /** Ask before discarding unsaved changes; resolves true when it's fine to proceed. */
    async confirmDiscardUnsavedChanges(verb: string): Promise<boolean> {
      if (!this.dirty) return true
      return confirmAction(`Discard unsaved changes and ${verb}?`, {
        title: 'Unsaved changes',
        confirmLabel: 'Discard',
        cancelLabel: 'Cancel',
      })
    },

    async newConfig() {
      if (!(await this.confirmDiscardUnsavedChanges('start a new config'))) return
      this.config = emptyConfig()
      this.fileName = null
      this.openedFile = null
      this.selection = ['modes', 0]
      this.page = 1
      this.buttonSetDraft = null
      this.dirty = false
      this.parseError = null
    },

    async open() {
      if (!(await this.confirmDiscardUnsavedChanges('open a different file'))) return
      const opened = await openConfigFile()
      if (!opened) return
      try {
        const parsed = JSON.parse(opened.text) as DeviceConfig
        this.config = parsed
        this.openedFile = opened
        this.fileName = opened.name
        // Land on the File pane rather than drilling straight into a mode, so the
        // user can see what the file contains and pick (or add) a mode from there.
        this.selection = filePath()
        this.page = 1
        this.buttonSetDraft = null
        this.dirty = false
        this.parseError = null
      } catch (err) {
        const message = err instanceof Error ? err.message : String(err)
        this.parseError = `${opened.name}: ${message}`
      }
    },

    /**
     * Empties every `buttons` array in the file — File, every Mode, every
     * Bank, every Patch — while leaving everything else (names, parameters,
     * midi channels, the file/mode/bank/patch tree itself) untouched. Used
     * when switching to a different device profile (see
     * ChangeDeviceLink.vue): button ids and roles are only meaningful for
     * one specific device, so there is nothing sensible to carry across a
     * switch — see the editor-device-switching-is-destructive decision.
     */
    clearAllButtons() {
      this.config.buttons = []
      for (const mode of this.config.modes) {
        mode.buttons = []
        for (const bank of mode.banks) {
          bank.buttons = []
          for (const patch of bank.patches) patch.buttons = []
        }
      }
      this.selection = filePath()
      this.buttonSetDraft = null
      this.dirty = true
    },

    /** Export the current config as a downloaded .json file; the edits are then no longer "not yet downloaded". */
    download() {
      downloadConfigFile(this.fileName ?? 'config.json', this.json)
      this.dirty = false
    },

    async save() {
      if (this.openedFile) {
        await saveConfigFile(this.openedFile, this.json)
        this.dirty = false
        return
      }
      await this.saveAs()
    },

    async saveAs() {
      const name = this.fileName ?? 'config.json'
      const result = await saveConfigFileAs(name, this.json)
      if (result) {
        this.openedFile = result
        this.fileName = result.name
      }
      this.dirty = false
    },
  },
})
