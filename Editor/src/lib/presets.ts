// Presets: named, reusable lists of actions defined once at the top of the file
// (`config.presets`) and run from any button with an `ExecutePreset` action.
//
// A button runs its actions strictly top to bottom, and an ExecutePreset action
// runs that preset's actions at its own position in the list — so a button can
// have a custom action, then a preset, then another custom action. The firmware
// does exactly this expansion when it loads the config (see Button::addPresetActions
// in MIDI-Toolbox/Button.h); `flattenActions` below is the editor's copy of it.
//
// Buttons store only the preset's `id`, so editing a preset changes every button
// that runs it, and deleting one removes it from all of them.

import type { Action, Button, DeviceConfig, Preset } from '../types/config'

/** Shown wherever a preset has no name. */
export const UNNAMED_PRESET = '(unnamed preset)'

export function presetName(preset: Preset): string {
  return preset.name.trim() || UNNAMED_PRESET
}

/** The preset with this id, if there is one. */
export function findPreset(config: DeviceConfig, id: number | undefined): Preset | undefined {
  return id === undefined ? undefined : config.presets?.find((p) => p.id === id)
}

/** A fresh preset id: one more than the highest in use, so ids are small and never clash. */
export function nextPresetId(config: DeviceConfig): number {
  return (config.presets ?? []).reduce((max, p) => Math.max(max, p.id + 1), 0)
}

/**
 * The presets in alphabetical order by name (case-insensitive, locale-aware;
 * unnamed ones sort by their placeholder; ties fall back to id so the order is
 * stable). For display only — `config.presets` itself stays in creation order,
 * and buttons refer to presets by id, so sorting never changes what runs.
 */
export function sortedPresets(config: DeviceConfig): Preset[] {
  return [...(config.presets ?? [])].sort(
    (a, b) =>
      presetName(a).localeCompare(presetName(b), undefined, { sensitivity: 'base' }) || a.id - b.id,
  )
}

/** Whether another preset already has this preset's name (names are allowed to repeat, but it is confusing). */
export function hasDuplicateName(config: DeviceConfig, preset: Preset): boolean {
  const name = preset.name.trim().toLowerCase()
  if (!name) return false
  return (config.presets ?? []).some((p) => p !== preset && p.name.trim().toLowerCase() === name)
}

/**
 * `actions` with every ExecutePreset replaced by that preset's actions, in
 * place. A preset that doesn't exist contributes nothing (the firmware skips it
 * too), and an ExecutePreset inside a preset is ignored (no nesting).
 */
export function flattenActions(actions: Action[], config: DeviceConfig): Action[] {
  const out: Action[] = []
  for (const action of actions) {
    if (action.type !== 'ExecutePreset') {
      out.push(action)
      continue
    }
    const preset = findPreset(config, action.actionParameters?.presetId)
    for (const inner of preset?.actions ?? []) if (inner.type !== 'ExecutePreset') out.push(inner)
  }
  return out
}

/**
 * Whether a preset is already called `name` (ignoring case and surrounding spaces). Preset names
 * must be unique, so this is how a new or renamed preset is checked. Pass `except` to leave one
 * preset out of the check, so a preset never clashes with itself when it is being renamed. An
 * empty name is never "taken": unnamed presets are allowed.
 */
export function isPresetNameTaken(config: DeviceConfig, name: string, except?: Preset): boolean {
  const wanted = name.trim().toLowerCase()
  if (!wanted) return false
  return (config.presets ?? []).some((p) => p !== except && p.name.trim().toLowerCase() === wanted)
}

/** `base` if no preset has that name, otherwise `base 2`, `base 3`, ... the first one that is free. */
export function uniquePresetName(config: DeviceConfig, base: string): string {
  if (!isPresetNameTaken(config, base)) return base
  for (let n = 2; ; n++) {
    const candidate = `${base} ${n}`
    if (!isPresetNameTaken(config, candidate)) return candidate
  }
}

/** Whether `button` runs a preset (has an Execute preset action). */
export function runsAnyPreset(button: Button): boolean {
  return (button.actions ?? []).some((a) => a.type === 'ExecutePreset')
}

/**
 * Whether a button can be turned into a preset: it must have actions to move, and none of them may
 * be Execute preset - a preset can't run another preset, so a button that uses presets has to be
 * reorganised by hand.
 */
export function canCreatePresetFrom(button: Button): boolean {
  return (button.actions?.length ?? 0) > 0 && !runsAnyPreset(button)
}

/** A copy of everything `button` runs, ready to become a new preset's actions (sharing nothing with the button). */
export function actionsForNewPreset(button: Button): Action[] {
  return JSON.parse(JSON.stringify(button.actions ?? [])) as Action[]
}

/** Every `buttons` array in the tree, with where it lives as a path of names ("File", "Pads > Keys", …). */
function* buttonLists(config: DeviceConfig): Generator<{ buttons: Button[]; location: string }> {
  const name = (item: { name: string }, kind: string) => item.name.trim() || `(unnamed ${kind})`
  if (config.buttons) yield { buttons: config.buttons, location: 'File' }
  for (const mode of config.modes) {
    const m = name(mode, 'mode')
    if (mode.buttons) yield { buttons: mode.buttons, location: m }
    for (const bank of mode.banks) {
      const b = `${m} > ${name(bank, 'bank')}`
      if (bank.buttons) yield { buttons: bank.buttons, location: b }
      for (const patch of bank.patches) {
        yield { buttons: patch.buttons, location: `${b} > ${name(patch, 'patch')}` }
      }
    }
  }
}

const runsPreset = (action: Action, id: number) =>
  action.type === 'ExecutePreset' && action.actionParameters?.presetId === id

/** The buttons that run preset `id`, for the delete confirmation. */
export function presetUsers(config: DeviceConfig, id: number): { button: string; location: string }[] {
  const users: { button: string; location: string }[] = []
  for (const { buttons, location } of buttonLists(config)) {
    for (const button of buttons) {
      if (button.actions?.some((a) => runsPreset(a, id))) {
        users.push({ button: button.name?.trim() || `Button ${button.id + 1}`, location })
      }
    }
  }
  return users
}

/**
 * Deletes preset `id` and removes it from every button that runs it. A button
 * left with no actions is removed too (no cosmetic-only buttons). Returns how
 * many buttons ran the preset.
 */
export function removePreset(config: DeviceConfig, id: number): number {
  let users = 0
  for (const { buttons } of buttonLists(config)) {
    for (const button of [...buttons]) {
      if (!button.actions?.some((a) => runsPreset(a, id))) continue
      users++
      button.actions = button.actions.filter((a) => !runsPreset(a, id))
      if (button.actions.length === 0) buttons.splice(buttons.indexOf(button), 1)
    }
  }
  if (config.presets) {
    config.presets = config.presets.filter((p) => p.id !== id)
    if (config.presets.length === 0) delete config.presets
  }
  return users
}

/** Removes every preset that has no actions left (and from the buttons that ran them). */
export function removeEmptyPresets(config: DeviceConfig): void {
  for (const preset of [...(config.presets ?? [])]) {
    if (preset.actions.length === 0) removePreset(config, preset.id)
  }
}
