// Tests for lib/actionDefaults.ts: velocity and control change values are never left empty.
import { describe, expect, it } from 'vitest'
import {
  CC_DEFAULT_OFF,
  CC_DEFAULT_ON,
  DEFAULT_VELOCITY,
  ensureActionDefaults,
  fillActionDefaults,
  newPlayNotesAction,
} from './actionDefaults'
import type { Action, Button, DeviceConfig } from '../types/config'

const cc = (params: object): Action => ({ type: 'ControlChange', actionParameters: params })
const notes = (params: object): Action => ({ type: 'PlayNotes', actionParameters: params })
const button = (id: number, ...actions: Action[]): Button => ({ id, name: `b${id}`, buttonMode: 'Momentary', actions })

describe('the defaults', () => {
  it('are velocity 100, and CC full on and full off', () => {
    expect(DEFAULT_VELOCITY).toBe(100)
    expect(CC_DEFAULT_ON).toBe(127)
    expect(CC_DEFAULT_OFF).toBe(0)
  })
})

describe('newPlayNotesAction', () => {
  it('starts with the default velocity', () => {
    expect(newPlayNotesAction()).toEqual({ type: 'PlayNotes', actionParameters: { velocity: 100 } })
  })

  it('gives a fresh object each time', () => {
    const a = newPlayNotesAction()
    const b = newPlayNotesAction()
    a.actionParameters!.velocity = 5
    expect(b.actionParameters!.velocity).toBe(100)
  })
})

describe('ensureActionDefaults', () => {
  it('gives Play notes a velocity if it has none', () => {
    const action: Action = { type: 'PlayNotes' }
    ensureActionDefaults(action)
    expect(action.actionParameters).toEqual({ velocity: 100 })
  })

  it('keeps the notes and anything else set on a Play notes action', () => {
    const action = notes({ noteNames: ['C3'], noteSpread: 'SpreadUp', midiChannel: 2 })
    ensureActionDefaults(action)
    expect(action.actionParameters).toEqual({ noteNames: ['C3'], noteSpread: 'SpreadUp', midiChannel: 2, velocity: 100 })
  })

  it('never changes a velocity that is set, including 0', () => {
    for (const velocity of [0, 1, 64, 127]) {
      const action = notes({ velocity })
      ensureActionDefaults(action)
      expect(action.actionParameters).toEqual({ velocity })
    }
  })

  it('gives a bare control change both values', () => {
    const action: Action = { type: 'ControlChange' }
    ensureActionDefaults(action)
    expect(action.actionParameters).toEqual({ controlValue: 127, releaseValue: 0 })
  })

  it('fills only the control change value that is missing, and keeps the rest', () => {
    const onOnly = cc({ controllerNumber: 102, controlValue: 64 })
    ensureActionDefaults(onOnly)
    expect(onOnly.actionParameters).toEqual({ controllerNumber: 102, controlValue: 64, releaseValue: 0 })

    const offOnly = cc({ releaseValue: 20 })
    ensureActionDefaults(offOnly)
    expect(offOnly.actionParameters).toEqual({ controlValue: 127, releaseValue: 20 })

    const set = cc({ controlValue: 0, releaseValue: 127 })
    ensureActionDefaults(set)
    expect(set.actionParameters).toEqual({ controlValue: 0, releaseValue: 127 })
  })

  it('leaves every other action type alone', () => {
    const program: Action = { type: 'ProgramChange' }
    const patch: Action = { type: 'SetPatch', actionParameters: { patchIndex: 1 } }
    ensureActionDefaults(program)
    ensureActionDefaults(patch)
    expect(program.actionParameters).toBeUndefined()
    expect(patch.actionParameters).toEqual({ patchIndex: 1 })
  })
})

describe('fillActionDefaults', () => {
  const config = (): DeviceConfig => ({
    presets: [{ id: 0, name: 'p', actions: [cc({ controllerNumber: 1 }), notes({ noteNames: ['C3'] })] }],
    buttons: [button(0, cc({ controllerNumber: 2 }))],
    modes: [
      {
        name: 'M',
        buttons: [button(1, notes({ noteNames: ['D3'] }))],
        banks: [
          {
            name: 'B',
            buttons: [button(2, cc({ controllerNumber: 4 }))],
            patches: [
              {
                name: 'P',
                buttons: [button(3, cc({ controllerNumber: 5 }), notes({ noteNames: ['E3'] }), { type: 'ProgramChange', actionParameters: { programNumber: 1 } })],
              },
            ],
          },
        ],
      },
    ],
  })

  it('fills every Play notes and control change at every level, presets included', () => {
    const c = config()
    fillActionDefaults(c)
    const patchActions = c.modes[0].banks[0].patches[0].buttons[0].actions
    expect(c.presets![0].actions.map((a) => a.actionParameters)).toEqual([
      { controllerNumber: 1, controlValue: 127, releaseValue: 0 },
      { noteNames: ['C3'], velocity: 100 },
    ])
    expect(c.buttons![0].actions[0].actionParameters).toEqual({ controllerNumber: 2, controlValue: 127, releaseValue: 0 })
    expect(c.modes[0].buttons![0].actions[0].actionParameters).toEqual({ noteNames: ['D3'], velocity: 100 })
    expect(c.modes[0].banks[0].buttons![0].actions[0].actionParameters).toEqual({ controllerNumber: 4, controlValue: 127, releaseValue: 0 })
    expect(patchActions[0].actionParameters).toEqual({ controllerNumber: 5, controlValue: 127, releaseValue: 0 })
    expect(patchActions[1].actionParameters).toEqual({ noteNames: ['E3'], velocity: 100 })
  })

  it('does not touch other actions', () => {
    const c = config()
    fillActionDefaults(c)
    expect(c.modes[0].banks[0].patches[0].buttons[0].actions[2].actionParameters).toEqual({ programNumber: 1 })
  })

  it('copes with a file that has no presets and no extra button lists', () => {
    const c: DeviceConfig = { modes: [{ name: 'M', banks: [{ name: 'B', patches: [{ name: 'P', buttons: [button(0, notes({}), cc({}))] }] }] }] }
    fillActionDefaults(c)
    const [n, k] = c.modes[0].banks[0].patches[0].buttons[0].actions
    expect(n.actionParameters).toEqual({ velocity: 100 })
    expect(k.actionParameters).toEqual({ controlValue: 127, releaseValue: 0 })
  })
})
