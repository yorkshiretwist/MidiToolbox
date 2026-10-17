// The accordion pane stack mirrors one path into the config tree:
//   File > Mode > Bank > Patch > Button
// `store.selection` is that path as an array of keys (see SelectionPath). These
// helpers translate between the raw path and the level-plus-indices the panes
// actually need, and build the canonical path for each level.
//
// A Button can be defined directly on the File (root Config), Mode, Bank, or
// Patch — see the Patch > Bank > Mode > Config override order in
// MIDI-Toolbox/Runtime.h. A selection path reaching a button is therefore the
// container's own path plus a trailing `['buttons', i]`, e.g. a Bank-owned
// button is `['modes', m, 'banks', b, 'buttons', i]`. `buttonOwnerLevel` says
// which of those four containers the selected button actually lives in.

import type { SelectionPath } from '../types/config'

/** `'preset'` is a File-level child (File > Preset), not one of the four button-owning containers. */
export type PaneLevel = 'file' | 'mode' | 'bank' | 'patch' | 'button' | 'preset'

/** The four levels a Button can be defined directly on. */
export type ButtonOwnerLevel = 'file' | 'mode' | 'bank' | 'patch'

export interface ParsedSelection {
  level: PaneLevel
  modeIndex?: number
  bankIndex?: number
  patchIndex?: number
  /** Index into `config.presets`. Set iff level === 'preset'. */
  presetIndex?: number
  /** Index into the owning level's `buttons` array (not the physical button id). */
  buttonIndex?: number
  /** Which level's `buttons` array `buttonIndex` refers to. Set iff level === 'button'. */
  buttonOwnerLevel?: ButtonOwnerLevel
}

export function parseSelection(path: SelectionPath): ParsedSelection {
  if (path[0] === 'presets' && typeof path[1] === 'number') {
    return { level: 'preset', presetIndex: path[1] }
  }
  if (path[0] === 'buttons' && typeof path[1] === 'number') {
    return { level: 'button', buttonOwnerLevel: 'file', buttonIndex: path[1] }
  }
  if (path[0] !== 'modes' || typeof path[1] !== 'number') return { level: 'file' }
  const mi = path[1]

  if (path[2] === 'buttons' && typeof path[3] === 'number') {
    return { level: 'button', buttonOwnerLevel: 'mode', modeIndex: mi, buttonIndex: path[3] }
  }
  if (path[2] !== 'banks' || typeof path[3] !== 'number')
    return { level: 'mode', modeIndex: mi }
  const bi = path[3]

  if (path[4] === 'buttons' && typeof path[5] === 'number') {
    return {
      level: 'button',
      buttonOwnerLevel: 'bank',
      modeIndex: mi,
      bankIndex: bi,
      buttonIndex: path[5],
    }
  }
  if (path[4] !== 'patches' || typeof path[5] !== 'number')
    return { level: 'bank', modeIndex: mi, bankIndex: bi }
  const pi = path[5]

  if (path[6] !== 'buttons' || typeof path[7] !== 'number')
    return { level: 'patch', modeIndex: mi, bankIndex: bi, patchIndex: pi }
  return {
    level: 'button',
    buttonOwnerLevel: 'patch',
    modeIndex: mi,
    bankIndex: bi,
    patchIndex: pi,
    buttonIndex: path[7],
  }
}

export const filePath = (): SelectionPath => []
export const presetPath = (i: number): SelectionPath => ['presets', i]
export const fileButtonPath = (i: number): SelectionPath => ['buttons', i]
export const modePath = (m: number): SelectionPath => ['modes', m]
export const modeButtonPath = (m: number, i: number): SelectionPath => [
  'modes',
  m,
  'buttons',
  i,
]
export const bankPath = (m: number, b: number): SelectionPath => [
  'modes',
  m,
  'banks',
  b,
]
export const bankButtonPath = (m: number, b: number, i: number): SelectionPath => [
  'modes',
  m,
  'banks',
  b,
  'buttons',
  i,
]
export const patchPath = (m: number, b: number, p: number): SelectionPath => [
  'modes',
  m,
  'banks',
  b,
  'patches',
  p,
]
export const buttonPath = (
  m: number,
  b: number,
  p: number,
  i: number,
): SelectionPath => ['modes', m, 'banks', b, 'patches', p, 'buttons', i]
