// Tests for the config store: selection, pages, button moves and swaps, and editing levels.
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useConfigStore } from './config'
import { bankPath, filePath, modePath, parseSelection, patchPath } from '../lib/selection'
import { confirmState, resolveConfirm } from '../lib/confirm'

// The store boots in the FX / Stompboxes / Pedalboard patch (see initialSelection).

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('selectButtonId', () => {
  it('opens the Button pane for a configured footswitch', () => {
    const store = useConfigStore()
    store.selectButtonId(0)
    expect(store.parsed.level).toBe('button')
    expect(store.selectedButton?.name).toBe('Overdrive')
  })

  it('creates a blank button for an empty footswitch', () => {
    const store = useConfigStore()
    // The initial Pedalboard patch has every id 0-9 taken on page 1, and ids
    // 10-12 now always resolve to the built-in navigation default (see
    // builtInNavButtons() in lib/buttons.ts and the 'built-in navigation
    // ids' describe block below) rather than ever being truly empty — so
    // this moves to the FX mode itself, which defines no buttons at all.
    store.select(modePath(1))
    store.selectButtonId(3)
    expect(store.selectedButton).toBeDefined()
    expect(store.selectedButton?.id).toBe(3)
    expect(store.selectedButton?.actions).toHaveLength(0)
  })

  it('drops the blank button again when selection moves away without an action', () => {
    const store = useConfigStore()
    store.select(modePath(1))
    store.selectButtonId(3)
    store.select(['modes', 1, 'banks', 0, 'patches', 0])
    expect(store.config.modes[1].buttons?.some((b) => b.id === 3)).toBe(false)
  })

  it('keeps the new button once it has an action', () => {
    const store = useConfigStore()
    store.select(modePath(1))
    store.selectButtonId(3)
    store.mutate(() => {
      store.selectedButton?.actions.push({ type: 'ControlChange' })
    })
    store.select(['modes', 1, 'banks', 0, 'patches', 0])
    expect(store.config.modes[1].buttons?.some((b) => b.id === 3)).toBe(true)
  })

  it('re-selects an existing button rather than duplicating it', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    const before = patch.buttons.length
    store.selectButtonId(0)
    store.select(['modes', 1, 'banks', 0, 'patches', 0])
    store.selectButtonId(0)
    expect(patch.buttons.filter((b) => b.id === 0 && (b.page ?? 1) === 1)).toHaveLength(1)
    expect(patch.buttons.length).toBe(before)
  })
})

describe('page navigation', () => {
  it('reports the patch page count from the highest Button.page', () => {
    const store = useConfigStore()
    // FX / Stompboxes / Pedalboard has buttons on pages 1 and 2.
    expect(store.pageCount).toBe(2)
  })

  it('cyclePage wraps within [1, pageCount]', () => {
    const store = useConfigStore()
    expect(store.page).toBe(1)
    store.cyclePage(1)
    expect(store.page).toBe(2)
    store.cyclePage(1)
    expect(store.page).toBe(1)
    store.cyclePage(-1)
    expect(store.page).toBe(2)
  })

  it('is a no-op on a single-page patch', () => {
    const store = useConfigStore()
    store.select(['modes', 0, 'banks', 0, 'patches', 0]) // Pads / Key: F / Low
    expect(store.pageCount).toBe(1)
    store.cyclePage(1)
    expect(store.page).toBe(1)
  })

  it('resets the page when moving to a different patch', () => {
    const store = useConfigStore()
    store.cyclePage(1)
    expect(store.page).toBe(2)
    store.select(['modes', 1, 'banks', 0, 'patches', 0]) // same patch
    expect(store.page).toBe(2)
    store.select(['modes', 0, 'banks', 0, 'patches', 1]) // different patch
    expect(store.page).toBe(1)
  })

  it('keeps the page when selecting a button inside the current patch', () => {
    const store = useConfigStore()
    store.cyclePage(1)
    store.selectButtonId(0) // id 0 exists on page 2 (Fuzz)
    expect(store.page).toBe(2)
    expect(store.selectedButton?.name).toBe('Fuzz')
  })

  it('currentPage clamps a stale page into range', () => {
    const store = useConfigStore()
    store.page = 9
    expect(store.currentPage).toBe(2)
  })

  it('selectButtonAt selects by array index and brings its page into view', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    const fuzzIndex = patch.buttons.findIndex(
      (b) => b.id === 0 && b.page === 2,
    )
    store.selectButtonAt(fuzzIndex)
    expect(store.selectedButton?.name).toBe('Fuzz')
    expect(store.currentPage).toBe(2)
  })

  it('moving a button to a new page raises the page count', () => {
    const store = useConfigStore()
    store.selectButtonId(1) // Delay, id 1, page 1 (no id-1 button on page 3)
    const result = store.setButtonPage(3)
    expect(result.ok).toBe(true)
    expect(store.pageCount).toBe(3)
    expect(store.currentPage).toBe(3)
  })

  it('blocks a page move that would collide on (id, page)', () => {
    const store = useConfigStore()
    store.selectButtonId(0) // Overdrive, id 0, page 1
    const result = store.setButtonPage(2) // page 2 already has id 0 (Fuzz)
    expect(result.ok).toBe(false)
    expect(result.conflict?.name).toBe('Fuzz')
    expect(store.selectedButton?.page).toBe(1) // unchanged
    expect(store.pageCount).toBe(2)
  })
})

describe('swapButtonSlots', () => {
  // Boot patch: FX / Stompboxes / Pedalboard (modes[1].banks[0].patches[0]).
  //   index 0 = id 0 "Overdrive" page 1
  //   index 1 = id 1 "Delay"     page 1
  //   index 10 = id 0 "Fuzz"     page 2

  it('trades id and page between two buttons on the same page', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    store.swapButtonSlots(0, 1)
    expect(patch.buttons[0]).toMatchObject({ name: 'Overdrive', id: 1, page: 1 })
    expect(patch.buttons[1]).toMatchObject({ name: 'Delay', id: 0, page: 1 })
    expect(store.dirty).toBe(true)
  })

  it('moves a button to another page when swapped across pages', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    store.swapButtonSlots(0, 10) // Overdrive (id 0 / p1) <-> Fuzz (id 0 / p2)
    expect(patch.buttons[0]).toMatchObject({ name: 'Overdrive', id: 0, page: 2 })
    expect(patch.buttons[10]).toMatchObject({ name: 'Fuzz', id: 0, page: 1 })
  })

  it('is a no-op when both indices are the same', () => {
    const store = useConfigStore()
    store.swapButtonSlots(0, 0)
    expect(store.dirty).toBe(false)
  })

  it('rewrites buttonSet references so an exclusive group follows its buttons', () => {
    const store = useConfigStore()
    store.select(['modes', 0, 'banks', 0, 'patches', 0]) // Pads / Low: ids 0-4
    const patch = store.config.modes[0].banks[0].patches[0]
    // Give one button an asymmetric set that names id 0 but not id 2.
    patch.buttons[1].buttonSet = [4, 0]
    store.swapButtonSlots(0, 2) // id 0 <-> id 2
    // The "0" reference now points at the button that took id 2; kept sorted.
    expect(patch.buttons[1].buttonSet).toEqual([2, 4])
  })

  it('remaps the swapped buttons’ own buttonSet arrays', () => {
    const store = useConfigStore()
    store.select(['modes', 0, 'banks', 0, 'patches', 0])
    const patch = store.config.modes[0].banks[0].patches[0]
    patch.buttons[0].buttonSet = [1, 2] // id-0 button cancels ids 1 and 2
    store.swapButtonSlots(0, 1) // id 0 <-> id 1
    // buttons[0] now holds id 1; its "1" self-reference becomes "0".
    expect(patch.buttons[0].buttonSet).toEqual([0, 2])
  })

  it('leaves buttonSet arrays untouched on a pure page swap', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0] // Pedalboard
    patch.buttons[1].buttonSet = [0] // references id 0
    store.swapButtonSlots(0, 10) // both id 0, pages 1 and 2 — no id change
    expect(patch.buttons[1].buttonSet).toEqual([0])
  })

  it('leaves page implicit when a patch omits it and the value stays 1', () => {
    const store = useConfigStore()
    store.select(['modes', 0, 'banks', 0, 'patches', 0]) // Pads, single page
    const patch = store.config.modes[0].banks[0].patches[0]
    const hadPage = patch.buttons.slice(0, 2).some((b) => 'page' in b)
    store.swapButtonSlots(0, 1)
    if (!hadPage) {
      expect('page' in patch.buttons[0]).toBe(false)
      expect('page' in patch.buttons[1]).toBe(false)
    }
    expect(patch.buttons[0].id).not.toBe(patch.buttons[1].id)
  })
})

describe('swapFootswitchButtons', () => {
  // Boot patch Pedalboard, page 1: ids 0-9 bound, ids 10-12 empty.
  // Page 2 carries id 0 "Fuzz".

  it('swaps the buttons on two footswitches of the current page', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    store.swapFootswitchButtons(0, 1)
    expect(patch.buttons.find((b) => b.name === 'Overdrive')).toMatchObject({
      id: 1,
      page: 1,
    })
    expect(patch.buttons.find((b) => b.name === 'Delay')).toMatchObject({
      id: 0,
      page: 1,
    })
    expect(store.dirty).toBe(true)
  })

  it('moves the button onto an empty footswitch, leaving the source empty', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    store.swapFootswitchButtons(0, 10) // id 10 is unbound on page 1
    expect(patch.buttons.find((b) => b.name === 'Overdrive')?.id).toBe(10)
    expect(patch.buttons.some((b) => b.id === 0 && (b.page ?? 1) === 1)).toBe(
      false,
    )
  })

  it('is a no-op when both footswitches are empty', () => {
    const store = useConfigStore()
    store.swapFootswitchButtons(10, 11)
    expect(store.dirty).toBe(false)
  })

  it('leaves buttons on other pages alone', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    store.swapFootswitchButtons(0, 1) // page 1
    expect(patch.buttons.find((b) => b.name === 'Fuzz')).toMatchObject({
      id: 0,
      page: 2,
    })
  })

  it('rewires buttonSet references through the id swap', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    patch.buttons[1].buttonSet = [0, 5] // Delay names id 0 and id 5
    store.swapFootswitchButtons(0, 2) // id 0 <-> id 2
    expect(patch.buttons[1].buttonSet).toEqual([2, 5])
  })
})

describe('button set editing', () => {
  const PADS_LOW = ['modes', 0, 'banks', 0, 'patches', 0]

  function openExclusiveButton() {
    const store = useConfigStore()
    store.select(PADS_LOW)
    store.selectButtonId(0) // "Lo F", Exclusive, buttonSet [0,1,2,3,4]
    return store
  }

  it('starts a draft from the button’s current set', () => {
    const store = openExclusiveButton()
    expect(store.selectedButton?.buttonMode).toBe('Exclusive')
    store.toggleButtonSetEditing()
    expect(store.buttonSetEditing).toBe(true)
    expect(store.buttonSetDraft).toEqual([0, 1, 2, 3, 4])
  })

  it('pressFootswitch toggles draft membership without touching the button', () => {
    const store = openExclusiveButton()
    store.toggleButtonSetEditing()
    store.pressFootswitch(1) // remove
    store.pressFootswitch(7) // add
    expect(store.buttonSetDraft).toEqual([0, 2, 3, 4, 7])
    expect(store.selectedButton?.buttonSet).toEqual([0, 1, 2, 3, 4])
  })

  it('Save commits the draft onto the button and ends the mode', () => {
    const store = openExclusiveButton()
    store.toggleButtonSetEditing()
    store.pressFootswitch(1)
    store.pressFootswitch(7)
    store.toggleButtonSetEditing() // "Save"
    expect(store.buttonSetEditing).toBe(false)
    expect(store.buttonSetDraft).toBeNull()
    expect(store.selectedButton?.buttonSet).toEqual([0, 2, 3, 4, 7])
    expect(store.dirty).toBe(true)
  })

  it('the button itself can never be removed from its own set', () => {
    const store = openExclusiveButton()
    store.toggleButtonSetEditing()
    for (const id of [0, 1, 2, 3, 4]) store.pressFootswitch(id)
    expect(store.buttonSetDraft).toEqual([0])
    store.toggleButtonSetEditing()
    expect(store.selectedButton?.buttonSet).toEqual([0])
  })

  it('navigating away discards the draft', () => {
    const store = openExclusiveButton()
    store.toggleButtonSetEditing()
    store.pressFootswitch(9)
    store.select(PADS_LOW)
    expect(store.buttonSetEditing).toBe(false)
    expect(store.config.modes[0].banks[0].patches[0].buttons[0].buttonSet).toEqual(
      [0, 1, 2, 3, 4],
    )
  })

  it('does nothing for a non-Exclusive button', () => {
    const store = useConfigStore()
    store.selectButtonId(0) // FX Pedalboard "Overdrive", Latched
    expect(store.selectedButton?.buttonMode).toBe('Latched')
    store.toggleButtonSetEditing()
    expect(store.buttonSetEditing).toBe(false)
  })

  it('outside set mode pressFootswitch still selects', () => {
    const store = useConfigStore()
    store.pressFootswitch(1)
    expect(store.parsed.level).toBe('button')
    expect(store.selectedButton?.name).toBe('Delay')
  })
})

describe('json getter', () => {
  it('omits cosmetic-only buttons from the output', () => {
    const store = useConfigStore()
    store.selectButtonId(12) // creates a blank button, left action-less
    const out = JSON.parse(store.json)
    const patch = out.modes[1].banks[0].patches[0]
    expect(patch.buttons.some((b: { id: number }) => b.id === 12)).toBe(false)
  })

  it('writes the On and Off value of every control change, using the defaults where a file left them out', () => {
    const store = useConfigStore()
    // the sample's Overdrive button is a control change that has only a controller number
    const overdrive = store.config.modes[1].banks[0].patches[0].buttons.find((b) => b.name === 'Overdrive')!
    expect(overdrive.actions[0].actionParameters).toEqual({ controllerNumber: 102 })

    const out = JSON.parse(store.json)
    const saved = out.modes[1].banks[0].patches[0].buttons.find((b: { name: string }) => b.name === 'Overdrive')
    expect(saved.actions[0].actionParameters).toEqual({ controllerNumber: 102, controlValue: 127, releaseValue: 0 })

    // every control change in the whole file has both
    const ccParams: Record<string, unknown>[] = []
    const walk = (value: unknown) => {
      if (Array.isArray(value)) value.forEach(walk)
      else if (value && typeof value === 'object') {
        const o = value as Record<string, unknown>
        if (o.type === 'ControlChange') ccParams.push((o.actionParameters ?? {}) as Record<string, unknown>)
        Object.values(o).forEach(walk)
      }
    }
    walk(out)
    expect(ccParams.length).toBeGreaterThan(0)
    for (const p of ccParams) {
      expect(typeof p.controlValue).toBe('number')
      expect(typeof p.releaseValue).toBe('number')
    }
  })

  it('writes a velocity for every Play notes action, 100 where a file left it out, and keeps the ones that are set', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    patch.buttons[0].actions = [{ type: 'PlayNotes', actionParameters: { noteNames: ['C3'] } }]
    patch.buttons[1].actions = [{ type: 'PlayNotes', actionParameters: { noteNames: ['D3'], velocity: 64 } }]
    patch.buttons[2].actions = [{ type: 'PlayNotes', actionParameters: { noteNames: ['E3'], velocity: 0 } }]

    const out = JSON.parse(store.json).modes[1].banks[0].patches[0].buttons
    expect(out[0].actions[0].actionParameters.velocity).toBe(100)
    expect(out[1].actions[0].actionParameters.velocity).toBe(64)
    expect(out[2].actions[0].actionParameters.velocity).toBe(0)
    // the config being edited is not changed by saving
    expect(patch.buttons[0].actions[0].actionParameters).toEqual({ noteNames: ['C3'] })
  })

  it('keeps the values that are set, and does not change the config being edited', () => {
    const store = useConfigStore()
    const patch = store.config.modes[1].banks[0].patches[0]
    patch.buttons[0].actions = [{ type: 'ControlChange', actionParameters: { controllerNumber: 7, controlValue: 64, releaseValue: 10 } }]
    patch.buttons[1].actions = [{ type: 'ControlChange', actionParameters: { controllerNumber: 8 } }]

    const out = JSON.parse(store.json).modes[1].banks[0].patches[0].buttons
    expect(out[0].actions[0].actionParameters).toEqual({ controllerNumber: 7, controlValue: 64, releaseValue: 10 })
    expect(out[1].actions[0].actionParameters).toEqual({ controllerNumber: 8, controlValue: 127, releaseValue: 0 })
    // saving never touches the live config
    expect(patch.buttons[1].actions[0].actionParameters).toEqual({ controllerNumber: 8 })
  })
})

describe('parseSelection round-trip', () => {
  it('reads the button index back out of the path', () => {
    const store = useConfigStore()
    store.selectButtonId(2)
    const parsed = parseSelection(store.selection)
    expect(parsed).toMatchObject({ level: 'button', modeIndex: 1, bankIndex: 0, patchIndex: 0 })
    expect(store.config.modes[1].banks[0].patches[0].buttons[parsed.buttonIndex!].id).toBe(2)
  })
})

// Mode/Bank/File button editing — see activateButtonAtLevel() in the store.
// Fixture reminder: Pads/Key:F bank (modes[0].banks[0]) defines its own
// buttons for ids 5-8; its "Low" patch (patches[0]) defines its own for ids
// 0-4, with no overlap between the two, and FX/Stompboxes (modes[1].banks[0])
// defines none at Bank or Mode level.
beforeEach(() => {
  resolveConfirm(false)
  confirmState.open = false
})

describe('editingLevel', () => {
  it('matches the active pane', () => {
    const store = useConfigStore()
    store.select(filePath())
    expect(store.editingLevel).toBe('file')
    store.select(modePath(0))
    expect(store.editingLevel).toBe('mode')
    store.select(bankPath(0, 0))
    expect(store.editingLevel).toBe('bank')
    store.select(patchPath(0, 0, 0))
    expect(store.editingLevel).toBe('patch')
  })

  it('while a Button is open, stays scoped to the level that button belongs to', () => {
    const store = useConfigStore()
    store.select(bankPath(0, 0))
    store.selectButtonId(5) // existing own Bank button ("Low")
    expect(store.parsed.buttonOwnerLevel).toBe('bank')
    expect(store.editingLevel).toBe('bank')
  })
})

describe('activateButtonAtLevel: own button', () => {
  it('opens an existing own button without asking to confirm anything', () => {
    const store = useConfigStore()
    store.select(bankPath(0, 0))
    store.activateButtonAtLevel('bank', 5)
    expect(confirmState.open).toBe(false)
    expect(store.parsed).toMatchObject({ level: 'button', buttonOwnerLevel: 'bank' })
    expect(store.selectedButton?.name).toBe('Low')
  })

  it('creates a blank own button at the level browsed, when nothing defines the id anywhere', () => {
    const store = useConfigStore()
    store.select(modePath(1)) // FX mode — no Mode or Bank buttons at all
    store.activateButtonAtLevel('mode', 3)
    expect(confirmState.open).toBe(false)
    expect(store.parsed).toMatchObject({ level: 'button', buttonOwnerLevel: 'mode', modeIndex: 1 })
    expect(store.selectedButton).toMatchObject({ id: 3, actions: [] })
    expect(store.config.modes[1].buttons).toHaveLength(1)
  })
})

describe('activateButtonAtLevel: overriding an inherited button', () => {
  it('asks to confirm, and does nothing on cancel', async () => {
    const store = useConfigStore()
    store.select(patchPath(0, 0, 0)) // "Low" patch — doesn't define id 5, its Bank does
    const pending = store.activateButtonAtLevel('patch', 5)
    expect(confirmState.open).toBe(true)
    resolveConfirm(false)
    await pending
    expect(store.parsed.level).toBe('patch') // selection never moved
    expect(store.config.modes[0].banks[0].patches[0].buttons.some((b) => b.id === 5)).toBe(false)
  })

  it('copies the inherited button down and selects it, on confirm', async () => {
    const store = useConfigStore()
    store.select(patchPath(0, 0, 0))
    const pending = store.activateButtonAtLevel('patch', 5)
    resolveConfirm(true)
    await pending
    expect(store.parsed).toMatchObject({ level: 'button', buttonOwnerLevel: 'patch' })
    expect(store.selectedButton).toMatchObject({ id: 5, name: 'Low', page: 1 })
    expect(store.dirty).toBe(true)
    // The Bank's own button is untouched — this is a copy, not a move.
    expect(store.config.modes[0].banks[0].buttons?.find((b) => b.id === 5)?.name).toBe('Low')
  })
})

// ids 10-12 (printed 11-13) never truly "have nothing defined" - they fall
// back to the built-in navigation default (builtInNavButtons() in
// lib/buttons.ts) instead, so activating one always goes through the
// override-confirmation flow, with the stronger nav-specific wording, rather
// than ever creating a blank button.
describe('activateButtonAtLevel: built-in navigation ids (10-12)', () => {
  it('asks to confirm with the built-in role named, and does nothing on cancel', async () => {
    const store = useConfigStore()
    store.select(patchPath(0, 0, 0))
    const pending = store.activateButtonAtLevel('patch', 11)
    expect(confirmState.open).toBe(true)
    expect(confirmState.message).toContain('built in')
    resolveConfirm(false)
    await pending
    expect(store.parsed.level).toBe('patch')
    expect(store.config.modes[0].banks[0].patches[0].buttons.some((b) => b.id === 11)).toBe(false)
  })

  it('copies the built-in default down and selects it, on confirm', async () => {
    const store = useConfigStore()
    store.select(patchPath(0, 0, 0))
    const pending = store.activateButtonAtLevel('patch', 11)
    resolveConfirm(true)
    await pending
    expect(store.parsed).toMatchObject({ level: 'button', buttonOwnerLevel: 'patch' })
    expect(store.selectedButton).toMatchObject({
      id: 11,
      name: 'Cycle Parameter Down',
      actions: [{ type: 'CycleParameterValue', actionParameters: { direction: 'Down' } }],
    })
  })

  it('never creates a blank button for ids 10-12', () => {
    const store = useConfigStore()
    store.select(modePath(1)) // FX mode — no Mode or Bank buttons at all
    store.activateButtonAtLevel('mode', 10)
    // Unlike an ordinary unconfigured id, this awaits confirmation instead —
    // nothing is created synchronously, and never a blank/actionless button.
    expect(store.config.modes[1].buttons ?? []).toHaveLength(0)
  })
})

describe("buttonEditor: a level's own vs. inherited buttons", () => {
  it('separates the Bank\'s own buttons from what it inherits', () => {
    const store = useConfigStore()
    store.select(bankPath(0, 0))
    const { own, inherited } = store.buttonEditor('bank')
    expect(own.map((e) => e.button.name).sort()).toEqual(['High', 'Low', 'Middle', 'Wide'])
    // This Mode/File define no buttons of their own. The built-in navigation
    // defaults for ids 10-12 are deliberately NOT listed here - they need no
    // configuration, so inheritedButtons() excludes them (see lib/buttons.ts).
    expect(inherited).toEqual([])
  })
})

describe('deleteSelectedButton at every level', () => {
  it('deletes a Mode-owned button and returns to the Mode pane', () => {
    const store = useConfigStore()
    store.select(modePath(1))
    store.activateButtonAtLevel('mode', 3)
    store.mutate(() => {
      store.selectedButton?.actions.push({ type: 'ControlChange' })
    })
    store.deleteSelectedButton()
    expect(store.parsed).toEqual({ level: 'mode', modeIndex: 1 })
    expect(store.config.modes[1].buttons).toHaveLength(0)
  })

  it('deletes a Bank-owned button and returns to the Bank pane', () => {
    const store = useConfigStore()
    store.select(bankPath(0, 0))
    store.activateButtonAtLevel('bank', 5) // existing "Low" bank button
    store.deleteSelectedButton()
    expect(store.parsed).toEqual({ level: 'bank', modeIndex: 0, bankIndex: 0 })
    expect(store.config.modes[0].banks[0].buttons?.some((b) => b.id === 5)).toBe(false)
  })
})

describe('dropDraftButtons at Mode/Bank/File levels', () => {
  it('drops a cosmetic Mode-level button on the way out, keeps one with an action', () => {
    const store = useConfigStore()
    store.select(modePath(1))
    store.activateButtonAtLevel('mode', 3) // blank, cosmetic
    store.select(filePath())
    expect(store.config.modes[1].buttons ?? []).toHaveLength(0)
  })
})

// Used when switching device profile — see stores/device.ts /
// ChangeDeviceLink.vue and the editor-device-switching-is-destructive
// decision: a different device has nothing to do with this one, so every
// button is reset, not just the ones that no longer fit.
describe('clearAllButtons', () => {
  it('empties every buttons array at every level, keeping names/structure intact', () => {
    const store = useConfigStore()
    // The fixture only has Bank/Patch-level buttons; give it File and
    // Mode-level ones too so all four levels are genuinely exercised.
    store.config.buttons = [{ id: 0, name: 'File button', buttonMode: 'Momentary', actions: [{ type: 'ControlChange' }] }]
    store.config.modes[0].buttons = [{ id: 1, name: 'Mode button', buttonMode: 'Momentary', actions: [{ type: 'ControlChange' }] }]
    expect(store.config.modes[0].banks[0].buttons?.length).toBeGreaterThan(0)
    expect(store.config.modes[0].banks[0].patches[0].buttons.length).toBeGreaterThan(0)

    store.clearAllButtons()

    expect(store.config.buttons).toEqual([])
    expect(store.config.modes[0].buttons).toEqual([])
    expect(store.config.modes[0].banks[0].buttons).toEqual([])
    expect(store.config.modes[0].banks[0].patches[0].buttons).toEqual([])
    // Names/structure survive untouched.
    expect(store.config.modes[0].name).toBe('Pads')
    expect(store.config.modes[0].banks[0].name).toBe('Key: F')
    expect(store.config.modes[0].banks[0].patches[0].name).toBe('Low')
  })

  it('drops the selection back to the File level and marks the config dirty', () => {
    const store = useConfigStore()
    store.select(patchPath(0, 0, 0))
    store.clearAllButtons()
    expect(store.parsed.level).toBe('file')
    expect(store.dirty).toBe(true)
  })
})
