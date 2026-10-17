// Tests for deleting and moving modes/banks/patches through the store, including the confirmation dialog contents.
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useConfigStore } from './config'
import type { DeviceConfig } from '../types/config'

vi.mock('../lib/confirm', () => ({
  confirmDelete: vi.fn(async () => true),
  confirmAction: vi.fn(async () => true),
}))

vi.mock('../lib/prompt', () => ({
  promptText: vi.fn(async () => 'Chorus sound'),
}))

import { confirmAction, confirmDelete } from '../lib/confirm'
import { promptText } from '../lib/prompt'

function load(): ReturnType<typeof useConfigStore> {
  const store = useConfigStore()
  const config: DeviceConfig = {
    modes: [
      {
        name: 'Pads',
        banks: [
          {
            name: 'Keys',
            patches: [
              { name: 'Intro', buttons: [{ id: 0, name: 'To chorus', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { patchIndex: 2 } }] }] },
              { name: 'Verse', buttons: [] },
              { name: 'Chorus', buttons: [] },
            ],
          },
        ],
      },
      {
        name: 'FX',
        banks: [
          {
            name: 'Drive',
            patches: [
              {
                name: 'a',
                buttons: [
                  { id: 0, name: 'Back to chorus', buttonMode: 'Momentary', actions: [{ type: 'SetPatch', actionParameters: { modeIndex: 0, bankIndex: 0, patchIndex: 2 } }] },
                ],
              },
            ],
          },
        ],
      },
    ],
  }
  store.config = config
  return store
}

beforeEach(() => {
  setActivePinia(createPinia())
  vi.clearAllMocks()
})

describe('removeItem', () => {
  it('uses the plain prompt when nothing points at the item', async () => {
    const store = load()
    expect(await store.removeItem([0, 0, 1], 'the patch “Verse”')).toBe(true)
    expect(confirmDelete).toHaveBeenCalledWith('the patch “Verse”')
    expect(confirmAction).not.toHaveBeenCalled()
    expect(store.config.modes[0].banks[0].patches.map((p) => p.name)).toEqual(['Intro', 'Chorus'])
  })

  it('lists the actions that point at the item and removes them on confirm', async () => {
    const store = load()
    expect(await store.removeItem([0, 0, 2], 'the patch “Chorus”')).toBe(true)
    expect(confirmDelete).not.toHaveBeenCalled()
    const [message, opts] = vi.mocked(confirmAction).mock.calls[0]
    expect(message).toContain('the patch “Chorus”')
    expect(message).toContain('will be removed')
    // the button inside the same bank points at Chorus too, and is listed
    expect(opts?.details).toEqual([
      '“To chorus” (Pads > Keys > Intro): Set patch → Pads > Keys > Chorus',
      '“Back to chorus” (FX > Drive > a): Set patch → Pads > Keys > Chorus',
    ])
    expect(store.config.modes[0].banks[0].patches[0].buttons).toEqual([]) // its only action went, so did the button
    expect(store.config.modes[1].banks[0].patches[0].buttons).toEqual([])
  })

  it('changes nothing when the user declines', async () => {
    const store = load()
    vi.mocked(confirmAction).mockResolvedValueOnce(false)
    expect(await store.removeItem([0, 0, 2], 'the patch “Chorus”')).toBe(false)
    expect(store.config.modes[0].banks[0].patches).toHaveLength(3)
    expect(store.config.modes[1].banks[0].patches[0].buttons).toHaveLength(1)
  })
})

describe('moveItem', () => {
  it('moves a patch to a position and keeps every action pointing at the same patch', () => {
    const store = load()
    store.moveItem([0, 0], 2, 0) // Chorus first
    expect(store.config.modes[0].banks[0].patches.map((p) => p.name)).toEqual(['Chorus', 'Intro', 'Verse'])
    // "To chorus" lives on Intro (now index 1) and still points at Chorus (now index 0)
    expect(store.config.modes[0].banks[0].patches[1].buttons[0].actions[0].actionParameters).toEqual({ patchIndex: 0 })
    expect(store.config.modes[1].banks[0].patches[0].buttons[0].actions[0].actionParameters).toEqual({
      modeIndex: 0,
      bankIndex: 0,
      patchIndex: 0,
    })
    expect(store.dirty).toBe(true)
  })

  it('moves whole modes', () => {
    const store = load()
    store.moveItem([], 0, 1)
    expect(store.config.modes.map((m) => m.name)).toEqual(['FX', 'Pads'])
    // the FX button pointing into Pads now says mode 1
    expect(store.config.modes[0].banks[0].patches[0].buttons[0].actions[0].actionParameters).toEqual({
      modeIndex: 1,
      bankIndex: 0,
      patchIndex: 2,
    })
  })

  it('ignores moving an item to where it already is', () => {
    const store = load()
    store.moveItem([0, 0], 1, 1)
    expect(store.dirty).toBe(false)
  })
})

describe('presets', () => {
  const run = (presetId: number) => ({ type: 'ExecutePreset' as const, actionParameters: { presetId } })

  function loadWithPresets(): ReturnType<typeof useConfigStore> {
    const store = useConfigStore()
    store.config = {
      presets: [
        { id: 0, name: 'Clean', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 1 } }] },
        { id: 1, name: 'Lead', actions: [{ type: 'ProgramChange', actionParameters: { programNumber: 2 } }] },
      ],
      buttons: [
        { id: 0, name: 'Clean only', buttonMode: 'Momentary', actions: [run(0)] },
        { id: 1, name: 'Both', buttonMode: 'Momentary', actions: [run(0), run(1)] },
      ],
      modes: [{ name: 'M', banks: [{ name: 'B', patches: [{ name: 'P', buttons: [] }] }] }],
    }
    store.select([])
    return store
  }

  beforeEach(() => {
    setActivePinia(createPinia())
    vi.mocked(confirmDelete).mockClear()
    vi.mocked(confirmAction).mockClear()
    vi.mocked(promptText).mockClear()
    vi.mocked(promptText).mockResolvedValue('Chorus sound')
  })

  it('adds a preset with a free id and one starter action, and opens it', () => {
    const store = loadWithPresets()
    store.addPreset()
    expect(store.config.presets).toHaveLength(3)
    expect(store.config.presets![2].id).toBe(2)
    expect(store.config.presets![2].actions).toHaveLength(1)
    // the starter action is a Play notes with its default velocity already set
    expect(store.config.presets![2].actions[0]).toEqual({ type: 'PlayNotes', actionParameters: { velocity: 100 } })
    expect(store.parsed).toMatchObject({ level: 'preset', presetIndex: 2 })
    expect(store.selectedPreset?.name).toBe('New preset')
    expect(store.dirty).toBe(true)
  })

  it('adds the first preset to a file that has none', () => {
    const store = loadWithPresets()
    store.config.presets = undefined
    store.addPreset()
    expect(store.config.presets).toHaveLength(1)
    expect(store.config.presets![0].id).toBe(0)
  })

  it('deletes an unused preset after a plain confirmation', async () => {
    const store = loadWithPresets()
    store.config.buttons = []
    expect(await store.deletePreset(1)).toBe(true)
    expect(confirmDelete).toHaveBeenCalledWith('the preset “Lead”')
    expect(store.config.presets!.map((p) => p.name)).toEqual(['Clean'])
  })

  it('lists the buttons that run a preset in the confirmation, then removes it from them all', async () => {
    const store = loadWithPresets()
    expect(await store.deletePreset(0)).toBe(true)
    const details = vi.mocked(confirmAction).mock.calls[0][1]?.details
    expect(details).toEqual(['“Clean only” (File)', '“Both” (File)'])
    expect(store.config.presets!.map((p) => p.name)).toEqual(['Lead'])
    // "Clean only" had nothing else, so it goes; "Both" keeps its other preset
    expect(store.config.buttons!.map((b) => b.name)).toEqual(['Both'])
    expect(store.config.buttons![0].actions).toEqual([run(1)])
  })

  it('leaves everything alone when the delete is declined', async () => {
    const store = loadWithPresets()
    vi.mocked(confirmAction).mockResolvedValueOnce(false)
    expect(await store.deletePreset(0)).toBe(false)
    expect(store.config.presets).toHaveLength(2)
    expect(store.config.buttons).toHaveLength(2)
  })

  it('returns to the File pane when the open preset is deleted', async () => {
    const store = loadWithPresets()
    store.select(['presets', 1])
    await store.deletePreset(1)
    expect(store.parsed.level).toBe('file')
  })

  it('treats the Preset pane as File scope for action targets', () => {
    const store = loadWithPresets()
    store.select(['presets', 0])
    expect(store.editingLevel).toBe('file')
  })

  it('gives each new preset a name no other preset has', () => {
    const store = loadWithPresets()
    store.addPreset()
    store.addPreset()
    store.addPreset()
    expect(store.config.presets!.map((p) => p.name)).toEqual(['Clean', 'Lead', 'New preset', 'New preset 2', 'New preset 3'])
  })

  describe('creating a preset from the open button', () => {
    const note = { type: 'PlayNotes' as const, actionParameters: { noteNames: ['C3'] } }
    const cc = { type: 'ControlChange' as const, actionParameters: { controllerNumber: 7, controlValue: 100 } }

    function loadWithButton(...actions: (typeof note | typeof cc | ReturnType<typeof run>)[]) {
      const store = loadWithPresets()
      store.config.buttons!.push({ id: 5, name: 'Lead tone', buttonMode: 'Momentary', actions })
      store.select(['buttons', 2])
      return store
    }

    it('moves all the actions into a preset with the chosen name and leaves one Execute preset action', async () => {
      const store = loadWithButton(note, cc)
      expect(await store.createPresetFromSelectedButton()).toBe(true)

      const created = store.config.presets!.at(-1)!
      expect(created).toEqual({ id: 2, name: 'Chorus sound', actions: [note, cc] })
      expect(store.config.buttons![2].actions).toEqual([run(2)])
      expect(store.dirty).toBe(true)
    })

    it('asks for the name, starting from the button name, and says the actions move', async () => {
      const store = loadWithButton(note)
      await store.createPresetFromSelectedButton()
      const [message, options] = vi.mocked(promptText).mock.calls[0]
      expect(message).toMatch(/move into a new preset/)
      expect(options?.initial).toBe('Lead tone')
    })

    it('does not accept a name another preset already has, ignoring case and spaces', async () => {
      const store = loadWithButton(note)
      await store.createPresetFromSelectedButton()
      const problem = vi.mocked(promptText).mock.calls[0][1]!.problem!
      expect(problem('Clean')).toMatch(/already exists/)
      expect(problem('  lead ')).toMatch(/already exists/)
      expect(problem('Something new')).toBe('')
    })

    it('does nothing when the prompt is cancelled', async () => {
      const store = loadWithButton(note, cc)
      vi.mocked(promptText).mockResolvedValueOnce(null)
      expect(await store.createPresetFromSelectedButton()).toBe(false)
      expect(store.config.presets).toHaveLength(2)
      expect(store.config.buttons![2].actions).toEqual([note, cc])
      expect(store.dirty).toBe(false)
    })

    it('does nothing if the player moved to another button while the prompt was open', async () => {
      const store = loadWithButton(note)
      vi.mocked(promptText).mockImplementationOnce(async () => {
        store.select(['buttons', 0])
        return 'Late'
      })
      expect(await store.createPresetFromSelectedButton()).toBe(false)
      expect(store.config.presets).toHaveLength(2)
      expect(store.config.buttons![2].actions).toEqual([note])
    })

    it('does nothing if the name was taken while the prompt was open', async () => {
      const store = loadWithButton(note)
      vi.mocked(promptText).mockImplementationOnce(async () => {
        store.config.presets!.push({ id: 9, name: 'Chorus sound', actions: [note] })
        return 'Chorus sound'
      })
      expect(await store.createPresetFromSelectedButton()).toBe(false)
      expect(store.config.presets).toHaveLength(3) // only the one added behind its back
      expect(store.config.buttons![2].actions).toEqual([note])
    })

    it('is not possible for a button that already runs a preset', async () => {
      for (const actions of [[run(0)], [note, run(1)], [run(0), run(1)]]) {
        const store = loadWithButton(...actions)
        expect(await store.createPresetFromSelectedButton()).toBe(false)
        expect(promptText).not.toHaveBeenCalled()
        expect(store.config.presets).toHaveLength(2)
        expect(store.config.buttons![2].actions).toEqual(actions)
      }
    })

    it('is not possible for a button with no actions', async () => {
      const store = loadWithButton()
      expect(await store.createPresetFromSelectedButton()).toBe(false)
      expect(promptText).not.toHaveBeenCalled()
    })

    it('does nothing when no button is open', async () => {
      const store = loadWithPresets()
      store.select([])
      expect(await store.createPresetFromSelectedButton()).toBe(false)
    })

    it('works on a button at any level, e.g. inside a patch, and starts the preset list if there is none', async () => {
      const store = loadWithPresets()
      store.config.presets = undefined
      store.config.buttons = []
      store.config.modes[0].banks[0].patches[0].buttons.push({ id: 3, name: 'In a patch', buttonMode: 'Momentary', actions: [note, cc] })
      store.select(['modes', 0, 'banks', 0, 'patches', 0, 'buttons', 0])

      expect(await store.createPresetFromSelectedButton()).toBe(true)
      expect(store.config.presets).toEqual([{ id: 0, name: 'Chorus sound', actions: [note, cc] }])
      expect(store.config.modes[0].banks[0].patches[0].buttons[0].actions).toEqual([run(0)])
    })
  })
})
