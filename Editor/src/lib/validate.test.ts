// Tests for lib/validate.ts: schema and device-aware config validation.
import { describe, expect, it } from 'vitest'
import { validateConfig } from './validate'
import { liveRigDevice } from '../data/liveRigDevice'
import type { DeviceConfig } from '../types/config'

const minimalValid: DeviceConfig = {
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

describe('validateConfig', () => {
  it('accepts a minimal valid config', () => {
    const result = validateConfig(minimalValid, liveRigDevice)
    expect(result.valid).toBe(true)
    expect(result.issues).toHaveLength(0)
  })

  it('rejects a config with no modes and reports the missing property', () => {
    const result = validateConfig({}, liveRigDevice)
    expect(result.valid).toBe(false)
    expect(result.issues.some((i) => i.keyword === 'required')).toBe(true)
  })

  it('flags a bank left with no patches (the "delete last patch" state)', () => {
    const bad = structuredClone(minimalValid)
    bad.modes[0].banks[0].patches = []
    const result = validateConfig(bad, liveRigDevice)
    expect(result.valid).toBe(false)
    expect(result.byPath.has('/modes/0/banks/0/patches')).toBe(true)
    expect(
      result.byPath.get('/modes/0/banks/0/patches')?.some((i) => i.keyword === 'minItems'),
    ).toBe(true)
  })

  it('flags a structurally out-of-range button id (ajv)', () => {
    const bad = structuredClone(minimalValid)
    bad.modes[0].banks[0].patches[0].buttons.push({
      id: 999,
      name: 'X',
      buttonMode: 'Momentary',
      actions: [],
    })
    const result = validateConfig(bad, liveRigDevice)
    expect(result.valid).toBe(false)
    const path = '/modes/0/banks/0/patches/0/buttons/0/id'
    expect(result.byPath.has(path)).toBe(true)
    expect(result.byPath.get(path)?.some((i) => i.keyword === 'maximum')).toBe(true)
  })

  it('flags a button id that is structurally in range but does not exist on the active device', () => {
    const bad = structuredClone(minimalValid)
    bad.modes[0].banks[0].patches[0].buttons.push({
      id: 50,
      name: 'X',
      buttonMode: 'Momentary',
      actions: [],
    })
    const result = validateConfig(bad, liveRigDevice)
    expect(result.valid).toBe(false)
    const path = '/modes/0/banks/0/patches/0/buttons/0/id'
    expect(result.byPath.get(path)?.some((i) => i.keyword === 'deviceButtonId')).toBe(true)
  })

  it('does not flag an id that exists on a different (larger) device', () => {
    const bad = structuredClone(minimalValid)
    bad.modes[0].banks[0].patches[0].buttons.push({
      id: 20,
      name: 'X',
      buttonMode: 'Momentary',
      actions: [],
    })
    const biggerDevice = {
      ...liveRigDevice,
      buttons: [...liveRigDevice.buttons, { id: 20, xMm: 0, yMm: 0, led: 'above' as const }],
    }
    const result = validateConfig(bad, biggerDevice)
    expect(result.valid).toBe(true)
  })

  it('flags an Exclusive buttonSet entry that does not exist on the device', () => {
    const bad = structuredClone(minimalValid)
    bad.modes[0].banks[0].patches[0].buttons.push({
      id: 0,
      name: 'X',
      buttonMode: 'Exclusive',
      buttonSet: [0, 50],
      actions: [],
    })
    const result = validateConfig(bad, liveRigDevice)
    expect(result.valid).toBe(false)
    const path = '/modes/0/banks/0/patches/0/buttons/0/buttonSet/1'
    expect(result.byPath.get(path)?.some((i) => i.keyword === 'deviceButtonId')).toBe(true)
  })
})

describe('presets', () => {
  const withPresets = (): DeviceConfig => ({
    ...structuredClone(minimalValid),
    presets: [
      { id: 0, name: 'Clean', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 3 } }] },
      { id: 1, name: 'Lead', actions: [{ type: 'ControlChange', actionParameters: { controllerNumber: 7 } }] },
    ],
  })
  const runs = (presetId?: number) => ({
    id: 0,
    name: 'Go',
    buttonMode: 'Momentary' as const,
    actions: [{ type: 'ExecutePreset' as const, actionParameters: presetId === undefined ? {} : { presetId } }],
  })

  it('accepts a button that runs an existing preset', () => {
    const c = withPresets()
    c.modes[0].banks[0].patches[0].buttons.push(runs(1))
    expect(validateConfig(c, liveRigDevice).valid).toBe(true)
  })

  it('flags an ExecutePreset naming a preset that does not exist', () => {
    const c = withPresets()
    c.modes[0].banks[0].patches[0].buttons.push(runs(42))
    const result = validateConfig(c, liveRigDevice)
    expect(result.valid).toBe(false)
    expect(result.byPath.has('/modes/0/banks/0/patches/0/buttons/0/actions/0/actionParameters/presetId')).toBe(true)
  })

  it('flags an ExecutePreset with no preset chosen', () => {
    const c = withPresets()
    c.buttons = [runs()]
    expect(validateConfig(c, liveRigDevice).byPath.has('/buttons/0/actions/0/actionParameters/presetId')).toBe(true)
  })

  it('flags duplicate preset ids', () => {
    const c = withPresets()
    c.presets![1].id = 0
    expect(validateConfig(c, liveRigDevice).byPath.has('/presets/1/id')).toBe(true)
  })

  it('requires a preset to have at least one action', () => {
    const c = withPresets()
    c.presets![0].actions = []
    expect(validateConfig(c, liveRigDevice).byPath.has('/presets/0/actions')).toBe(true)
  })

  it('does not allow a preset to run another preset', () => {
    const c = withPresets()
    c.presets![0].actions.push({ type: 'ExecutePreset', actionParameters: { presetId: 1 } })
    expect(validateConfig(c, liveRigDevice).valid).toBe(false)
  })

  it('needs a fully-qualified target for Set patch inside a preset', () => {
    const c = withPresets()
    c.presets![0].actions = [{ type: 'SetPatch', actionParameters: { patchIndex: 0 } }]
    expect(validateConfig(c, liveRigDevice).byPath.has('/presets/0/actions/0/actionParameters/patchIndex')).toBe(true)
    c.presets![0].actions = [{ type: 'SetPatch', actionParameters: { modeIndex: 0, bankIndex: 0, patchIndex: 0 } }]
    expect(validateConfig(c, liveRigDevice).valid).toBe(true)
  })
})

describe('MIDI value ranges', () => {
  // one button on the patch with a single action of the given type and parameters
  function withAction(type: string, actionParameters: object): DeviceConfig {
    const config = structuredClone(minimalValid)
    config.modes[0].banks[0].patches[0].buttons = [
      { id: 0, name: 'b', buttonMode: 'Momentary', actions: [{ type, actionParameters } as never] },
    ]
    return config
  }

  const ranged: [string, string][] = [
    ['PlayNotes', 'velocity'],
    ['ProgramChange', 'programNumber'],
    ['ControlChange', 'controllerNumber'],
    ['ControlChange', 'controlValue'],
    ['ControlChange', 'releaseValue'],
  ]

  it.each(ranged)('accepts the whole 0-127 range for %s %s', (type, key) => {
    for (const value of [0, 1, 64, 127]) {
      expect(validateConfig(withAction(type, { [key]: value }), liveRigDevice).valid, `${key}=${value}`).toBe(true)
    }
  })

  it.each(ranged)('flags %s %s above 127 or below 0', (type, key) => {
    for (const value of [128, 255, 1000, -1]) {
      const result = validateConfig(withAction(type, { [key]: value }), liveRigDevice)
      expect(result.valid, `${key}=${value}`).toBe(false)
      expect(result.byPath.has(`/modes/0/banks/0/patches/0/buttons/0/actions/0/actionParameters/${key}`)).toBe(true)
    }
  })

  it('still accepts the sample file, which has control changes with no On/Off value', () => {
    const config = withAction('ControlChange', { controllerNumber: 102 })
    expect(validateConfig(config, liveRigDevice).valid).toBe(true)
  })
})
