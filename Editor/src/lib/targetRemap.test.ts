// Tests for keeping Set patch/bank/mode actions aimed at the right item when the tree is reordered or items deleted.
import { describe, expect, it } from 'vitest'
import { applyStructureChange, findAffectedActions, mapPath } from './targetRemap'
import type { Action, Button, DeviceConfig, Patch } from '../types/config'

const set = (type: Action['type'], actionParameters: Action['actionParameters']): Action => ({ type, actionParameters })
const button = (id: number, name: string, ...actions: Action[]): Button => ({ id, name, buttonMode: 'Momentary', actions })
const patch = (name: string, ...buttons: Button[]): Patch => ({ name, buttons })

/** Pads: Keys (Intro, Verse, Chorus) and Strings (Solo, Outro); FX: Drive (a, b). */
function tree(): DeviceConfig {
  return {
    modes: [
      {
        name: 'Pads',
        banks: [
          { name: 'Keys', patches: [patch('Intro'), patch('Verse'), patch('Chorus')] },
          { name: 'Strings', patches: [patch('Solo'), patch('Outro')] },
        ],
      },
      { name: 'FX', banks: [{ name: 'Drive', patches: [patch('a'), patch('b')] }] },
    ],
  }
}

const firstParams = (buttons: Button[]) => buttons[0].actions[0].actionParameters

describe('mapPath', () => {
  it('shifts later siblings down on removal and drops the removed item and its children', () => {
    const remove = { kind: 'remove', path: [0, 1] } as const
    expect(mapPath([0, 2, 1], remove)).toEqual([0, 1, 1])
    expect(mapPath([0, 1, 1], remove)).toBeNull()
    expect(mapPath([0, 1], remove)).toBeNull()
    expect(mapPath([0, 0], remove)).toEqual([0, 0])
    expect(mapPath([1, 5], remove)).toEqual([1, 5])
    expect(mapPath([0], remove)).toEqual([0])
  })

  it('moves an item to a position and shifts the ones it passes, with everything inside them', () => {
    const down = { kind: 'move', parent: [0], from: 0, to: 2 } as const
    expect(mapPath([0, 0, 3], down)).toEqual([0, 2, 3])
    expect(mapPath([0, 1], down)).toEqual([0, 0])
    expect(mapPath([0, 2], down)).toEqual([0, 1])
    expect(mapPath([0, 3], down)).toEqual([0, 3])
    expect(mapPath([1, 0], down)).toEqual([1, 0])
    expect(mapPath([0], down)).toEqual([0])
    const up = { kind: 'move', parent: [0], from: 2, to: 0 } as const
    expect(mapPath([0, 2], up)).toEqual([0, 0])
    expect(mapPath([0, 0], up)).toEqual([0, 1])
    expect(mapPath([0, 1], up)).toEqual([0, 2])
  })
})

describe('moving', () => {
  it('puts a dragged patch where it was dropped: 1 below 3 of five gives 2, 3, 1, 4, 5', () => {
    const c = tree()
    c.modes[0].banks[0].patches = ['1', '2', '3', '4', '5'].map((name) => patch(name))
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 0, to: 2 })
    expect(c.modes[0].banks[0].patches.map((p) => p.name)).toEqual(['2', '3', '1', '4', '5'])
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 4, to: 1 }) // 5 dragged above 3
    expect(c.modes[0].banks[0].patches.map((p) => p.name)).toEqual(['2', '5', '3', '1', '4'])
  })

  it('re-aims a same-bank SetPatch when its sibling patches are reordered', () => {
    const c = tree()
    const bank = c.modes[0].banks[0]
    // On "Intro", a button that jumps to "Chorus" (patch 2).
    bank.patches[0].buttons = [button(0, 'To chorus', set('SetPatch', { patchIndex: 2 }))]
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 2, to: 0 }) // Chorus first
    expect(bank.patches.map((p) => p.name)).toEqual(['Chorus', 'Intro', 'Verse'])
    expect(firstParams(bank.patches[1].buttons)).toEqual({ patchIndex: 0 }) // the button is on Intro (now 1); still Chorus
  })

  it('keeps pointing at the same patch when the button itself moves', () => {
    const c = tree()
    const bank = c.modes[0].banks[0]
    bank.patches[0].buttons = [button(0, 'To verse', set('SetPatch', { patchIndex: 1 }))]
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 0, to: 2 }) // Intro to the end
    // Order is now Verse, Chorus, Intro; the button (on Intro, index 2) points at Verse (index 0).
    expect(firstParams(bank.patches[2].buttons)).toEqual({ patchIndex: 0 })
  })

  it('re-aims a SetBank and a cross-mode SetPatch when banks and modes are reordered', () => {
    const c = tree()
    c.modes[0].banks[0].patches[0].buttons = [
      button(0, 'Strings', set('SetBank', { bankIndex: 1 })),
      button(1, 'Drive b', set('SetPatch', { modeIndex: 1, bankIndex: 0, patchIndex: 1 })),
    ]
    applyStructureChange(c, { kind: 'move', parent: [0], from: 1, to: 0 }) // Strings before Keys
    const moved = c.modes[0].banks[1].patches[0].buttons // Keys is now bank 1
    expect(moved[0].actions[0].actionParameters).toEqual({ bankIndex: 0 })
    applyStructureChange(c, { kind: 'move', parent: [], from: 1, to: 0 }) // FX before Pads
    const buttons = c.modes[1].banks[1].patches[0].buttons // Pads is now mode 1
    expect(buttons[1].actions[0].actionParameters).toEqual({ modeIndex: 0, bankIndex: 0, patchIndex: 1 })
  })

  it('leaves an action alone when its stored indices still mean the same thing', () => {
    const c = tree()
    c.modes[0].banks[0].patches[0].buttons = [button(0, 'Verse', set('SetPatch', { patchIndex: 1 }))]
    applyStructureChange(c, { kind: 'move', parent: [1, 0], from: 0, to: 1 }) // reorder a/b in another bank
    expect(firstParams(c.modes[0].banks[0].patches[0].buttons)).toEqual({ patchIndex: 1 })
  })

  it('follows a bank defaultPatchIndex to the same patch', () => {
    const c = tree()
    c.modes[0].banks[0].defaultPatchIndex = 2 // Chorus
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 2, to: 0 })
    expect(c.modes[0].banks[0].defaultPatchIndex).toBeUndefined() // Chorus is now index 0, the default
    c.modes[0].banks[1].defaultPatchIndex = 0 // Solo
    applyStructureChange(c, { kind: 'move', parent: [0, 1], from: 0, to: 1 })
    expect(c.modes[0].banks[1].defaultPatchIndex).toBe(1)
  })

  it('never lists or removes anything on a move', () => {
    const c = tree()
    c.modes[0].banks[0].patches[0].buttons = [button(0, 'x', set('SetPatch', { patchIndex: 1 }))]
    expect(findAffectedActions(c, { kind: 'move', parent: [0, 0], from: 0, to: 1 })).toEqual([])
  })
})

describe('deleting', () => {
  it('renumbers actions that point past the deleted item', () => {
    const c = tree()
    c.modes[0].banks[0].patches[0].buttons = [button(0, 'Chorus', set('SetPatch', { patchIndex: 2 }))]
    applyStructureChange(c, { kind: 'remove', path: [0, 0, 1] }) // delete Verse
    expect(firstParams(c.modes[0].banks[0].patches[0].buttons)).toEqual({ patchIndex: 1 })
  })

  it('lists the actions that point at the item or anything inside it', () => {
    const c = tree()
    c.modes[1].banks[0].patches[0].buttons = [
      button(0, 'Go chorus', set('SetPatch', { modeIndex: 0, bankIndex: 0, patchIndex: 2 })),
      button(1, 'Go strings', set('SetBank', { modeIndex: 0, bankIndex: 1 })),
      button(2, 'Go FX', set('SetMode', { modeIndex: 1 })),
    ]
    const affected = findAffectedActions(c, { kind: 'remove', path: [0, 0] }) // delete bank Keys
    expect(affected).toEqual([
      { button: 'Go chorus', location: 'FX > Drive > a', action: 'Set patch', target: 'Pads > Keys > Chorus' },
    ])
    // deleting the whole Pads mode catches the bank and patch actions too
    expect(findAffectedActions(c, { kind: 'remove', path: [0] }).map((a) => a.button)).toEqual(['Go chorus', 'Go strings'])
  })

  it('does not list a SetBank when only a patch inside that bank is deleted', () => {
    const c = tree()
    c.modes[1].banks[0].patches[0].buttons = [button(0, 'Go strings', set('SetBank', { modeIndex: 0, bankIndex: 1 }))]
    expect(findAffectedActions(c, { kind: 'remove', path: [0, 1, 0] })).toEqual([])
  })

  it('does not list actions that live inside the deleted item', () => {
    const c = tree()
    c.modes[0].banks[0].patches[1].buttons = [button(0, 'inside', set('SetPatch', { patchIndex: 0 }))]
    expect(findAffectedActions(c, { kind: 'remove', path: [0, 0] })).toEqual([])
  })

  it('removes the actions, and the button too when that leaves it with none', () => {
    const c = tree()
    const buttons = [
      button(0, 'only action', set('SetPatch', { modeIndex: 0, bankIndex: 1, patchIndex: 0 })),
      button(1, 'two actions', set('SetPatch', { modeIndex: 0, bankIndex: 1, patchIndex: 1 }), set('SetMode', { modeIndex: 1 })),
      button(2, 'unrelated', set('SetMode', { modeIndex: 1 })),
    ]
    c.modes[1].banks[0].patches[0].buttons = buttons
    const removed = applyStructureChange(c, { kind: 'remove', path: [0, 1] }) // delete bank Strings
    expect(removed.map((a) => a.button)).toEqual(['only action', 'two actions'])
    expect(buttons.map((b) => b.name)).toEqual(['two actions', 'unrelated'])
    expect(buttons[0].actions.map((a) => a.type)).toEqual(['SetMode'])
  })

  it('re-aims what remains after a mode is deleted', () => {
    const c = tree()
    // In FX, a button pointing back at Pads > Strings > Outro.
    c.modes[1].banks[0].patches[0].buttons = [button(0, 'Outro', set('SetPatch', { modeIndex: 0, bankIndex: 1, patchIndex: 1 }))]
    applyStructureChange(c, { kind: 'remove', path: [0, 0] }) // delete bank Keys: Strings becomes bank 0
    expect(firstParams(c.modes[1].banks[0].patches[0].buttons)).toEqual({ modeIndex: 0, bankIndex: 0, patchIndex: 1 })
  })

  it('shifts and clears a bank defaultPatchIndex', () => {
    const c = tree()
    c.modes[0].banks[0].defaultPatchIndex = 2
    applyStructureChange(c, { kind: 'remove', path: [0, 0, 0] }) // delete Intro
    expect(c.modes[0].banks[0].defaultPatchIndex).toBe(1)
    applyStructureChange(c, { kind: 'remove', path: [0, 0, 1] }) // delete the default itself
    expect(c.modes[0].banks[0].defaultPatchIndex).toBeUndefined()
  })
})

describe('presets', () => {
  const preset = (id: number, name: string, ...actions: Action[]) => ({ id, name, actions })
  const run = (presetId: number): Action => set('ExecutePreset', { presetId })
  const full = (m: number, b: number, p: number) => set('SetPatch', { modeIndex: m, bankIndex: b, patchIndex: p })

  it('re-aims a fully-qualified target inside a preset when items move', () => {
    const c = tree()
    c.presets = [preset(0, 'Go', full(0, 0, 2))]
    applyStructureChange(c, { kind: 'move', parent: [0, 0], from: 2, to: 0 })
    expect(c.presets[0].actions[0].actionParameters).toEqual({ modeIndex: 0, bankIndex: 0, patchIndex: 0 })
  })

  it('removes the action that points at a deleted item, keeping the preset and others', () => {
    const c = tree()
    c.presets = [preset(0, 'Go', full(0, 0, 1), set('ProgramChange', { programNumber: 1 }))]
    applyStructureChange(c, { kind: 'remove', path: [0, 0, 1] })
    expect(c.presets[0].actions).toEqual([set('ProgramChange', { programNumber: 1 })])
  })

  it('deletes a preset left with no actions, and the button actions that ran it', () => {
    const c = tree()
    c.presets = [preset(0, 'Go', full(0, 0, 1)), preset(1, 'Keep', set('ProgramChange', { programNumber: 2 }))]
    c.buttons = [button(0, 'Runs go only', run(0)), button(1, 'Runs both', run(0), run(1))]
    applyStructureChange(c, { kind: 'remove', path: [0, 0, 1] })
    expect(c.presets.map((p) => p.id)).toEqual([1])
    expect(c.buttons!.map((b) => b.name)).toEqual(['Runs both'])
    expect(c.buttons![0].actions).toEqual([run(1)])
  })

  it('names a preset action in the confirmation list', () => {
    const c = tree()
    c.presets = [preset(0, 'Go', full(0, 0, 1))]
    expect(findAffectedActions(c, { kind: 'remove', path: [0, 0, 1] })).toEqual([
      { button: 'Go', location: 'Preset', action: 'Set patch', target: 'Pads > Keys > Verse' },
    ])
  })
})
