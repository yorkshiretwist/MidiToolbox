// Keeping SetPatch / SetBank / SetMode actions pointing at the right thing when
// modes, banks or patches are reordered or deleted.
//
// A Set action stores indices (see lib/actionTargets.ts), so renumbering the
// items it points at would silently re-aim it. Both operations here therefore
// work on the target an action means, not the numbers it happens to store:
//   1. before the change, resolve every Set action to the absolute item it names;
//   2. apply the change to the tree;
//   3. work out where that item (and the button's own container) ended up, and
//      rewrite the stored indices only if they no longer mean the same thing;
//   4. for a deletion, remove the actions that pointed at something now gone.
// Bank `defaultPatchIndex` is an index into the bank's patches and is kept
// pointing at the same patch the same way.

import type { Action, Button, DeviceConfig, Preset } from '../types/config'
import { removeEmptyPresets, UNNAMED_PRESET } from './presets'
import {
  canonicalParams,
  isIndexedTargetType,
  resolveTarget,
  targetKeys,
  type IndexedTargetType,
  type TargetScope,
  type TargetTuple,
} from './actionTargets'

/** A mode, bank or patch by absolute indices: [mode], [mode, bank] or [mode, bank, patch]. */
export type ItemPath = readonly number[]

export type StructureChange =
  | { kind: 'remove'; path: ItemPath }
  /**
   * Move the child at `from` so it ends up at index `to` of the list under
   * `parent` ([] = the modes, [m] = a mode's banks, [m, b] = a bank's patches);
   * the ones in between shift by one. (1, 2, 3, 4, 5) with 1 moved to index 2
   * becomes (2, 3, 1, 4, 5).
   */
  | { kind: 'move'; parent: ItemPath; from: number; to: number }

/** An action that the change would delete, described for the confirmation dialog. */
export interface AffectedAction {
  /** The button's (or preset's) name — "Button N" if a button has none. */
  button: string
  /** Where the button lives: "File", "Pads", "Pads > Keys" or "Pads > Keys > Intro" — or "Preset" for an action inside a preset. */
  location: string
  /** Human name of the action type, e.g. "Set patch". */
  action: string
  /** What it points at, as a path of names, e.g. "Pads > Keys > Intro". */
  target: string
}

/** Where `path` ends up after `change`, or null if it is (inside) the item removed. */
export function mapPath(path: ItemPath, change: StructureChange): ItemPath | null {
  if (change.kind === 'remove') {
    const depth = change.path.length
    if (path.length < depth) return path
    const parentSame = change.path.slice(0, depth - 1).every((v, i) => path[i] === v)
    if (!parentSame) return path
    const at = path[depth - 1]
    const removed = change.path[depth - 1]
    if (at === removed) return null
    if (at > removed) {
      const next = [...path]
      next[depth - 1] = at - 1
      return next
    }
    return path
  }

  const depth = change.parent.length
  if (path.length <= depth) return path
  if (!change.parent.every((v, i) => path[i] === v)) return path
  const at = path[depth]
  const { from, to } = change
  let mapped = at
  if (at === from) mapped = to
  else if (from < to && at > from && at <= to) mapped = at - 1
  else if (from > to && at >= to && at < from) mapped = at + 1
  if (mapped === at) return path
  const next = [...path]
  next[depth] = mapped
  return next
}

const ACTION_LABEL: Record<IndexedTargetType, string> = {
  SetPatch: 'Set patch',
  SetBank: 'Set bank',
  SetMode: 'Set mode',
}

/** Presets are scanned like File-level buttons (a preset has no single location, so its targets are always fully qualified). */
type ActionHost = Button | Preset

interface ActionRef {
  buttons: ActionHost[]
  button: ActionHost
  action: Action
  type: IndexedTargetType
  /** Where the container (file/mode/bank/patch) sits before the change. */
  location: ItemPath
  target: TargetTuple
}

function scopeOfLocation(location: ItemPath): TargetScope {
  return { modeIndex: location[0], bankIndex: location[1] }
}

/** Every Set* action whose stored indices resolve to one specific item, with where it lives. */
function collectActionRefs(config: DeviceConfig): ActionRef[] {
  const refs: ActionRef[] = []
  const scan = (buttons: ActionHost[] | undefined, location: ItemPath) => {
    if (!buttons) return
    for (const button of buttons) {
      for (const action of button.actions ?? []) {
        if (!isIndexedTargetType(action.type)) continue
        const target = resolveTarget(action.type, action.actionParameters ?? {}, scopeOfLocation(location))
        if (target) refs.push({ buttons, button, action, type: action.type, location, target })
      }
    }
  }
  scan(config.buttons, [])
  scan(config.presets, [])
  config.modes.forEach((mode, m) => {
    scan(mode.buttons, [m])
    mode.banks.forEach((bank, b) => {
      scan(bank.buttons, [m, b])
      bank.patches.forEach((patch, p) => scan(patch.buttons, [m, b, p]))
    })
  })
  return refs
}

function nameAtPath(config: DeviceConfig, path: ItemPath): string {
  const names: string[] = []
  const mode = config.modes[path[0]]
  if (mode) names.push(mode.name.trim() || '(unnamed mode)')
  const bank = path.length > 1 ? mode?.banks[path[1]] : undefined
  if (bank) names.push(bank.name.trim() || '(unnamed bank)')
  const patch = path.length > 2 ? bank?.patches[path[2]] : undefined
  if (patch) names.push(patch.name.trim() || '(unnamed patch)')
  return names.join(' > ')
}

/** Whether `path` is the removed item or inside it (a target or container that goes with it). */
function isAtOrBelow(path: ItemPath, removed: ItemPath): boolean {
  return path.length >= removed.length && removed.every((v, i) => path[i] === v)
}

/**
 * The actions a removal would delete: those pointing at the removed item or
 * anything inside it, except ones that live inside it (they go with it anyway).
 * Always empty for a move.
 */
export function findAffectedActions(config: DeviceConfig, change: StructureChange): AffectedAction[] {
  if (change.kind !== 'remove') return []
  return collectActionRefs(config)
    .filter((ref) => isAtOrBelow(ref.target, change.path) && !isAtOrBelow(ref.location, change.path))
    .map((ref) => ({
      button: ref.button.name?.trim() || (isPreset(config, ref.button) ? UNNAMED_PRESET : `Button ${ref.button.id + 1}`),
      location: isPreset(config, ref.button) ? 'Preset' : ref.location.length === 0 ? 'File' : nameAtPath(config, ref.location),
      action: ACTION_LABEL[ref.type],
      target: nameAtPath(config, ref.target),
    }))
}

/** Re-aims a bank's `defaultPatchIndex` after `change` touched its patches. */
function fixDefaultPatchIndexes(config: DeviceConfig, change: StructureChange, before: Map<object, number>) {
  config.modes.forEach((mode, m) =>
    mode.banks.forEach((bank, b) => {
      const was = before.get(bank)
      if (was === undefined) return
            const patchChange =
        change.kind === 'remove'
          ? change.path.length === 3 && change.path[0] === m && change.path[1] === b
          : change.parent.length === 2 && change.parent[0] === m && change.parent[1] === b
      if (!patchChange) return
      const next = mapPath([m, b, was], change)
      if (next === null) delete bank.defaultPatchIndex
      else if (next[2] === 0) delete bank.defaultPatchIndex
      else bank.defaultPatchIndex = next[2]
    }),
  )
}

/**
 * Applies `change` to `config` in place, keeping every Set action aimed at the
 * same item (rewriting its stored indices only where they would otherwise
 * change meaning) and, for a removal, deleting the actions that pointed at what
 * was removed — and any button left with no actions by that. Returns the actions
 * that were deleted.
 */
export function applyStructureChange(config: DeviceConfig, change: StructureChange): AffectedAction[] {
  const affected = findAffectedActions(config, change)
  const refs = collectActionRefs(config)

  // Each bank's defaultPatchIndex, captured before the lists move.
  const defaults = new Map<object, number>()
  for (const mode of config.modes) {
    for (const bank of mode.banks) if (bank.defaultPatchIndex !== undefined) defaults.set(bank, bank.defaultPatchIndex)
  }

  // --- the structural change itself ---
  const listFor = (parent: ItemPath): unknown[] =>
    parent.length === 0
      ? config.modes
      : parent.length === 1
        ? config.modes[parent[0]].banks
        : config.modes[parent[0]].banks[parent[1]].patches
  if (change.kind === 'remove') {
    listFor(change.path.slice(0, -1)).splice(change.path[change.path.length - 1], 1)
  } else {
    const list = listFor(change.parent)
    const [item] = list.splice(change.from, 1)
    list.splice(change.to, 0, item)
  }

  // --- re-aim or drop the actions ---
  const drop = new Map<ActionHost[], Set<Action>>()
  for (const ref of refs) {
    const location = mapPath(ref.location, change)
    if (location === null) continue // the button's container was removed — it goes with it
    const target = mapPath(ref.target, change)
    if (target === null) {
      const doomed = drop.get(ref.buttons) ?? new Set<Action>()
      doomed.add(ref.action)
      drop.set(ref.buttons, doomed)
      continue
    }
    const params = ref.action.actionParameters ?? {}
    const stillMeansTheSame =
      sameTuple(resolveTarget(ref.type, params, scopeOfLocation(location)), target)
    if (stillMeansTheSame) continue
    const ap = (ref.action.actionParameters ??= {}) as Record<string, unknown>
    for (const key of targetKeys(ref.type)) delete ap[key]
    Object.assign(ap, canonicalParams(ref.type, target, scopeOfLocation(location)))
  }

  for (const [buttons, doomed] of drop) {
    for (const button of [...buttons]) {
      const kept = button.actions.filter((a) => !doomed.has(a))
      if (kept.length === button.actions.length) continue
      // No cosmetic-only buttons. (A preset left empty is removed below instead,
      // so the actions that run it go too.)
      if (kept.length === 0 && !isPreset(config, button)) buttons.splice(buttons.indexOf(button), 1)
      else button.actions = kept
    }
  }

  // A preset left with no actions is meaningless: remove it, and the actions
  // that ran it (and any button that leaves empty).
  removeEmptyPresets(config)

  fixDefaultPatchIndexes(config, change, defaults)
  return affected
}

function isPreset(config: DeviceConfig, host: ActionHost): boolean {
  return (config.presets ?? []).includes(host as Preset)
}

function sameTuple(a: TargetTuple | null, b: TargetTuple): boolean {
  return a !== null && a.length === b.length && a.every((v, i) => v === b[i])
}
