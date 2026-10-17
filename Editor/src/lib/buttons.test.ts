// Tests for lib/buttons.ts: page filtering, id lookup, built-in navigation buttons and cascade resolution.
import { describe, expect, it } from 'vitest'
import {
  BUTTON_LEVEL_CASCADE,
  buttonOnPage,
  buttonsContainerAt,
  builtInNavButtons,
  findButtonIndexById,
  findPageConflict,
  inheritedButtons,
  isCosmeticButton,
  navButtonDefaultLabel,
  navButtonDefaultRole,
  pruneCosmeticButtons,
  resolveButtonFrom,
  rgbCss,
} from './buttons'
import { liveRigDevice } from '../data/liveRigDevice'
import type { Button, DeviceConfig } from '../types/config'
import type { DeviceFile } from '../types/device'

const btn = (over: Partial<Button>): Button => ({
  id: 0,
  name: '',
  buttonMode: 'Momentary',
  actions: [],
  ...over,
})

// liveRigDevice's navigation is {select: 10, down: 11, up: 12} — the same ids
// the reference hardware has always used, so these tests read the same as
// before the device profile existed. A second fixture with different ids
// (below) proves the derivation is real, not just a renamed constant.
const DEVICE = liveRigDevice

describe('buttonOnPage', () => {
  it('treats a missing page as page 1', () => {
    expect(buttonOnPage(btn({}), 1)).toBe(true)
    expect(buttonOnPage(btn({}), 2)).toBe(false)
  })

  it('matches an explicit page', () => {
    expect(buttonOnPage(btn({ page: 2 }), 2)).toBe(true)
    expect(buttonOnPage(btn({ page: 2 }), 1)).toBe(false)
  })

  it('page 0 shows on every page', () => {
    expect(buttonOnPage(btn({ page: 0 }), 1)).toBe(true)
    expect(buttonOnPage(btn({ page: 0 }), 7)).toBe(true)
  })
})

describe('findButtonIndexById', () => {
  const buttons = [
    btn({ id: 0, page: 1 }),
    btn({ id: 1, page: 1 }),
    btn({ id: 0, page: 2 }),
  ]

  it('finds the button for an id on the current page', () => {
    expect(findButtonIndexById(buttons, 0, 1)).toBe(0)
    expect(findButtonIndexById(buttons, 0, 2)).toBe(2)
  })

  it('returns -1 when the id is unbound on that page', () => {
    expect(findButtonIndexById(buttons, 5, 1)).toBe(-1)
    expect(findButtonIndexById(buttons, 1, 2)).toBe(-1)
  })
})

describe('rgbCss', () => {
  it('renders an [r,g,b] triple', () => {
    expect(rgbCss([24, 250, 24])).toBe('rgb(24, 250, 24)')
  })

  it('falls back when the colour is missing or malformed', () => {
    expect(rgbCss(undefined)).toBe('#333333')
    expect(rgbCss([1, 2], 'red')).toBe('red')
  })
})

describe('findPageConflict', () => {
  const buttons = [
    btn({ id: 0, page: 1 }), // 0 — the one being moved
    btn({ id: 0, page: 2, name: 'Fuzz' }), // 1
    btn({ id: 5, page: 2 }), // 2
    btn({ id: 3, page: 0, name: 'Always' }), // 3
  ]

  it('reports the button already holding that id on the target page', () => {
    expect(findPageConflict(buttons, 0, 2)?.name).toBe('Fuzz')
  })

  it('allows a move to a free page', () => {
    expect(findPageConflict(buttons, 0, 3)).toBeUndefined()
  })

  it('never conflicts with itself', () => {
    expect(findPageConflict(buttons, 0, 1)).toBeUndefined()
  })

  it('treats a page-0 sibling as occupying every page', () => {
    expect(findPageConflict(buttons, 0, 2)).toBeDefined() // unrelated: id 0
    const withEvery = [btn({ id: 3, page: 1 }), buttons[3]]
    expect(findPageConflict(withEvery, 0, 5)?.name).toBe('Always')
  })
})

describe('builtInNavButtons / navButtonDefaultRole / navButtonDefaultLabel', () => {
  it('describes the built-in role for the active device’s three nav ids', () => {
    expect(navButtonDefaultRole(DEVICE, 10)).toMatch(/parameter/)
    expect(navButtonDefaultRole(DEVICE, 11)).toMatch(/down/)
    expect(navButtonDefaultRole(DEVICE, 12)).toMatch(/up/)
  })

  it('has no default role for a normal performance button', () => {
    expect(navButtonDefaultRole(DEVICE, 0)).toBeUndefined()
    expect(navButtonDefaultRole(DEVICE, 9)).toBeUndefined()
  })

  it('labels match the printed footswitch text', () => {
    expect(navButtonDefaultLabel(DEVICE, 10)).toBe('Param')
    expect(navButtonDefaultLabel(DEVICE, 11)).toBe('<')
    expect(navButtonDefaultLabel(DEVICE, 12)).toBe('>')
    expect(navButtonDefaultLabel(DEVICE, 0)).toBeUndefined()
  })

  it('is genuinely derived from the device profile, not hardcoded to 10-12', () => {
    const odd: DeviceFile = { ...DEVICE, navigation: { select: 8, down: 9, up: 1 } }
    expect(navButtonDefaultRole(odd, 8)).toMatch(/parameter/)
    expect(navButtonDefaultRole(odd, 9)).toMatch(/down/)
    expect(navButtonDefaultRole(odd, 1)).toMatch(/up/)
    // The reference device's own nav ids carry no special role on a device
    // that assigns them elsewhere.
    expect(navButtonDefaultRole(odd, 10)).toBeUndefined()
    expect(navButtonDefaultLabel(odd, 1)).toBe('>')

    const buttons = builtInNavButtons(odd)
    expect(Object.keys(buttons).map(Number).sort()).toEqual([1, 8, 9])
    expect(buttons[8]).toMatchObject({ id: 8, name: 'Cycle/Activate Parameter' })
  })
})

describe('pruneCosmeticButtons', () => {
  const config = (): DeviceConfig => ({
    modes: [
      {
        name: 'M',
        banks: [
          {
            name: 'B',
            patches: [
              {
                name: 'P',
                buttons: [
                  btn({ id: 0, name: 'real', actions: [{ type: 'ControlChange' }] }),
                  btn({ id: 1, name: 'cosmetic' }),
                ],
              },
            ],
          },
        ],
      },
    ],
  })

  it('drops buttons with no actions', () => {
    const pruned = pruneCosmeticButtons(config())
    const buttons = pruned.modes[0].banks[0].patches[0].buttons
    expect(buttons).toHaveLength(1)
    expect(buttons[0].name).toBe('real')
  })

  it('does not mutate the input', () => {
    const input = config()
    pruneCosmeticButtons(input)
    expect(input.modes[0].banks[0].patches[0].buttons).toHaveLength(2)
  })

  it('isCosmeticButton flags the action-less button', () => {
    expect(isCosmeticButton(btn({}))).toBe(true)
    expect(isCosmeticButton(btn({ actions: [{ type: 'PlayNotes' }] }))).toBe(false)
  })
})

// A config with a button for id 0 defined at every level, so cascade tests can
// tell exactly which level's button won.
function cascadeConfig(): DeviceConfig {
  return {
    buttons: [btn({ id: 0, name: 'File 0' }), btn({ id: 9, name: 'File-only 9' })],
    modes: [
      {
        name: 'M',
        buttons: [btn({ id: 0, name: 'Mode 0' }), btn({ id: 8, name: 'Mode-only 8' })],
        banks: [
          {
            name: 'B',
            buttons: [btn({ id: 0, name: 'Bank 0' }), btn({ id: 7, name: 'Bank-only 7' })],
            patches: [
              {
                name: 'P1',
                buttons: [
                  btn({ id: 0, name: 'Patch 0 p1', page: 1 }),
                  btn({ id: 6, name: 'Patch-only 6 p2', page: 2 }),
                ],
              },
            ],
          },
        ],
      },
    ],
  }
}

const AT = { modeIndex: 0, bankIndex: 0, patchIndex: 0 }

describe('buttonsContainerAt', () => {
  it("returns each level's own array", () => {
    const config = cascadeConfig()
    expect(buttonsContainerAt(config, 'file', AT)?.map((b) => b.name)).toEqual(['File 0', 'File-only 9'])
    expect(buttonsContainerAt(config, 'mode', AT)?.map((b) => b.name)).toEqual(['Mode 0', 'Mode-only 8'])
    expect(buttonsContainerAt(config, 'bank', AT)?.map((b) => b.name)).toEqual(['Bank 0', 'Bank-only 7'])
    expect(buttonsContainerAt(config, 'patch', AT)?.map((b) => b.name)).toHaveLength(2)
  })

  it('returns undefined when the level is unreachable from `at`', () => {
    const config = cascadeConfig()
    expect(buttonsContainerAt(config, 'bank', {})).toBeUndefined()
    expect(buttonsContainerAt(config, 'patch', { modeIndex: 0 })).toBeUndefined()
  })
})

describe('resolveButtonFrom', () => {
  it('applies Patch > Bank > Mode > Config override order', () => {
    const config = cascadeConfig()
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 0, 1)).toMatchObject({
      level: 'patch',
      button: { name: 'Patch 0 p1' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'bank', AT, 0, 1)).toMatchObject({
      level: 'bank',
      button: { name: 'Bank 0' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'mode', AT, 0, 1)).toMatchObject({
      level: 'mode',
      button: { name: 'Mode 0' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'file', AT, 0, 1)).toMatchObject({
      level: 'file',
      button: { name: 'File 0' },
    })
  })

  it('only page-filters the Patch level — Bank/Mode/Config always apply', () => {
    const config = cascadeConfig()
    // id 0's Patch button is page 1 only; on page 2 it falls through to Bank.
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 0, 2)).toMatchObject({
      level: 'bank',
      button: { name: 'Bank 0' },
    })
    // id 7 is Bank-only — resolves the same on every page.
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 7, 1)?.level).toBe('bank')
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 7, 99)?.level).toBe('bank')
  })

  it('returns undefined when nothing defines the id anywhere in the chain', () => {
    const config = cascadeConfig()
    // Not a nav id: those always fall back to the built-in navigation default
    // (see the 'built-in navigation fallback' describe block below) rather
    // than ever being truly undefined.
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 4, 1)).toBeUndefined()
  })

  it('starting from a less-specific level skips the more-specific ones', () => {
    const config = cascadeConfig()
    // Starting at Bank never sees the Patch-level override for id 0.
    expect(resolveButtonFrom(config, DEVICE, 'bank', AT, 0, 1)).toMatchObject({ level: 'bank' })
    // File-only id 9 is invisible when starting from Mode, since File is below it too — it still resolves.
    expect(resolveButtonFrom(config, DEVICE, 'mode', AT, 9, 1)).toMatchObject({ level: 'file' })
  })
})

describe('inheritedButtons', () => {
  it('lists ids a level does not itself define, from the nearest ancestor that does', () => {
    const config = cascadeConfig()
    const forBank = inheritedButtons(config, 'bank', AT)
    // Bank defines 0 and 7 itself, so those are excluded; 8 (Mode) and 9
    // (File) are inherited. The built-in navigation defaults are NOT listed
    // here - nothing configures them, so there's nothing to surface in this
    // "still needs overriding" section (see resolveButtonFrom() for where the
    // built-in fallback still applies).
    expect(forBank.map((r) => r.button.name)).toEqual(['Mode-only 8', 'File-only 9'])
    expect(forBank.every((r) => r.level !== 'bank')).toBe(true)
  })

  it('a more-specific ancestor shadows a less-specific one for the same id', () => {
    const config: DeviceConfig = {
      buttons: [btn({ id: 3, name: 'File 3' })],
      modes: [{ name: 'M', buttons: [btn({ id: 3, name: 'Mode 3' })], banks: [{ name: 'B', patches: [{ name: 'P', buttons: [] }] }] }],
    }
    const forBank = inheritedButtons(config, 'bank', { modeIndex: 0, bankIndex: 0 })
    expect(forBank).toHaveLength(1)
    expect(forBank[0]).toMatchObject({ level: 'mode', button: { name: 'Mode 3' } })
  })

  it('is page-aware for the Patch level: an id overridden on one page is still inherited on another', () => {
    const config = cascadeConfig()
    // id 0's Patch override only applies on page 1.
    expect(inheritedButtons(config, 'patch', AT, 1).some((r) => r.button.id === 0)).toBe(false)
    expect(inheritedButtons(config, 'patch', AT, 2).some((r) => r.button.id === 0)).toBe(true)
  })

  it('the File level has no ancestors to inherit from', () => {
    const config = cascadeConfig()
    expect(inheritedButtons(config, 'file', AT)).toEqual([])
  })

  it('does not list the built-in navigation defaults (ids 10-12) even when nothing overrides them', () => {
    const config = cascadeConfig()
    for (const level of BUTTON_LEVEL_CASCADE) {
      const ids = inheritedButtons(config, level, AT).map((r) => r.button.id)
      expect(ids).not.toEqual(expect.arrayContaining([10, 11, 12]))
    }
  })

  it('still lists a REAL override of a nav id (10-12), same as any other id', () => {
    const config: DeviceConfig = {
      buttons: [btn({ id: 11, name: 'Custom 11' })],
      modes: [{ name: 'M', buttons: [], banks: [{ name: 'B', patches: [{ name: 'P', buttons: [] }] }] }],
    }
    const forBank = inheritedButtons(config, 'bank', { modeIndex: 0, bankIndex: 0 })
    expect(forBank).toEqual([{ level: 'file', button: { id: 11, name: 'Custom 11', buttonMode: 'Momentary', actions: [] }, index: 0 }])
  })
})

describe('built-in navigation fallback (ids 10-12)', () => {
  it('resolveButtonFrom falls back to the built-in default when nothing overrides it', () => {
    const config = cascadeConfig()
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 10, 1)).toMatchObject({
      level: 'builtin',
      button: { name: 'Cycle/Activate Parameter' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 11, 1)).toMatchObject({
      level: 'builtin',
      button: { name: 'Cycle Parameter Down' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 12, 1)).toMatchObject({
      level: 'builtin',
      button: { name: 'Cycle Parameter Up' },
    })
  })

  it('an explicit override anywhere in the chain still wins over the built-in default', () => {
    const config: DeviceConfig = {
      buttons: [btn({ id: 11, name: 'Custom 11' })],
      modes: [{ name: 'M', buttons: [], banks: [{ name: 'B', patches: [{ name: 'P', buttons: [] }] }] }],
    }
    expect(
      resolveButtonFrom(config, DEVICE, 'patch', { modeIndex: 0, bankIndex: 0, patchIndex: 0 }, 11, 1),
    ).toMatchObject({ level: 'file', button: { name: 'Custom 11' } })
  })

  it('falls back to a DIFFERENT id on a device with different navigation ids', () => {
    const odd: DeviceFile = { ...DEVICE, navigation: { select: 8, down: 9, up: 1 } }
    const config = cascadeConfig()
    // id 8 is Mode-only on cascadeConfig(); that real override still wins.
    expect(resolveButtonFrom(config, odd, 'patch', AT, 8, 1)).toMatchObject({ level: 'mode' })
    // id 1 has nothing defined anywhere, so it falls back to the built-in
    // 'up' role on this device — it would NOT on the reference device.
    expect(resolveButtonFrom(config, odd, 'patch', AT, 1, 1)).toMatchObject({
      level: 'builtin',
      button: { name: 'Cycle Parameter Up' },
    })
    expect(resolveButtonFrom(config, DEVICE, 'patch', AT, 1, 1)).toBeUndefined()
  })
})
