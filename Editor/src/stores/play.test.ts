// Tests for the Play-mode simulator: held-note safety, exclusive buttons, navigation and parameter cycling.
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useConfigStore } from './config'
import { TOAST_MIN_MS, usePlayStore } from './play'
import type { Action, DeviceConfig } from '../types/config'

vi.mock('../lib/midi', () => ({
  sendNoteOn: vi.fn(),
  sendNoteOff: vi.fn(),
  sendControlChange: vi.fn(),
  sendProgramChange: vi.fn(),
}))

import { sendControlChange, sendNoteOff, sendNoteOn, sendProgramChange } from '../lib/midi'

function load(config: DeviceConfig) {
  const configStore = useConfigStore()
  configStore.config = config
  const play = usePlayStore()
  play.enterPlayMode()
  return play
}

beforeEach(() => {
  setActivePinia(createPinia())
  vi.clearAllMocks()
  vi.useRealTimers()
})

// A two-patch bank: P1's id-0 button latches a held chord; id 1 jumps
// straight to P2. Id 2's CC button lives at Bank level (shared by both
// patches), so pressing it demonstrates "commit the pending nav, then run
// the button's own action against the now-live patch" without the button
// itself vanishing when the patch changes underneath it.
const CHORD_TREE: DeviceConfig = {
  modes: [
    {
      name: 'M1',
      banks: [
        {
          name: 'B1',
          buttons: [
            {
              id: 2,
              name: 'CC',
              buttonMode: 'Momentary',
              actions: [{ type: 'ControlChange', actionParameters: { controllerNumber: 20, controlValue: 100 } }],
            },
          ],
          patches: [
            {
              name: 'P1',
              buttons: [
                {
                  id: 0,
                  name: 'Chord',
                  buttonMode: 'Latched',
                  actions: [{ type: 'PlayNotes', actionParameters: { noteNames: ['C4', 'E4', 'G4'] } }],
                },
                {
                  id: 1,
                  name: 'ToP2',
                  buttonMode: 'Momentary',
                  actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 1 } }],
                },
              ],
            },
            { name: 'P2', buttons: [] },
          ],
        },
      ],
    },
  ],
}

describe('enterPlayMode', () => {
  it('lands on Mode 0 / Bank 0 / its default Patch, pending == live', () => {
    const play = load(CHORD_TREE)
    expect(play.modeIndex).toBe(0)
    expect(play.bankIndex).toBe(0)
    expect(play.patchIndex).toBe(0)
    expect(play.dirty).toBe(false)
  })
})

describe('held-note safety — the whole point', () => {
  it('cycling pending Patch does not release a latched chord until activation', () => {
    const play = load(CHORD_TREE)
    play.pressPhysical(0)
    expect(sendNoteOn).toHaveBeenCalledTimes(3)
    expect(play.isOn(0)).toBe(true)

    play.pendingCyclePatch('Up')
    expect(play.dirty).toBe(true)
    expect(sendNoteOff).not.toHaveBeenCalled()
    expect(play.isOn(0)).toBe(true)

    play.activateSelection()
    expect(sendNoteOff).toHaveBeenCalledTimes(3)
    expect(play.patchIndex).toBe(1)
    expect(play.dirty).toBe(false)
  })

  it('a pure-performance button press pre-commits a pending change, then runs on the new patch', () => {
    const play = load(CHORD_TREE)
    play.pendingCyclePatch('Up') // pending -> P2, live still P1
    expect(play.dirty).toBe(true)

    play.pressPhysical(2) // ControlChange only — pure performance
    expect(play.patchIndex).toBe(1) // committed
    expect(play.dirty).toBe(false)
    expect(sendControlChange).toHaveBeenCalledWith(20, 100, 1)
  })

  it('a SetPatch button jumps immediately, discarding any pending change with one teardown', () => {
    const play = load(CHORD_TREE)
    play.pressPhysical(0) // latch the chord
    play.pendingCycleBank('Up') // no other bank — dirty stays false; use patch cycle instead
    play.pendingCyclePatch('Down') // dirty now, pending wraps to P2 (only 2 patches)

    play.pressPhysical(1) // SetPatch -> patchIndex 1 directly
    expect(play.patchIndex).toBe(1)
    expect(play.dirty).toBe(false)
    expect(sendNoteOff).toHaveBeenCalledTimes(3) // the chord's teardown
  })

  it('a Momentary button releases on release, not on press', () => {
    const play = load(CHORD_TREE)
    play.pressPhysical(2)
    expect(sendControlChange).toHaveBeenLastCalledWith(20, 100, 1)
    play.releasePhysical(2)
    expect(sendControlChange).toHaveBeenLastCalledWith(20, 0, 1)
  })

  it('a round trip at any level clears dirty with no MIDI sent', () => {
    const play = load(CHORD_TREE)
    play.pendingCyclePatch('Up')
    play.pendingCyclePatch('Down')
    expect(play.dirty).toBe(false)
    expect(sendNoteOff).not.toHaveBeenCalled()
    expect(sendNoteOn).not.toHaveBeenCalled()
  })
})

describe('Exclusive buttons', () => {
  const EXCLUSIVE_TREE: DeviceConfig = {
    modes: [
      {
        name: 'M1',
        banks: [
          {
            name: 'B1',
            patches: [
              {
                name: 'P1',
                buttons: [
                  {
                    id: 0,
                    name: 'Clean',
                    buttonMode: 'Exclusive',
                    buttonSet: [1],
                    actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 1 } }],
                  },
                  {
                    id: 1,
                    name: 'Crunch',
                    buttonMode: 'Exclusive',
                    buttonSet: [0],
                    actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 2 } }],
                  },
                ],
              },
            ],
          },
        ],
      },
    ],
  }

  it('turns off the sibling in its set before turning itself on', () => {
    const play = load(EXCLUSIVE_TREE)
    play.pressPhysical(0)
    expect(play.isOn(0)).toBe(true)
    play.pressPhysical(1)
    expect(play.isOn(0)).toBe(false)
    expect(play.isOn(1)).toBe(true)
  })

  it('pressing an already-on Exclusive button turns it off, like Latched', () => {
    const play = load(EXCLUSIVE_TREE)
    play.pressPhysical(0)
    play.pressPhysical(0)
    expect(play.isOn(0)).toBe(false)
  })
})

describe('navigation reentrancy guard', () => {
  it('navSetPatch is a no-op when the target is already the live patch', () => {
    const play = load(CHORD_TREE)
    expect(play.patchIndex).toBe(0)
    play.navSetPatch(0)
    expect(sendNoteOff).not.toHaveBeenCalled()
    expect(sendNoteOn).not.toHaveBeenCalled()
  })

  it('navSetMode is a no-op when the target is already the live mode', () => {
    const play = load(CHORD_TREE)
    play.navSetMode(0)
    expect(play.modeIndex).toBe(0)
    expect(sendNoteOff).not.toHaveBeenCalled()
  })
})

describe('MaintainActiveButtonOnPatchChange', () => {
  const MAINTAIN_TREE: DeviceConfig = {
    modes: [
      {
        name: 'M1',
        banks: [
          {
            name: 'B1',
            maintainActiveButtonOnPatchChange: true,
            patches: [
              {
                name: 'P1',
                buttons: [
                  {
                    id: 0,
                    name: 'Chord A',
                    buttonMode: 'Latched',
                    actions: [{ type: 'PlayNotes', actionParameters: { noteNames: ['C4'] } }],
                  },
                ],
              },
              {
                name: 'P2',
                buttons: [
                  {
                    id: 0,
                    name: 'Chord B',
                    buttonMode: 'Latched',
                    actions: [{ type: 'PlayNotes', actionParameters: { noteNames: ['D4'] } }],
                  },
                ],
              },
            ],
          },
        ],
      },
    ],
  }

  it('re-triggers a held button against the incoming patch, picking up its own voicing', () => {
    const play = load(MAINTAIN_TREE)
    play.pressPhysical(0) // holds Chord A (C4)
    expect(sendNoteOn).toHaveBeenLastCalledWith(60, 100, 1)

    play.navSetPatch(1)

    expect(sendNoteOff).toHaveBeenCalledWith(60, 1) // A's note-off
    expect(sendNoteOn).toHaveBeenLastCalledWith(62, 100, 1) // B's own voicing (D4), re-triggered
    expect(play.isOn(0)).toBe(true)
  })
})

describe('CycleActivateParameter (built-in footswitch 10 role)', () => {
  it('advances the selector when nothing is pending, activates when dirty', () => {
    const play = load(CHORD_TREE)
    expect(play.selectedParameter).toBe('Patch')

    play.pressPhysical(10) // built-in default: CycleActivateParameter
    expect(play.selectedParameter).toBe('Bank')
    expect(play.dirty).toBe(false)

    play.pendingCyclePatch('Up')
    expect(play.dirty).toBe(true)

    play.pressPhysical(10)
    expect(play.dirty).toBe(false)
    expect(play.patchIndex).toBe(1)
  })
})

describe('exitPlayMode', () => {
  it('releases every held note so nothing is left sounding', () => {
    const play = load(CHORD_TREE)
    play.pressPhysical(0)
    expect(play.isOn(0)).toBe(true)

    play.exitPlayMode()

    expect(sendNoteOff).toHaveBeenCalledTimes(3)
    expect(play.active).toBe(false)
  })
})

describe('PlayNotes velocity and channel', () => {
  it('defaults velocity to 100 and resolves the patch MIDI channel', () => {
    const tree: DeviceConfig = {
      modes: [
        {
          name: 'M1',
          banks: [
            {
              name: 'B1',
              patches: [
                {
                  name: 'P1',
                  midiChannel: 5,
                  buttons: [
                    {
                      id: 0,
                      name: 'Note',
                      buttonMode: 'Momentary',
                      actions: [{ type: 'PlayNotes', actionParameters: { noteNames: ['C4'] } }],
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
    }
    const play = load(tree)
    play.pressPhysical(0)
    expect(sendNoteOn).toHaveBeenCalledWith(60, 100, 5)
  })
})

describe('ProgramChange', () => {
  it('sends on press and nothing on release', () => {
    const tree: DeviceConfig = {
      modes: [
        {
          name: 'M1',
          banks: [
            {
              name: 'B1',
              patches: [
                {
                  name: 'P1',
                  buttons: [
                    {
                      id: 0,
                      name: 'PC',
                      buttonMode: 'Momentary',
                      actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 7 } }],
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
    }
    const play = load(tree)
    play.pressPhysical(0)
    expect(sendProgramChange).toHaveBeenCalledWith(7, 1)
    play.releasePhysical(0)
    expect(sendProgramChange).toHaveBeenCalledTimes(1)
  })
})

// ---- button-name overlay and the Live screen layout -------------------------

const TOAST_CONFIG: DeviceConfig = {
  modes: [
    {
      name: 'M1',
      banks: [
        {
          name: 'B1',
          screenLayout: 'Live',
          subheaderColourCode: [1, 2, 3],
          patches: [
            {
              name: 'P1',
              buttons: [
                { id: 0, name: 'Lead', buttonMode: 'Momentary', colourCode: [200, 10, 10], actions: [{ type: 'PlayNotes', actionParameters: { noteNames: ['C3'] } }] },
                { id: 1, name: 'Wah', buttonMode: 'Latched', actions: [{ type: 'ControlChange', actionParameters: { controllerNumber: 11 } }] },
                { id: 2, name: 'Next', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 1 } }] },
                { id: 3, name: 'Step', buttonMode: 'Momentary', actions: [{ type: 'CyclePatch', actionParameters: { direction: 'Up' } }] },
                { id: 4, name: 'Both', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 1 } }, { type: 'ControlChange', actionParameters: { controllerNumber: 1 } }] },
                { id: 5, name: 'Pg2', page: 2, buttonMode: 'Momentary', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 1 } }] },
              ],
            },
            { name: 'P2', buttons: [] },
          ],
        },
      ],
    },
  ],
}

describe('button-name overlay', () => {
  beforeEach(() => {
    vi.useFakeTimers()
  })

  it('shows the name of a button that sends MIDI, in the button colour', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(0)
    expect(play.toastVisible).toBe(true)
    expect(play.toastText).toBe('Lead')
    expect(play.toastColour).toEqual([200, 10, 10])
  })

  it('falls back to the bank colour when the button has none', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(1)
    expect(play.toastColour).toEqual([1, 2, 3])
  })

  it('shows nothing for buttons that only navigate', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(2)
    expect(play.toastVisible).toBe(false)
    play.releasePhysical(2)
    play.pressPhysical(3)
    expect(play.toastVisible).toBe(false)
  })

  it('shows the name of a button that navigates and sends MIDI, and the patch change is immediate', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(4)
    expect(play.patchIndex).toBe(1)
    expect(play.toastText).toBe('Both')
  })

  it('shows ON then OFF for a latched button', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(1)
    expect(play.toastText).toBe('Wah ON')
    play.pressPhysical(1)
    expect(play.toastText).toBe('Wah OFF')
  })

  it('disappears after 500ms for a latched button', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(1)
    vi.advanceTimersByTime(TOAST_MIN_MS - 1)
    expect(play.toastVisible).toBe(true)
    vi.advanceTimersByTime(1)
    expect(play.toastVisible).toBe(false)
  })

  it('stays up while a momentary button is held, then goes when released', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(0)
    vi.advanceTimersByTime(5000)
    expect(play.toastVisible).toBe(true)
    play.releasePhysical(0)
    expect(play.toastVisible).toBe(false)
  })

  it('lasts the full minimum time after a quick tap of a momentary button', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(0)
    play.releasePhysical(0)
    expect(play.toastVisible).toBe(true)
    vi.advanceTimersByTime(TOAST_MIN_MS)
    expect(play.toastVisible).toBe(false)
  })

  it('restarts the timer when another MIDI button is pressed', () => {
    const play = load(TOAST_CONFIG)
    play.pressPhysical(1)
    vi.advanceTimersByTime(400)
    play.pressPhysical(1)
    vi.advanceTimersByTime(400)
    expect(play.toastVisible).toBe(true)
    vi.advanceTimersByTime(100)
    expect(play.toastVisible).toBe(false)
  })
})

describe('live screen layout', () => {
  it('reads the layout from the live bank', () => {
    const play = load(TOAST_CONFIG)
    expect(play.liveLayout).toBe(true)
    const grid = load({ modes: [{ name: 'M', banks: [{ name: 'B', patches: [{ name: 'P', buttons: [] }] }] }] })
    expect(grid.liveLayout).toBe(false)
  })

  it('previews the page count of the pending patch, so single-page patches show no page box', () => {
    const play = load(TOAST_CONFIG)
    expect(play.previewPageCount).toBe(2) // P1 has a page-2 button
    play.pendingPatchIndex = 1
    expect(play.previewPageCount).toBe(1) // P2 has a single page
  })
})

describe('Set patch / Set bank with a longer target path', () => {
  const tree: DeviceConfig = {
    modes: [
      {
        name: 'M1',
        banks: [
          { name: 'B1', patches: [{ name: 'P1', buttons: [] }, { name: 'P2', buttons: [] }] },
          { name: 'B2', defaultPatchIndex: 0, patches: [{ name: 'P1', buttons: [] }] },
        ],
      },
      {
        name: 'M2',
        banks: [
          { name: 'B1', patches: [{ name: 'P1', buttons: [] }] },
          { name: 'B2', defaultPatchIndex: 1, patches: [{ name: 'P1', buttons: [] }, { name: 'P2', buttons: [] }] },
        ],
      },
    ],
  }

  it('stays in the current bank with only a patch index', () => {
    const play = load(tree)
    play.navSetPatch(1)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([0, 0, 1])
  })

  it('reaches a patch of another bank, and of another mode', () => {
    const play = load(tree)
    play.navSetPatch(0, 1)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([0, 1, 0])
    play.navSetPatch(1, 1, 1)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([1, 1, 1])
  })

  it('ignores a mode index without a bank index, and a target that does not exist', () => {
    const play = load(tree)
    play.navSetPatch(1, undefined, 1)
    expect([play.modeIndex, play.patchIndex]).toEqual([0, 1])
    play.navSetPatch(5, 0, 0)
    expect(play.patchIndex).toBe(1)
  })

  it('does not treat the same patch number in another bank as already there', () => {
    const play = load(tree)
    play.navSetPatch(0, 1)
    expect(play.bankIndex).toBe(1)
  })

  it('SetBank reaches a bank of another mode and lands on its default patch', () => {
    const play = load(tree)
    play.navSetBank(1, 1)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([1, 1, 1])
  })

  it('runs through a button action', () => {
    const play = load({
      modes: [
        {
          name: 'M1',
          banks: [
            {
              name: 'B1',
              patches: [
                {
                  name: 'P1',
                  buttons: [{ id: 0, name: 'Go', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { modeIndex: 1, bankIndex: 0, patchIndex: 0 } }] }],
                },
              ],
            },
          ],
        },
        { name: 'M2', banks: [{ name: 'B1', patches: [{ name: 'Far', buttons: [] }] }] },
      ],
    })
    play.pressPhysical(0)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([1, 0, 0])
  })
})

describe('Set patch for the patch that is already live', () => {
  const chord: Action = { type: 'PlayNotes', actionParameters: { noteNames: ['C4'] } }
  const resetTree = (selfSelectOnLoad = false): DeviceConfig => ({
    modes: [
      {
        name: 'M1',
        banks: [
          {
            name: 'B1',
            patches: [
              {
                name: 'P1',
                buttons: [
                  { id: 0, name: 'chord', buttonMode: 'Latched', actions: [chord] },
                  { id: 1, name: 'boot', buttonMode: 'Latched', defaultStatus: 'On', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 7 } }] },
                  { id: 2, name: 'reset', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 0 } }] },
                  ...(selfSelectOnLoad
                    ? [
                        {
                          id: 3,
                          name: 'self',
                          buttonMode: 'Latched' as const,
                          defaultStatus: 'On' as const,
                          actions: [{ type: 'SetPatch' as const, actionParameters: { patchIndex: 0 } }, { type: 'ProgramChange' as const, actionParameters: { programNumber: 9 } }],
                        },
                      ]
                    : []),
                ],
              },
              { name: 'P2', buttons: [] },
            ],
          },
        ],
      },
    ],
  })

  it('resets it: held notes released, default-on buttons seeded again, page 1, pending discarded', () => {
    const play = load(resetTree())
    play.pressPhysical(0) // chord on
    vi.clearAllMocks()
    play.page = 2
    play.pendingCyclePatch('Up')
    expect(play.dirty).toBe(true)

    play.navSetPatch(0) // Set patch for the patch we are on

    expect(sendNoteOff).toHaveBeenCalledTimes(1)
    expect(sendProgramChange).toHaveBeenCalledWith(7, 1) // "boot" seeded again
    expect(play.isOn(0)).toBe(false)
    expect(play.isOn(1)).toBe(true)
    expect(play.page).toBe(1)
    expect(play.patchIndex).toBe(0)
    expect(play.dirty).toBe(false)
  })

  it('runs from a button press too', () => {
    const play = load(resetTree())
    play.pressPhysical(0) // chord on
    vi.clearAllMocks()

    play.pressPhysical(2) // the reset button

    expect(sendNoteOff).toHaveBeenCalledTimes(1)
    expect(play.isOn(0)).toBe(false)
  })

  it('does not recurse when a default-on button selects its own patch', () => {
    const play = load(resetTree(true))
    expect(vi.mocked(sendProgramChange).mock.calls.filter(([n]) => n === 9)).toHaveLength(1) // seeded once at entry
    vi.clearAllMocks()

    play.navSetPatch(0) // a deliberate reset re-seeds once

    expect(vi.mocked(sendProgramChange).mock.calls.filter(([n]) => n === 9)).toHaveLength(1)
    expect(play.isOn(3)).toBe(true)
  })
})

describe('Set bank and Set mode for the bank or mode already live', () => {
  const tree = (): DeviceConfig => ({
    modes: [
      {
        name: 'M1',
        banks: [
          {
            name: 'B1',
            defaultPatchIndex: 1,
            patches: [
              { name: 'P1', buttons: [] },
              {
                name: 'P2',
                buttons: [{ id: 9, name: 'seed', buttonMode: 'Latched', defaultStatus: 'On', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 3 } }] }],
              },
            ],
          },
          { name: 'B2', patches: [{ name: 'P1', buttons: [] }] },
        ],
      },
      {
        name: 'M2',
        banks: [],
        buttons: [{ id: 4, name: 'selfMode', buttonMode: 'Latched', defaultStatus: 'On', actions: [{ type: 'SetMode', actionParameters: { modeIndex: 1 } }] }],
      },
    ],
  })

  it('Set bank for the bank you are in goes home to its default patch', () => {
    const play = load(tree())
    play.navSetPatch(0) // leave the default patch
    expect(play.patchIndex).toBe(0)
    play.navSetBank(0)
    expect([play.bankIndex, play.patchIndex]).toEqual([0, 1])
  })

  it('Set bank from the default patch resets it: the default-on button is seeded again', () => {
    const play = load(tree())
    vi.clearAllMocks()
    play.navSetBank(0)
    expect(sendProgramChange).toHaveBeenCalledWith(3, 1)
    expect(play.isOn(9)).toBe(true)
  })

  it('Set mode for the mode you are in goes back to its first bank and default patch', () => {
    const play = load(tree())
    play.navSetBank(1)
    expect(play.bankIndex).toBe(1)
    play.navSetMode(0)
    expect([play.modeIndex, play.bankIndex, play.patchIndex]).toEqual([0, 0, 1])
  })

  it('a default-on button selecting its own bankless mode does not recurse, even on a reset', () => {
    const play = load(tree())
    play.navSetMode(1)
    expect([play.modeIndex, play.bankIndex]).toEqual([1, -1])
    expect(play.isOn(4)).toBe(true)
    play.navSetMode(1) // a deliberate reset terminates too
    expect(play.modeIndex).toBe(1)
  })
})

describe('presets', () => {
  const presetTree = (): DeviceConfig => ({
    presets: [
      {
        id: 4,
        name: 'Clean',
        actions: [
          { type: 'ProgramChange', actionParameters: { programNumber: 11 } },
          { type: 'ControlChange', actionParameters: { controllerNumber: 91, controlValue: 64, releaseValue: 5 } },
        ],
      },
    ],
    modes: [
      {
        name: 'M1',
        banks: [
          {
            name: 'B1',
            patches: [
              {
                name: 'P1',
                buttons: [
                  {
                    id: 0,
                    name: 'Mixed',
                    buttonMode: 'Momentary',
                    actions: [
                      { type: 'ProgramChange', actionParameters: { programNumber: 1 } },
                      { type: 'ExecutePreset', actionParameters: { presetId: 4 } },
                      { type: 'ProgramChange', actionParameters: { programNumber: 2 } },
                    ],
                  },
                ],
              },
            ],
          },
        ],
      },
    ],
  })

  it('runs a preset where it sits in the button action list, then the rest', () => {
    const play = load(presetTree())
    vi.clearAllMocks()
    play.pressPhysical(0)
    expect(vi.mocked(sendProgramChange).mock.calls.map(([n]) => n)).toEqual([1, 11, 2])
    expect(sendControlChange).toHaveBeenCalledWith(91, 64, 1)
  })

  it('releases the preset actions with the button (a CC sends its release value)', () => {
    const play = load(presetTree())
    play.pressPhysical(0)
    vi.clearAllMocks()
    play.releasePhysical(0)
    expect(sendControlChange).toHaveBeenCalledWith(91, 5, 1)
  })

  it('uses the edited preset the next time the button is pressed', () => {
    const tree = presetTree()
    const play = load(tree)
    tree.presets![0].actions[0].actionParameters = { programNumber: 99 }
    vi.clearAllMocks()
    play.pressPhysical(0)
    expect(vi.mocked(sendProgramChange).mock.calls.map(([n]) => n)).toEqual([1, 99, 2])
  })
})
