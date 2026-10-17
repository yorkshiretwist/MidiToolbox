// Tests for lib/selection.ts: parsing a selection path into levels and indices.
import { describe, expect, it } from 'vitest'
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
} from './selection'

describe('parseSelection', () => {
  it('reads a preset, a child of the file', () => {
    expect(parseSelection(presetPath(2))).toEqual({ level: 'preset', presetIndex: 2 })
  })

  it('reads the file level from an empty path', () => {
    expect(parseSelection(filePath())).toEqual({ level: 'file' })
  })

  it('reads mode / bank / patch levels', () => {
    expect(parseSelection(modePath(1))).toEqual({ level: 'mode', modeIndex: 1 })
    expect(parseSelection(bankPath(1, 2))).toEqual({
      level: 'bank',
      modeIndex: 1,
      bankIndex: 2,
    })
    expect(parseSelection(patchPath(1, 2, 3))).toEqual({
      level: 'patch',
      modeIndex: 1,
      bankIndex: 2,
      patchIndex: 3,
    })
  })

  it('reads a File-owned button', () => {
    expect(parseSelection(fileButtonPath(0))).toEqual({
      level: 'button',
      buttonOwnerLevel: 'file',
      buttonIndex: 0,
    })
  })

  it('reads a Mode-owned button', () => {
    expect(parseSelection(modeButtonPath(1, 2))).toEqual({
      level: 'button',
      buttonOwnerLevel: 'mode',
      modeIndex: 1,
      buttonIndex: 2,
    })
  })

  it('reads a Bank-owned button', () => {
    expect(parseSelection(bankButtonPath(1, 2, 3))).toEqual({
      level: 'button',
      buttonOwnerLevel: 'bank',
      modeIndex: 1,
      bankIndex: 2,
      buttonIndex: 3,
    })
  })

  it('reads a Patch-owned button', () => {
    expect(parseSelection(buttonPath(1, 2, 3, 4))).toEqual({
      level: 'button',
      buttonOwnerLevel: 'patch',
      modeIndex: 1,
      bankIndex: 2,
      patchIndex: 3,
      buttonIndex: 4,
    })
  })

  it('falls back to file level for a malformed path', () => {
    expect(parseSelection(['modes', 'not-a-number'])).toEqual({ level: 'file' })
    expect(parseSelection(['modes', 0, 'banks', 'nope'])).toEqual({
      level: 'mode',
      modeIndex: 0,
    })
  })
})
