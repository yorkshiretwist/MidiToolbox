// What a SetPatch / SetBank / SetMode action can point at, and how that is stored.
//
// A target is stored as the indices that say WHERE it is, from the innermost
// level outwards, and only as many as are needed:
//   SetPatch  { patchIndex }                          a patch of the bank the button is in
//             { bankIndex, patchIndex }               a patch of another bank, same mode
//             { modeIndex, bankIndex, patchIndex }    a patch of another mode
//   SetBank   { bankIndex }                           a bank of the mode the button is in
//             { modeIndex, bankIndex }                a bank of another mode
//   SetMode   { modeIndex }
// A Set action always lands on the default of whatever it doesn't name: a bank
// opens on its default patch, a mode on its first bank's default patch.
//
// How much a target must name depends on where the button lives (its scope), so
// that it always means exactly one item:
//   - a button on a Patch or Bank is in one known bank (and mode): a patch of
//     that bank needs just its index;
//   - a button on a Mode is in one known mode but any of its banks: a patch must
//     be chosen as Bank > Patch;
//   - a button on the File could be anywhere: a patch must be chosen as
//     Mode > Bank > Patch, and a bank as Mode > Bank.
// There is deliberately no "patch N of whichever bank is active" form: the
// editor never offers it, and the validator flags it. (The firmware still
// understands it, so hand-written configs keep running.)
// Config switching (SetConfig) has no deeper path.

import type { ActionParameters, DeviceConfig } from '../types/config'

export type IndexedTargetType = 'SetPatch' | 'SetBank' | 'SetMode'

/** Where a button lives: an undefined index means "any" (the button is shared across several). */
export interface TargetScope {
  modeIndex?: number
  bankIndex?: number
}

/** The target-describing slice of an action's parameters. */
export type TargetParams = Pick<ActionParameters, 'modeIndex' | 'bankIndex' | 'patchIndex'>

export interface TargetOption {
  /** Stable identifier for the <option>. */
  key: string
  /** The path shown, e.g. "Verse" or "Key: F > Verse" or "Pads > Key: F > Verse". */
  label: string
  /** The parameters that select it, exactly as stored in the config. */
  params: TargetParams
}

const PATH_SEPARATOR = ' > '

const KEYS: Record<IndexedTargetType, (keyof TargetParams)[]> = {
  SetPatch: ['modeIndex', 'bankIndex', 'patchIndex'],
  SetBank: ['modeIndex', 'bankIndex'],
  SetMode: ['modeIndex'],
}

export function isIndexedTargetType(type: string): type is IndexedTargetType {
  return type === 'SetPatch' || type === 'SetBank' || type === 'SetMode'
}

/** The parameters an action of this type stores its target in. */
export function targetKeys(type: IndexedTargetType): (keyof TargetParams)[] {
  return KEYS[type]
}

function nameOf(item: { name: string }, kind: string): string {
  return item.name.trim() || `(unnamed ${kind})`
}

function pick(params: TargetParams, type: IndexedTargetType): TargetParams {
  const out: TargetParams = {}
  for (const key of KEYS[type]) if (params[key] !== undefined) out[key] = params[key]
  return out
}

function sameParams(a: TargetParams, b: TargetParams, type: IndexedTargetType): boolean {
  return KEYS[type].every((key) => a[key] === b[key])
}

/**
 * The pick-list for a Set* action, in tree order: first the targets in the
 * button's own bank/mode (shortest path), then the rest, each labelled with its
 * path down from the first level that differs from where the button lives.
 */
export function targetOptions(type: IndexedTargetType, config: DeviceConfig, scope: TargetScope): TargetOption[] {
  const options: TargetOption[] = []

  if (type === 'SetMode') {
    config.modes.forEach((mode, m) =>
      options.push({ key: `${m}`, label: nameOf(mode, 'mode'), params: { modeIndex: m } }),
    )
    return options
  }

  const sameMode = (m: number) => scope.modeIndex != null && m === scope.modeIndex
  const sameBank = (m: number, b: number) => sameMode(m) && scope.bankIndex != null && b === scope.bankIndex

  config.modes.forEach((mode, m) => {
    mode.banks.forEach((bank, b) => {
      if (type === 'SetBank') {
        options.push(
          sameMode(m)
            ? { key: `${m}.${b}`, label: nameOf(bank, 'bank'), params: { bankIndex: b } }
            : {
                key: `${m}.${b}`,
                label: [nameOf(mode, 'mode'), nameOf(bank, 'bank')].join(PATH_SEPARATOR),
                params: { modeIndex: m, bankIndex: b },
              },
        )
        return
      }
      bank.patches.forEach((patch, p) => {
        const patchName = nameOf(patch, 'patch')
        if (sameBank(m, b)) {
          options.push({ key: `${m}.${b}.${p}`, label: patchName, params: { patchIndex: p } })
        } else if (sameMode(m)) {
          options.push({
            key: `${m}.${b}.${p}`,
            label: [nameOf(bank, 'bank'), patchName].join(PATH_SEPARATOR),
            params: { bankIndex: b, patchIndex: p },
          })
        } else {
          options.push({
            key: `${m}.${b}.${p}`,
            label: [nameOf(mode, 'mode'), nameOf(bank, 'bank'), patchName].join(PATH_SEPARATOR),
            params: { modeIndex: m, bankIndex: b, patchIndex: p },
          })
        }
      })
    })
  })

  // Targets in the button's own bank (or mode) come first.
  const rank = (o: TargetOption) => {
    const [m, b] = o.key.split('.').map(Number)
    return type === 'SetPatch' ? (sameBank(m, b) ? 0 : sameMode(m) ? 1 : 2) : sameMode(m) ? 0 : 1
  }
  options.sort((a, b) => rank(a) - rank(b))

  return options
}

/** The option an action's current parameters correspond to, if any. */
export function findOption(options: TargetOption[], type: IndexedTargetType, params: TargetParams): TargetOption | undefined {
  const wanted = pick(params, type)
  return options.find((o) => sameParams(o.params, wanted, type))
}

/**
 * Why a Set* action's target is not usable, or null if it is. `params` may use
 * any of the stored forms; a level it leaves out is taken from the button's
 * scope, and if the scope doesn't fix it the target is ambiguous.
 */
export function targetProblem(
  type: IndexedTargetType,
  params: TargetParams,
  config: DeviceConfig,
  scope: TargetScope,
): string | null {
  const target = resolveTarget(type, params, scope)
  if (!target) {
    if (type === 'SetMode') return 'must choose a mode'
    if (type === 'SetBank') return 'must choose a bank together with its mode (Mode > Bank)'
    return 'must choose a patch together with its bank' + (scope.modeIndex == null ? ' and mode (Mode > Bank > Patch)' : ' (Bank > Patch)')
  }
  const [m, b, p] = target
  const mode = config.modes[m]
  if (!mode) return `must point at an existing mode (got index ${m})`
  if (type === 'SetMode') return null
  const bank = mode.banks[b]
  if (!bank) return `must point at a bank that exists in that mode (got index ${b})`
  if (type === 'SetBank') return null
  return bank.patches[p] ? null : `must point at a patch that exists in that bank (got index ${p})`
}

/** A target as absolute indices: [mode], [mode, bank] or [mode, bank, patch], by action type. */
export type TargetTuple = readonly number[]

/** The stored parameters that point at `target` from a button in `scope` — the shortest form that still names it. */
export function canonicalParams(type: IndexedTargetType, target: TargetTuple, scope: TargetScope): TargetParams {
  const [m, b, p] = target
  if (type === 'SetMode') return { modeIndex: m }
  const sameMode = scope.modeIndex != null && m === scope.modeIndex
  if (type === 'SetBank') return sameMode ? { bankIndex: b } : { modeIndex: m, bankIndex: b }
  if (sameMode && scope.bankIndex != null && b === scope.bankIndex) return { patchIndex: p }
  return sameMode ? { bankIndex: b, patchIndex: p } : { modeIndex: m, bankIndex: b, patchIndex: p }
}

/**
 * The one target the stored parameters name for a button in `scope`, as absolute
 * indices — or null when a level is left out that the button's scope doesn't fix
 * (the ambiguous short form).
 */
export function resolveTarget(type: IndexedTargetType, params: TargetParams, scope: TargetScope): TargetTuple | null {
  const defined = (...values: (number | undefined)[]) => values.every((v) => v !== undefined)
  if (type === 'SetMode') return defined(params.modeIndex) ? [params.modeIndex!] : null
  if (type === 'SetBank') {
    const m = params.modeIndex ?? scope.modeIndex
    return defined(m, params.bankIndex) ? [m!, params.bankIndex!] : null
  }
  if (params.bankIndex !== undefined) {
    const m = params.modeIndex ?? scope.modeIndex
    return defined(m, params.patchIndex) ? [m!, params.bankIndex, params.patchIndex!] : null
  }
  return defined(scope.modeIndex, scope.bankIndex, params.patchIndex)
    ? [scope.modeIndex!, scope.bankIndex!, params.patchIndex!]
    : null
}

/** The scope of a button container path such as "/modes/0/banks/1/buttons" (as used by the validator). */
export function scopeOfPath(path: string): TargetScope {
  const mode = /^\/modes\/(\d+)/.exec(path)
  const bank = /^\/modes\/\d+\/banks\/(\d+)/.exec(path)
  return {
    modeIndex: mode ? Number(mode[1]) : undefined,
    bankIndex: bank ? Number(bank[1]) : undefined,
  }
}
