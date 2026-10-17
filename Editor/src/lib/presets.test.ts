// Tests for lib/presets.ts: ordering, flattening, usage, and removal of presets.
import { describe, expect, it } from 'vitest'
import {
  actionsForNewPreset,
  canCreatePresetFrom,
  flattenActions,
  hasDuplicateName,
  isPresetNameTaken,
  nextPresetId,
  presetUsers,
  removeEmptyPresets,
  removePreset,
  runsAnyPreset,
  sortedPresets,
  uniquePresetName,
} from './presets'
import { pruneCosmeticButtons } from './buttons'
import type { Action, Button, DeviceConfig, Preset } from '../types/config'

const pc = (n: number): Action => ({ type: 'ProgramChange', actionParameters: { programNumber: n } })
const note = (n: string): Action => ({ type: 'PlayNotes', actionParameters: { noteNames: [n] } })
const run = (presetId: number): Action => ({ type: 'ExecutePreset', actionParameters: { presetId } })
const preset = (id: number, name: string, ...actions: Action[]): Preset => ({ id, name, actions })
const button = (id: number, name: string, ...actions: Action[]): Button => ({ id, name, buttonMode: 'Momentary', actions })

function config(): DeviceConfig {
  return {
    presets: [preset(0, 'verse', pc(1), pc(2)), preset(1, 'Chorus', note('C3')), preset(2, 'Bridge', pc(9))],
    buttons: [button(0, 'File one', run(0))],
    modes: [
      {
        name: 'Pads',
        buttons: [button(1, 'Mode one', note('E3'), run(1))],
        banks: [
          {
            name: 'Keys',
            patches: [{ name: 'Intro', buttons: [button(2, 'Patch one', run(0), run(1)), button(3, 'Plain', pc(5))] }],
          },
        ],
      },
    ],
  }
}

describe('nextPresetId', () => {
  it('is 0 for no presets and one past the highest id otherwise', () => {
    expect(nextPresetId({ modes: [] })).toBe(0)
    expect(nextPresetId(config())).toBe(3)
  })
})

describe('sortedPresets', () => {
  it('sorts by name, ignoring case, without touching the stored order', () => {
    const c = config()
    expect(sortedPresets(c).map((p) => p.name)).toEqual(['Bridge', 'Chorus', 'verse'])
    expect(c.presets!.map((p) => p.name)).toEqual(['verse', 'Chorus', 'Bridge'])
  })

  it('breaks ties by id so the order is stable', () => {
    const c: DeviceConfig = { modes: [], presets: [preset(5, 'Same', pc(1)), preset(2, 'Same', pc(1))] }
    expect(sortedPresets(c).map((p) => p.id)).toEqual([2, 5])
  })

  it('sorts an unnamed preset by its placeholder', () => {
    const c: DeviceConfig = { modes: [], presets: [preset(0, 'Zed', pc(1)), preset(1, '  ', pc(1))] }
    expect(sortedPresets(c).map((p) => p.id)).toEqual([1, 0])
  })
})

describe('hasDuplicateName', () => {
  it('spots another preset with the same name, ignoring case and spacing', () => {
    const c = config()
    c.presets!.push(preset(3, ' CHORUS ', pc(1)))
    expect(hasDuplicateName(c, c.presets![1])).toBe(true)
    expect(hasDuplicateName(c, c.presets![0])).toBe(false)
  })

  it('never flags an empty name', () => {
    const c: DeviceConfig = { modes: [], presets: [preset(0, '', pc(1)), preset(1, '', pc(1))] }
    expect(hasDuplicateName(c, c.presets![0])).toBe(false)
  })
})

describe('flattenActions', () => {
  it('replaces an ExecutePreset with the preset actions in place, in the order written', () => {
    const c = config()
    const out = flattenActions([note('A1'), run(0), pc(7), run(1)], c)
    expect(out.map((a) => a.type)).toEqual(['PlayNotes', 'ProgramChange', 'ProgramChange', 'ProgramChange', 'PlayNotes'])
    expect(out.map((a) => a.actionParameters?.programNumber ?? a.actionParameters?.noteNames?.[0])).toEqual([
      'A1',
      1,
      2,
      7,
      'C3',
    ])
  })

  it('can run the same preset twice', () => {
    expect(flattenActions([run(1), run(1)], config())).toHaveLength(2)
  })

  it('skips a preset that does not exist', () => {
    expect(flattenActions([run(99), pc(1)], config())).toEqual([pc(1)])
  })

  it('ignores an ExecutePreset inside a preset (no nesting)', () => {
    const c = config()
    c.presets![0].actions.push(run(1))
    expect(flattenActions([run(0)], c)).toEqual([pc(1), pc(2)])
  })
})

describe('presetUsers', () => {
  it('lists every button that runs the preset, with where it lives', () => {
    expect(presetUsers(config(), 0)).toEqual([
      { button: 'File one', location: 'File' },
      { button: 'Patch one', location: 'Pads > Keys > Intro' },
    ])
    expect(presetUsers(config(), 2)).toEqual([])
  })
})

describe('removePreset', () => {
  it('removes the preset and every action that runs it, keeping the rest in order', () => {
    const c = config()
    expect(removePreset(c, 1)).toBe(2)
    expect(c.presets!.map((p) => p.id)).toEqual([0, 2])
    expect(c.modes[0].buttons![0].actions).toEqual([note('E3')])
    expect(c.modes[0].banks[0].patches[0].buttons[0].actions).toEqual([run(0)])
  })

  it('removes a button that is left with no actions', () => {
    const c = config()
    removePreset(c, 0)
    expect(c.buttons).toEqual([])
    expect(c.modes[0].banks[0].patches[0].buttons.map((b) => b.name)).toEqual(['Patch one', 'Plain'])
    expect(c.modes[0].banks[0].patches[0].buttons[0].actions).toEqual([run(1)])
  })

  it('drops the presets list itself once the last preset goes', () => {
    const c: DeviceConfig = { modes: [], presets: [preset(0, 'Only', pc(1))] }
    removePreset(c, 0)
    expect(c.presets).toBeUndefined()
  })
})

describe('removeEmptyPresets', () => {
  it('removes a preset with no actions along with the actions that ran it', () => {
    const c = config()
    c.presets![2].actions = []
    c.buttons = [button(0, 'Only runs bridge', run(2))]
    removeEmptyPresets(c)
    expect(c.presets!.map((p) => p.id)).toEqual([0, 1])
    expect(c.buttons).toEqual([])
  })
})

describe('saving', () => {
  it('keeps a button that only runs a preset (it is not cosmetic)', () => {
    const c = config()
    c.modes[0].buttons = [button(1, 'Only a preset', run(1))]
    expect(pruneCosmeticButtons(c).modes[0].buttons).toHaveLength(1)
  })

  it('leaves no empty presets list in the saved file', () => {
    const c: DeviceConfig = { modes: config().modes, presets: [] }
    expect('presets' in pruneCosmeticButtons(c)).toBe(false)
  })
})

describe('creating a preset from a button', () => {
  it('copies the actions of a plain button', () => {
    const c = config()
    const plain = c.modes[0].banks[0].patches[0].buttons[1] // "Plain": one ProgramChange
    const copy = actionsForNewPreset(plain)
    expect(copy).toEqual([pc(5)])
    // a real copy: changing it must not touch the button
    ;(copy[0].actionParameters as { programNumber: number }).programNumber = 99
    expect(plain.actions[0].actionParameters).toEqual({ programNumber: 5 })
  })

  it('is possible for a button with actions and no preset', () => {
    expect(canCreatePresetFrom(button(5, 'A', pc(1)))).toBe(true)
    expect(canCreatePresetFrom(button(5, 'B', pc(1), note('C3')))).toBe(true)
  })

  it('is not possible when the button has no actions', () => {
    expect(canCreatePresetFrom(button(5, 'Empty'))).toBe(false)
  })

  it('is not possible once the button runs any preset, because a preset cannot run a preset', () => {
    expect(canCreatePresetFrom(button(5, 'Only a preset', run(0)))).toBe(false)
    expect(canCreatePresetFrom(button(5, 'Mixed', pc(1), run(0)))).toBe(false)
    expect(canCreatePresetFrom(button(5, 'Two', run(0), run(1)))).toBe(false)
  })

  it('tells whether a button runs a preset', () => {
    expect(runsAnyPreset(button(5, 'Plain', pc(1)))).toBe(false)
    expect(runsAnyPreset(button(5, 'Empty'))).toBe(false)
    expect(runsAnyPreset(button(5, 'Mixed', pc(1), run(0)))).toBe(true)
  })
})

describe('preset names must be unique', () => {
  it('finds a taken name, ignoring case and spaces', () => {
    const c = config()
    expect(isPresetNameTaken(c, 'chorus')).toBe(true)
    expect(isPresetNameTaken(c, '  VERSE ')).toBe(true)
    expect(isPresetNameTaken(c, 'Outro')).toBe(false)
    expect(isPresetNameTaken({ modes: [] }, 'x')).toBe(false)
  })

  it('never treats an empty name as taken (unnamed presets are allowed)', () => {
    const c = config()
    c.presets!.push(preset(9, ''))
    expect(isPresetNameTaken(c, '')).toBe(false)
    expect(isPresetNameTaken(c, '   ')).toBe(false)
  })

  it('leaves out the preset being renamed, so it does not clash with itself', () => {
    const c = config()
    const chorus = c.presets![1]
    expect(isPresetNameTaken(c, 'Chorus', chorus)).toBe(false)
    expect(isPresetNameTaken(c, 'chorus ', chorus)).toBe(false)
    // but another preset's name is still taken
    expect(isPresetNameTaken(c, 'Bridge', chorus)).toBe(true)
  })

  it('picks the first free name by numbering', () => {
    const c = config()
    expect(uniquePresetName(c, 'Intro')).toBe('Intro')
    expect(uniquePresetName(c, 'Chorus')).toBe('Chorus 2')
    c.presets!.push(preset(5, 'Chorus 2'))
    expect(uniquePresetName(c, 'Chorus')).toBe('Chorus 3')
    expect(uniquePresetName({ modes: [] }, 'New preset')).toBe('New preset')
  })
})
