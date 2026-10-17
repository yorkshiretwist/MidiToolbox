// Tests for which patches/banks/modes a Set* action may target, how a target is stored, and the validator check built on it.
import { describe, expect, it } from 'vitest'
import { findOption, scopeOfPath, targetOptions, targetProblem } from './actionTargets'
import { validateConfig } from './validate'
import { liveRigDevice } from '../data/liveRigDevice'
import type { DeviceConfig } from '../types/config'

// M1 "Pads" has two banks (3 patches, 2 patches); M2 "FX" has one bank (4 patches).
const config: DeviceConfig = {
  modes: [
    {
      name: 'Pads',
      banks: [
        { name: 'Keys', patches: [{ name: 'Intro', buttons: [] }, { name: 'Verse', buttons: [] }, { name: 'Chorus', buttons: [] }] },
        { name: 'Strings', patches: [{ name: 'Solo', buttons: [] }, { name: 'Outro', buttons: [] }] },
      ],
    },
    { name: 'FX', banks: [{ name: 'Drive', patches: ['a', 'b', 'c', 'd'].map((name) => ({ name, buttons: [] })) }] },
  ],
}

const labels = (options: { label: string }[]) => options.map((o) => o.label)

describe('SetPatch targets', () => {
  it('for a Patch/Bank-level button: own bank by plain name, then other banks and modes by path', () => {
    const options = targetOptions('SetPatch', config, { modeIndex: 0, bankIndex: 1 })
    expect(labels(options)).toEqual([
      'Solo',
      'Outro',
      'Keys > Intro',
      'Keys > Verse',
      'Keys > Chorus',
      'FX > Drive > a',
      'FX > Drive > b',
      'FX > Drive > c',
      'FX > Drive > d',
    ])
  })

  it('stores only as many indices as the path needs', () => {
    const options = targetOptions('SetPatch', config, { modeIndex: 0, bankIndex: 1 })
    expect(options[0].params).toEqual({ patchIndex: 0 }) // sibling
    expect(options[2].params).toEqual({ bankIndex: 0, patchIndex: 0 }) // other bank, same mode
    expect(options[5].params).toEqual({ modeIndex: 1, bankIndex: 0, patchIndex: 0 }) // other mode
  })

  it('for a Mode-level button: every patch is chosen as Bank > Patch (or Mode > Bank > Patch in another mode) - never by number alone', () => {
    const options = targetOptions('SetPatch', config, { modeIndex: 0 })
    expect(labels(options).slice(0, 2)).toEqual(['Keys > Intro', 'Keys > Verse'])
    expect(options.find((o) => o.label === 'FX > Drive > a')?.params).toEqual({ modeIndex: 1, bankIndex: 0, patchIndex: 0 })
    // no short, "whichever bank is active" entries
    expect(options.every((o) => o.params.bankIndex !== undefined)).toBe(true)
  })

  it('for a File-level button every path is complete', () => {
    const options = targetOptions('SetPatch', config, {})
    expect(options[0].label).toBe('Pads > Keys > Intro')
    expect(options[0].params).toEqual({ modeIndex: 0, bankIndex: 0, patchIndex: 0 })
  })
})

describe('options are ordered by proximity, with the short name for anything in the same bank / mode', () => {
  // Mode 1: Bank 1 (Patch A, Patch B), Bank 2 (Patch E); Mode 2: Bank 3 (Patch J).
  const near: DeviceConfig = {
    modes: [
      {
        name: 'Mode 1',
        banks: [
          { name: 'Bank 1', patches: [{ name: 'Patch A', buttons: [] }, { name: 'Patch B', buttons: [] }] },
          { name: 'Bank 2', patches: [{ name: 'Patch E', buttons: [] }] },
        ],
      },
      { name: 'Mode 2', banks: [{ name: 'Bank 3', patches: [{ name: 'Patch J', buttons: [] }] }] },
    ],
  }

  it('Set patch from a button in a patch: same bank by name, then other bank in the mode, then other modes', () => {
    const options = targetOptions('SetPatch', near, { modeIndex: 0, bankIndex: 0 })
    expect(labels(options)).toEqual(['Patch A', 'Patch B', 'Bank 2 > Patch E', 'Mode 2 > Bank 3 > Patch J'])
    expect(options.map((o) => o.params)).toEqual([
      { patchIndex: 0 },
      { patchIndex: 1 },
      { bankIndex: 1, patchIndex: 0 },
      { modeIndex: 1, bankIndex: 0, patchIndex: 0 },
    ])
  })

  it('Set bank from a button in a bank: banks of the same mode by name, then other modes by path', () => {
    const options = targetOptions('SetBank', near, { modeIndex: 0, bankIndex: 1 })
    expect(labels(options)).toEqual(['Bank 1', 'Bank 2', 'Mode 2 > Bank 3'])
    expect(options.map((o) => o.params)).toEqual([{ bankIndex: 0 }, { bankIndex: 1 }, { modeIndex: 1, bankIndex: 0 }])
  })

  it('Set bank from a button in a mode: the banks of that mode by name too', () => {
    expect(labels(targetOptions('SetBank', near, { modeIndex: 1 }))).toEqual(['Bank 3', 'Mode 1 > Bank 1', 'Mode 1 > Bank 2'])
  })
})

describe('SetBank and SetMode targets', () => {
  it('lists banks of the same mode plainly and banks of other modes with their mode', () => {
    const options = targetOptions('SetBank', config, { modeIndex: 0, bankIndex: 0 })
    expect(labels(options)).toEqual(['Keys', 'Strings', 'FX > Drive'])
    expect(options[2].params).toEqual({ modeIndex: 1, bankIndex: 0 })
    expect(options[1].params).toEqual({ bankIndex: 1 })
  })

  it('never includes a patch level — a bank always opens on its default patch', () => {
    for (const o of targetOptions('SetBank', config, {})) expect(o.params.patchIndex).toBeUndefined()
  })

  it('lists every mode by name', () => {
    const options = targetOptions('SetMode', config, { modeIndex: 0 })
    expect(labels(options)).toEqual(['Pads', 'FX'])
    expect(options[1].params).toEqual({ modeIndex: 1 })
  })
})

describe('matching stored parameters back to an option', () => {
  const options = targetOptions('SetPatch', config, { modeIndex: 0, bankIndex: 1 })

  it('finds the option for the stored form', () => {
    expect(findOption(options, 'SetPatch', { patchIndex: 1 })?.label).toBe('Outro')
    expect(findOption(options, 'SetPatch', { modeIndex: 1, bankIndex: 0, patchIndex: 2 })?.label).toBe('FX > Drive > c')
  })

  it('finds nothing for a target that no longer exists', () => {
    expect(findOption(options, 'SetPatch', { patchIndex: 9 })).toBeUndefined()
    expect(findOption(options, 'SetPatch', {})).toBeUndefined()
  })
})

describe('target problems', () => {
  const patchLevel = { modeIndex: 0, bankIndex: 1 }

  it('accepts every form that resolves', () => {
    expect(targetProblem('SetPatch', { patchIndex: 1 }, config, patchLevel)).toBeNull()
    expect(targetProblem('SetPatch', { bankIndex: 0, patchIndex: 2 }, config, patchLevel)).toBeNull()
    expect(targetProblem('SetPatch', { modeIndex: 1, bankIndex: 0, patchIndex: 3 }, config, patchLevel)).toBeNull()
    expect(targetProblem('SetBank', { modeIndex: 1, bankIndex: 0 }, config, patchLevel)).toBeNull()
    expect(targetProblem('SetMode', { modeIndex: 1 }, config, patchLevel)).toBeNull()
  })

  it('rejects a target past the end at any level', () => {
    expect(targetProblem('SetPatch', { patchIndex: 2 }, config, patchLevel)).not.toBeNull() // Strings has 2
    expect(targetProblem('SetPatch', { bankIndex: 0, patchIndex: 3 }, config, patchLevel)).not.toBeNull()
    expect(targetProblem('SetPatch', { modeIndex: 1, bankIndex: 1, patchIndex: 0 }, config, patchLevel)).not.toBeNull()
    expect(targetProblem('SetBank', { modeIndex: 1, bankIndex: 1 }, config, patchLevel)).not.toBeNull()
    expect(targetProblem('SetMode', { modeIndex: 2 }, config, patchLevel)).not.toBeNull()
  })

  it('rejects negative indices', () => {
    expect(targetProblem('SetPatch', { patchIndex: -1 }, config, patchLevel)).not.toBeNull()
    expect(targetProblem('SetMode', { modeIndex: -1 }, config, patchLevel)).not.toBeNull()
  })

  it('rejects the short "whichever bank is active" form on a button that is not in one bank', () => {
    expect(targetProblem('SetPatch', { patchIndex: 1 }, config, { modeIndex: 0 })).not.toBeNull() // Mode-level: bank unknown
    expect(targetProblem('SetPatch', { patchIndex: 1 }, config, {})).not.toBeNull() // File-level
    expect(targetProblem('SetBank', { bankIndex: 0 }, config, {})).not.toBeNull() // File-level: mode unknown
    expect(targetProblem('SetBank', { bankIndex: 0 }, config, { modeIndex: 0 })).toBeNull() // mode is known
  })

  it('accepts a Mode-level patch chosen as Bank > Patch', () => {
    expect(targetProblem('SetPatch', { bankIndex: 1, patchIndex: 1 }, config, { modeIndex: 0 })).toBeNull()
  })

  it('needs a File-level patch with a bank to name its mode', () => {
    expect(targetProblem('SetPatch', { bankIndex: 0, patchIndex: 0 }, config, {})).not.toBeNull()
  })

  it('reads the scope from a button container path', () => {
    expect(scopeOfPath('/buttons')).toEqual({ modeIndex: undefined, bankIndex: undefined })
    expect(scopeOfPath('/modes/1/buttons')).toEqual({ modeIndex: 1, bankIndex: undefined })
    expect(scopeOfPath('/modes/0/banks/2/patches/3/buttons')).toEqual({ modeIndex: 0, bankIndex: 2 })
  })
})

function withAction(type: string, actionParameters: Record<string, number>): DeviceConfig {
  const c = structuredClone(config)
  c.modes[0].banks[1].patches[0].buttons = [
    { id: 0, name: 'Go', buttonMode: 'Momentary', actions: [{ type, actionParameters }] } as never,
  ]
  return c
}

describe('target validation in the whole config', () => {
  const device = liveRigDevice

  it('accepts an existing target in any form', () => {
    expect(validateConfig(withAction('SetPatch', { patchIndex: 1 }), device).valid).toBe(true)
    expect(validateConfig(withAction('SetPatch', { modeIndex: 1, bankIndex: 0, patchIndex: 3 }), device).valid).toBe(true)
    expect(validateConfig(withAction('SetBank', { modeIndex: 1, bankIndex: 0 }), device).valid).toBe(true)
  })

  it('flags a target past the end of the bank', () => {
    const result = validateConfig(withAction('SetPatch', { patchIndex: 2 }), device)
    expect(result.issues.map((i) => i.keyword)).toContain('actionTarget')
    expect(result.issues[0].path).toBe('/modes/0/banks/1/patches/0/buttons/0/actions/0/actionParameters/patchIndex')
  })

  it('flags a deep target that no longer exists', () => {
    expect(validateConfig(withAction('SetPatch', { modeIndex: 1, bankIndex: 0, patchIndex: 4 }), device).valid).toBe(false)
  })

  it('rejects a negative index', () => {
    expect(validateConfig(withAction('SetBank', { bankIndex: -1 }), device).valid).toBe(false)
  })

  it('flags the ambiguous short form on a File-level button', () => {
    const c = structuredClone(config)
    c.buttons = [{ id: 0, name: 'Go', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 0 } }] } as never]
    const result = validateConfig(c, device)
    expect(result.issues.map((i) => i.keyword)).toContain('actionTarget')
    expect(result.issues[0].path).toBe('/buttons/0/actions/0/actionParameters/patchIndex')
  })

  it('flags a stale mode target', () => {
    expect(validateConfig(withAction('SetMode', { modeIndex: 5 }), device).valid).toBe(false)
  })
})
