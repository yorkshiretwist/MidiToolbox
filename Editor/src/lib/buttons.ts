// Helpers for mapping the 13 physical footswitches onto a patch's `buttons`
// array, for resolving a button through the Patch > Bank > Mode > Config
// override cascade, and for cleaning the config on the way out.

import type { Bank, Button, DeviceConfig, Rgb } from '../types/config'
import type { DeviceFile, NavRole } from '../types/device'
import { type ButtonOwnerLevel, type ParsedSelection } from './selection'

/** page 0 means "every page" (see schema). Otherwise default is page 1. */
export function buttonOnPage(button: Button, page: number): boolean {
  const p = button.page ?? 1
  return p === 0 || p === page
}

/** Index of the button with this physical id on the given page, or -1. */
export function findButtonIndexById(
  buttons: Button[],
  id: number,
  page: number,
): number {
  return buttons.findIndex((b) => b.id === id && buttonOnPage(b, page))
}

/**
 * Within a patch, (id, page) is a composite key. Returns another button that
 * would clash if `buttons[movingIndex]` were placed on `targetPage` — same id,
 * and a page that overlaps (equal, or either side is 0 = "every page") — or
 * undefined if the move is free.
 */
export function findPageConflict(
  buttons: Button[],
  movingIndex: number,
  targetPage: number,
): Button | undefined {
  const moving = buttons[movingIndex]
  if (!moving) return undefined
  return buttons.find((b, i) => {
    if (i === movingIndex || b.id !== moving.id) return false
    const bp = b.page ?? 1
    return targetPage === 0 || bp === 0 || bp === targetPage
  })
}

/**
 * After two buttons in a patch trade physical ids, walk every Exclusive
 * `buttonSet` in that patch and swap `idA` ⟷ `idB` so each set keeps naming the
 * same buttons. Sets are re-sorted (the button-set editor stores them sorted).
 * A no-op when `idA === idB` (a pure page swap changes no id).
 */
export function remapButtonSetIds(
  buttons: Button[],
  idA: number,
  idB: number,
): void {
  if (idA === idB) return
  for (const button of buttons) {
    const set = button.buttonSet
    if (!set || !set.some((id) => id === idA || id === idB)) continue
    button.buttonSet = set
      .map((id) => (id === idA ? idB : id === idB ? idA : id))
      .sort((x, y) => x - y)
  }
}

/** Cascade priority, highest first — mirrors resolveButtonContainer() in MIDI-Toolbox/Runtime.h. */
export const BUTTON_LEVEL_CASCADE: ButtonOwnerLevel[] = ['patch', 'bank', 'mode', 'file']

/**
 * `'builtin'` is not a real, navigable File/Mode/Bank/Patch level — it marks a
 * `ResolvedButton` that fell all the way through the cascade to the built-in
 * navigation default (see `builtInNavButtons` below), the same lowest-
 * priority fallback `resolveButtonContainer()` applies in
 * MIDI-Toolbox/Runtime.h below the root Config. Kept separate from
 * `ButtonOwnerLevel` (which the accordion/selection code assumes is always one
 * of the four real containers) rather than added to it.
 */
export type ResolvedButtonLevel = ButtonOwnerLevel | 'builtin'

export const BUTTON_LEVEL_LABEL: Record<ResolvedButtonLevel, string> = {
  file: 'File',
  mode: 'Mode',
  bank: 'Bank',
  patch: 'Patch',
  builtin: 'Built-in',
}

/**
 * The `buttons` array a given level owns directly, given the Mode/Bank/Patch
 * already fixed by `at` (as much of it as that level needs — a Bank lookup
 * only needs `at.modeIndex`/`at.bankIndex`, for instance). Returns undefined
 * when that level isn't reachable from `at` (e.g. asking for the Bank level
 * while no bank is selected) — not to be confused with a level that has no
 * buttons of its own yet, which reads back as an empty/absent array.
 */
export function buttonsContainerAt(
  config: DeviceConfig,
  level: ButtonOwnerLevel,
  at: Pick<ParsedSelection, 'modeIndex' | 'bankIndex' | 'patchIndex'>,
): Button[] | undefined {
  if (level === 'file') return config.buttons
  if (at.modeIndex == null) return undefined
  const mode = config.modes[at.modeIndex]
  if (!mode) return undefined
  if (level === 'mode') return mode.buttons
  if (at.bankIndex == null) return undefined
  const bank = mode.banks[at.bankIndex]
  if (!bank) return undefined
  if (level === 'bank') return bank.buttons
  if (at.patchIndex == null) return undefined
  return bank.patches[at.patchIndex]?.buttons
}

export interface ResolvedButton {
  level: ResolvedButtonLevel
  button: Button
  /** Index within that level's own `buttons` array. -1 for a `'builtin'` result, which owns no array slot. */
  index: number
}

/**
 * Resolves the effective Button for physical `id`, starting the search at
 * `fromLevel` and walking up through Patch > Bank > Mode > Config, then
 * falling back to `device`'s built-in navigation default (see
 * `builtInNavButtons` below) for whichever three ids it names — the same
 * override order, built-in fallback included, that resolveButtonContainer()
 * applies in MIDI-Toolbox/Runtime.h. `page` only filters the Patch level;
 * Bank/Mode/Config buttons always apply regardless of page. Returns undefined
 * only for an id nothing defines and that has no built-in role either (i.e.
 * any id other than the device's three navigation ids, when unconfigured
 * everywhere).
 */
export function resolveButtonFrom(
  config: DeviceConfig,
  device: DeviceFile,
  fromLevel: ButtonOwnerLevel,
  at: Pick<ParsedSelection, 'modeIndex' | 'bankIndex' | 'patchIndex'>,
  id: number,
  page: number,
): ResolvedButton | undefined {
  const start = BUTTON_LEVEL_CASCADE.indexOf(fromLevel)
  for (let i = start; i < BUTTON_LEVEL_CASCADE.length; i++) {
    const level = BUTTON_LEVEL_CASCADE[i]
    const buttons = buttonsContainerAt(config, level, at)
    if (!buttons || buttons.length === 0) continue
    const index =
      level === 'patch'
        ? findButtonIndexById(buttons, id, page)
        : buttons.findIndex((b) => b.id === id)
    if (index !== -1) return { level, button: buttons[index], index }
  }
  const builtIn = builtInNavButtons(device)[id]
  return builtIn ? { level: 'builtin', button: builtIn, index: -1 } : undefined
}

/**
 * The buttons `fromLevel` inherits from a less-specific level (Bank/Mode/
 * Config) that it has not itself overridden — one entry per physical id,
 * already resolved to whichever ancestor level actually supplies it (e.g. a
 * Mode button shadows a Config button with the same id). Used to show a
 * Mode/Bank/Patch pane's "inherited, not yet overridden here" section.
 *
 * Deliberately does NOT include the device's built-in navigation defaults
 * (see builtInNavButtons below) when nothing overrides them - they need no
 * configuration to work, so listing them here would clutter this "still
 * needs your attention" section with entries that never do.
 * `resolveButtonFrom()` still falls back to them (the board/screen need to
 * know what's actually running); only a REAL override at some level - a
 * config author's own Button bound to one of these ids - shows up here, the
 * same as for any other id.
 *
 * `page` only matters when `fromLevel` is 'patch': unlike Bank/Mode/Config
 * buttons, a Patch button only overrides on the page(s) it's actually
 * defined on (page 0 = every page), so whether an id counts as "already
 * overridden" is itself a per-page question — see buttonOnPage(). Ignored
 * for every other level, which are never page-filtered.
 */
export function inheritedButtons(
  config: DeviceConfig,
  fromLevel: ButtonOwnerLevel,
  at: Pick<ParsedSelection, 'modeIndex' | 'bankIndex' | 'patchIndex'>,
  page = 1,
): ResolvedButton[] {
  const start = BUTTON_LEVEL_CASCADE.indexOf(fromLevel)
  const ownAll = buttonsContainerAt(config, fromLevel, at) ?? []
  const own = fromLevel === 'patch' ? ownAll.filter((b) => buttonOnPage(b, page)) : ownAll
  const claimed = new Set(own.map((b) => b.id))
  const result: ResolvedButton[] = []
  for (let i = start + 1; i < BUTTON_LEVEL_CASCADE.length; i++) {
    const level = BUTTON_LEVEL_CASCADE[i]
    const buttons = buttonsContainerAt(config, level, at) ?? []
    buttons.forEach((button, index) => {
      if (claimed.has(button.id)) return
      claimed.add(button.id)
      result.push({ level, button, index })
    })
  }
  return result.sort((a, z) => a.button.id - z.button.id)
}

export function rgbCss(rgb: number[] | undefined, fallback = '#333333'): string {
  if (!rgb || rgb.length < 3) return fallback
  const [r, g, b] = rgb
  return `rgb(${r}, ${g}, ${b})`
}

/**
 * The colour a button's LED / screen cell actually draws: its own
 * `colourCode`, falling back to the containing bank's `subheaderColourCode`
 * (see the "Defaults to the selected bank's subheaderColourCode" comment on
 * `colourCode` in config/schema.json) when the button doesn't specify one.
 */
export function effectiveColourCode(
  button: Button | undefined,
  bank: Bank | undefined,
): Rgb | undefined {
  return button?.colourCode ?? bank?.subheaderColourCode
}

/** A button with no actions is cosmetic-only; it must not reach the JSON. */
export function isCosmeticButton(button: Button): boolean {
  return !button.actions || button.actions.length === 0
}

/**
 * Deep-copy `config` with every cosmetic-only button removed (and an empty
 * `presets` list dropped), so a footswitch
 * that was selected but never given an action leaves no trace on save.
 */
export function pruneCosmeticButtons(config: DeviceConfig): DeviceConfig {
  // JSON round-trip rather than structuredClone: the config is pure JSON and
  // the live object is a Vue reactive proxy, which structuredClone rejects.
  const copy: DeviceConfig = JSON.parse(JSON.stringify(config))
  const strip = (host: { buttons?: Button[] }) => {
    if (host.buttons) host.buttons = host.buttons.filter((b) => !isCosmeticButton(b))
  }
  strip(copy)
  // An empty presets list adds nothing to the file.
  if (copy.presets?.length === 0) delete copy.presets
  for (const mode of copy.modes) {
    strip(mode)
    for (const bank of mode.banks) {
      strip(bank)
      for (const patch of bank.patches) strip(patch)
    }
  }
  return copy
}

const NAV_ROLES: NavRole[] = ['select', 'down', 'up']

/**
 * The three navigation roles' Button bodies — same names/actions regardless
 * of device, only which physical id each is keyed to varies (device.navigation).
 * Mirrors the CycleActivateParameterAction / CycleParameterValueAction
 * handling in MIDI-Toolbox/Navigation.h and builtInNavButtons() in
 * MIDI-Toolbox/Runtime.h, which reads the same three definitions out of
 * MidiToolboxDevice.h's NAV_SELECT_ID/NAV_DOWN_ID/NAV_UP_ID.
 */
const NAV_ROLE_BUTTON: Record<NavRole, Omit<Button, 'id'>> = {
  select: {
    name: 'Cycle/Activate Parameter',
    buttonMode: 'Momentary',
    actions: [{ type: 'CycleActivateParameter' }],
  },
  down: {
    name: 'Cycle Parameter Down',
    buttonMode: 'Momentary',
    actions: [{ type: 'CycleParameterValue', actionParameters: { direction: 'Down' } }],
  },
  up: {
    name: 'Cycle Parameter Up',
    buttonMode: 'Momentary',
    actions: [{ type: 'CycleParameterValue', actionParameters: { direction: 'Up' } }],
  },
}

const NAV_ROLE_DESCRIPTION: Record<NavRole, string> = {
  select: 'cycles / activates the current parameter',
  down: 'cycles the parameter value down',
  up: 'cycles the parameter value up',
}

/** The short label physically printed under/on a device's navigation footswitches. */
const NAV_ROLE_LABEL: Record<NavRole, string> = {
  select: 'Param',
  down: '<',
  up: '>',
}

/**
 * Built-in default Button for each of `device`'s three navigation
 * footswitches, keyed by physical id (device.navigation.select/down/up) —
 * this is the single source of truth `resolveButtonFrom()` falls back to
 * above; do not hand-roll another copy of these three definitions elsewhere.
 *
 * Overriding any of them at Patch/Bank/Mode/File level replaces the physical
 * footswitch's behaviour only — the on-screen parameter row on the device
 * always keeps this built-in behaviour regardless (see
 * docs/navigation.md), which is why the confirmation
 * dialog for these three ids (see confirmOverride() in stores/config.ts)
 * can say so rather than warning that navigation is lost outright.
 */
export function builtInNavButtons(device: DeviceFile): Record<number, Button> {
  const result: Record<number, Button> = {}
  for (const role of NAV_ROLES) {
    const id = device.navigation[role]
    result[id] = { id, ...NAV_ROLE_BUTTON[role] }
  }
  return result
}

function navRoleForId(device: DeviceFile, id: number): NavRole | undefined {
  return NAV_ROLES.find((role) => device.navigation[role] === id)
}

/** A short, human description of what a navigation id's built-in Button does on `device` — used in warnings/confirmations, not for display of a config's own override. */
export function navButtonDefaultRole(device: DeviceFile, id: number): string | undefined {
  const role = navRoleForId(device, id)
  return role ? NAV_ROLE_DESCRIPTION[role] : undefined
}

/**
 * The short label physically printed under/on `device`'s navigation
 * footswitches — distinct from builtInNavButtons()'s own `name` field (which
 * is a full, editable action name, shown once a config author overrides one
 * and starts customising it). While a nav id is still at its built-in
 * default, the board shows this terse label instead of that full name,
 * matching how the physical hardware is printed. Returns undefined for any
 * other id.
 */
export function navButtonDefaultLabel(device: DeviceFile, id: number): string | undefined {
  const role = navRoleForId(device, id)
  return role ? NAV_ROLE_LABEL[role] : undefined
}

// The board/screen grid slots (which column/row each physical button sits
// in) used to live here as fixed constants matching only the reference
// 13-button device. They're now derived from the active device profile's own
// xMm/yMm positions instead — see deviceBoardLayout()/deviceScreenLayout()
// in lib/deviceLayout.ts.
