// Tests for the design logic: presets, spacing and blocking checks, numbering, the on-screen grid, device.json and the exports.
import { describe, expect, it } from 'vitest'
import { analyse, stepSeverity } from './checks'
import { parseDevice, serialiseDevice } from './deviceFile'
import { buildArtifacts, buildZip, zipEntries } from './export/artifacts'
import { computeIds, mirrorGrid, performanceButtons, quickGridPositions, suggestScreen } from './layout'
import { SCREENS } from './catalog'
import { formatPinRuns } from './pins'
import { buildPreset, PRESETS } from './presets'
import { spacingViolations } from './spacing'
import { buttonDiameter } from './geometry'
import Ajv from 'ajv'
import profileSchema from '../generated/device-schema.json'
import liveRigProfile from '../../../config/live-rig-device.json'
import { buildDeviceProfile } from './export/deviceProfile'
import type { Design } from '../types/device'

const live = () => buildPreset('live-rig')

function moveButtonTo(design: Design, id: number, x: number, y: number) {
  const b = design.buttons.find((button) => button.id === id)!
  b.x = x
  b.y = y
}

describe('presets', () => {
  it.each(PRESETS.map((p) => p.id))('%s is a valid, exportable design', (id) => {
    const design = buildPreset(id)
    const analysis = analyse(design)
    expect(analysis.errors, analysis.checks.filter((c) => c.severity === 'error').map((c) => c.text).join('; ')).toBe(0)
    expect(performanceButtons(design).length).toBeGreaterThanOrEqual(3)
  })

  it('the current unit keeps its familiar ids and pins', () => {
    const design = live()
    const analysis = analyse(design)
    const byId = [...design.buttons].sort((a, b) => a.id - b.id)
    expect(design.buttons).toHaveLength(13)
    expect(design.buttons.map((b) => b.id).sort((a, b) => a - b)).toEqual([...Array(13).keys()])
    // navigation is the top row, ids 10-12
    expect([design.nav.select, design.nav.down, design.nav.up].map((uid) => design.buttons.find((b) => b.uid === uid)!.id)).toEqual([10, 11, 12])
    // and the button pins match Pins.h: 33-41, 15 for the grid, 21-23 for navigation
    const pins = byId.map((b) => analysis.plan.buttonPins.get(b.uid))
    expect(pins).toEqual([33, 34, 35, 36, 37, 38, 39, 40, 41, 15, 21, 22, 23])
  })
})

describe('spacing guidance', () => {
  it('needs 50 mm between centres for foot control, and only warns', () => {
    const design = live()
    expect(spacingViolations(design)).toHaveLength(0) // the preset uses 60 mm centres
    moveButtonTo(design, 1, 80, 160) // 50 mm from id 0: exactly enough
    expect(spacingViolations(design)).toHaveLength(0)
    moveButtonTo(design, 1, 75, 160) // 45 mm from id 0
    const analysis = analyse(design)
    expect(spacingViolations(design)).toHaveLength(1)
    expect(spacingViolations(design)[0]!.distance).toBe(45)
    expect(analysis.checks.find((c) => c.key === 'spacing')!.severity).toBe('warn')
    expect(analysis.blocked).toBe(false)
  })

  it('counts two grid spaces on a 25 mm grid as 50 mm', () => {
    const design = live()
    design.unit.pitch = 25
    moveButtonTo(design, 1, 100, 200)
    moveButtonTo(design, 0, 50, 200)
    expect(spacingViolations(design).filter((v) => v.distance === 50)).toHaveLength(0)
  })

  it('needs 20 mm between centres for finger control', () => {
    const design = live()
    design.purpose = 'finger'
    moveButtonTo(design, 1, 70, 160) // 40 mm from id 0
    expect(spacingViolations(design)).toHaveLength(0)
    moveButtonTo(design, 1, 50, 160) // 20 mm: exactly enough
    expect(spacingViolations(design)).toHaveLength(0)
    moveButtonTo(design, 1, 47, 160) // 17 mm: clear of the 16 mm dome, but too close
    expect(spacingViolations(design)).toHaveLength(1)
    expect(analyse(design).blocked).toBe(false)
    // any closer and the domes touch, which is an error instead
    moveButtonTo(design, 1, 35, 160)
    expect(spacingViolations(design)).toHaveLength(0)
    expect(analyse(design).checks.find((c) => c.key === 'overlap')!.severity).toBe('error')
  })

  it('ignores the unit edge', () => {
    const design = live()
    moveButtonTo(design, 0, 8, 160) // touching the edge
    expect(spacingViolations(design)).toHaveLength(0)
  })

  it('leaves overlaps to the error check', () => {
    const design = live()
    moveButtonTo(design, 1, 35, 160)
    expect(spacingViolations(design)).toHaveLength(0)
    const analysis = analyse(design)
    expect(analysis.checks.find((c) => c.key === 'overlap')!.severity).toBe('error')
    expect(analysis.blocked).toBe(true)
  })
})

describe('button size', () => {
  it('finger buttons are 12 mm and foot buttons 16 mm', () => {
    expect(buttonDiameter('finger')).toBe(12)
    expect(buttonDiameter('foot')).toBe(16)
  })

  it('finger buttons 14 mm apart do not overlap, foot buttons do', () => {
    const design = live()
    moveButtonTo(design, 1, 44, 160) // 14 mm from id 0
    expect(analyse(design).checks.find((c) => c.key === 'overlap')!.severity).toBe('error')
    design.purpose = 'finger'
    const analysis = analyse(design)
    expect(analysis.checks.find((c) => c.key === 'overlap')!.severity).toBe('ok')
    // ...but they are still closer than the 20 mm finger spacing
    expect(spacingViolations(design)).toHaveLength(1)
  })

  it('a button may sit closer to the unit edge when it is smaller', () => {
    const design = live()
    moveButtonTo(design, 0, 7, 190)
    expect(analyse(design).checks.find((c) => c.key === 'outside')!.severity).toBe('error')
    design.purpose = 'finger'
    expect(analyse(design).checks.find((c) => c.key === 'outside')!.severity).toBe('ok')
  })

  it('the drawing draws holes at the button size for the purpose', () => {
    for (const [purpose, hole] of [['foot', 16], ['finger', 12]] as const) {
      const design = live()
      design.purpose = purpose
      const svg = buildArtifacts(design, analyse(design)).drawingSvg
      expect(svg).toContain(`r="${hole / 2}"`)
      expect(svg).toContain(`ø${hole} mm`)
    }
  })
})

describe('measure-before-you-drill warnings', () => {
  const artifacts = () => {
    const design = live()
    return buildArtifacts(design, analyse(design))
  }

  it('the drawing opens with a boxed warning about hole sizes', () => {
    const svg = artifacts().drawingSvg
    expect(svg).toContain('MEASURE YOUR PARTS BEFORE YOU DRILL')
    expect(svg).toContain('placeholders')
    expect(svg).toContain('Do not drill from this drawing')
    // the warning comes before the unit outline, so it is the first thing on the page
    expect(svg.indexOf('MEASURE YOUR PARTS')).toBeLessThan(svg.indexOf('ILI9341 2.8″ — cut-out'))
  })

  it('the parts list carries it in both formats', () => {
    const art = artifacts()
    expect(art.partsCsv).toContain('MEASURE YOUR PARTS BEFORE YOU DRILL')
    expect(art.partsMarkdown).toContain('MEASURE YOUR PARTS BEFORE YOU DRILL')
    expect(art.parts.find((p) => p.part.startsWith('Momentary'))!.notes).toContain('measure your part')
  })

  it('the build guide leads with it, before any other section', () => {
    const guide = artifacts().buildGuide
    expect(guide.indexOf('MEASURE YOUR PARTS BEFORE YOU DRILL')).toBeGreaterThan(-1)
    expect(guide.indexOf('MEASURE YOUR PARTS BEFORE YOU DRILL')).toBeLessThan(guide.indexOf('## What you need'))
  })

  it('the schematic tells the builder to check datasheets before powering up', () => {
    expect(artifacts().schematicSvg).toContain('datasheets')
    expect(artifacts().schematicSvg).toContain('at your own risk')
  })

  it('the drawing and the build guide say you build at your own risk', () => {
    expect(artifacts().drawingSvg).toContain('at your own risk')
    expect(artifacts().buildGuide).toContain('BUILD AT YOUR OWN RISK')
    expect(artifacts().buildGuide).toContain('multimeter')
  })

  it('a long name cannot break out of the warning box', () => {
    const design = live()
    design.unit = { width: 120, height: 200, pitch: 10 }
    design.name = 'x'.repeat(200)
    const svg = buildArtifacts(design, analyse(design)).drawingSvg
    expect(svg.startsWith('<svg')).toBe(true)
    expect(svg.endsWith('</svg>')).toBe(true)
  })
})

describe('blocking checks', () => {
  it('an overlapping screen blocks export', () => {
    const design = live()
    design.screen.orientation = 'portrait'
    design.screen.y = 100
    const analysis = analyse(design)
    expect(analysis.screenConflict).toBe(true)
    expect(analysis.blocked).toBe(true)
  })

  it('a button outside the unit blocks export', () => {
    const design = live()
    design.unit.width = 200
    expect(analyse(design).blocked).toBe(true)
  })

  it('an untested or touchless screen only warns', () => {
    const design = live()
    design.screen.catalogId = 'st7789-20'
    const analysis = analyse(design)
    expect(analysis.blocked).toBe(false)
    expect(stepSeverity(analysis, 4)).toBe('warn')
  })

  it('too many buttons for the pins blocks export, with pins left unassigned', () => {
    const design = live()
    const { positions, unit } = quickGridPositions(design, { columns: 12, rows: 4, spacing: 20, grow: true })
    design.unit = unit
    for (const position of positions) {
      design.buttons.push({ uid: design.nextUid++, id: 0, x: position.x, y: position.y, led: 'above' })
    }
    const analysis = analyse(design)
    expect(analysis.plan.over).toBe(true)
    expect(analysis.blocked).toBe(true)
    expect(analysis.checks.find((c) => c.key === 'pins')!.severity).toBe('error')
    expect([...analysis.plan.buttonPins.values()].some((pin) => pin === undefined)).toBe(true)
  })
})

describe('numbering', () => {
  it('numbers bottom-left to top-right by rows', () => {
    const design = live()
    const ids = computeIds(design.buttons, 'bottom-left', 16)
    const at = (x: number, y: number) => ids.get(design.buttons.find((b) => b.x === x && b.y === y)!.uid)
    expect(at(30, 160)).toBe(0)
    expect(at(270, 160)).toBe(4)
    expect(at(30, 100)).toBe(5)
    expect(at(150, 40)).toBe(10)
    expect(at(270, 40)).toBe(12)
  })

  it('numbers top-left to bottom-right', () => {
    const design = live()
    const ids = computeIds(design.buttons, 'top-left', 16)
    expect(ids.get(design.buttons.find((b) => b.x === 150 && b.y === 40)!.uid)).toBe(0)
    expect(ids.get(design.buttons.find((b) => b.x === 270 && b.y === 160)!.uid)).toBe(12)
  })
})

describe('on-screen grid', () => {
  it('mirrors the columns and rows of the performance buttons', () => {
    const grid = mirrorGrid(performanceButtons(live()))
    expect(grid.halfCols).toBe(10) // 5 cells, each two half-columns wide
    expect(grid.rows).toBe(2)
    expect(grid.offGrid).toEqual([])
    expect(grid.clashes).toEqual([])
  })

  const at = (xs: number[], y: number, firstUid: number) =>
    xs.map((x, i) => ({ uid: firstUid + i, id: firstUid + i, x, y, led: 'above' as const }))

  it('draws a staggered row half a cell along instead of adding columns', () => {
    // 8 across the bottom at 40 mm, two above sitting between 1-2 and 2-3
    const bottom = at([0, 40, 80, 120, 160, 200, 240, 280], 100, 0)
    const upper = at([20, 60], 60, 8)
    const grid = mirrorGrid([...bottom, ...upper])
    expect(grid.halfCols).toBe(16) // 8 cells across; the staggered pair sits inside the span
    expect(grid.rows).toBe(2)
  })

  it('lays out the 15 performance buttons of the 400 mm keyboard unit in 8 cells across', () => {
    const p = 42 // mm between neighbours
    const bottom = at([0, 1, 2, 3, 4, 5, 6, 7].map((i) => i * p), 150, 0) // 1-8
    const middle = [
      ...at([0.5 * p, 1.5 * p], 100, 8), // 9, 10
      ...at([3.5 * p, 4.5 * p, 5.5 * p], 100, 10), // 11-13
    ]
    const top = at([0, p], 50, 13) // 14, 15
    const grid = mirrorGrid([...bottom, ...middle, ...top])
    expect(grid.halfCols).toBe(16) // 8 cells across - not the 17 distinct columns of the old grid
    expect(grid.rows).toBe(3)
    expect(grid.offGrid).toEqual([])
    expect(grid.clashes).toEqual([])
    const col = (uid: number) => grid.cells.find((c) => c.uid === uid)!.halfCol
    expect([col(8), col(9)]).toEqual([1, 3]) // 9 and 10 between 1-2 and 2-3
    expect([col(10), col(11), col(12)]).toEqual([7, 9, 11])
    expect([col(13), col(14)]).toEqual([0, 2])
  })

  it('does not stagger when every row lines up', () => {
    const grid = mirrorGrid([...at([0, 40, 80], 100, 0), ...at([0, 40], 60, 3)])
    expect(grid.halfCols).toBe(6)
    expect(grid.cells.find((c) => c.uid === 3)).toMatchObject({ halfCol: 0, row: 0 })
  })

  it('places staggered buttons one half-column in', () => {
    const grid = mirrorGrid([...at([0, 40, 80], 100, 0), ...at([20, 60], 60, 3)])
    expect(grid.halfCols).toBe(6)
    const col = (uid: number) => grid.cells.find((c) => c.uid === uid)!.halfCol
    expect([col(0), col(1), col(2)]).toEqual([0, 2, 4])
    expect([col(3), col(4)]).toEqual([1, 3])
  })

  it('groups rows with a tolerance, and flags buttons far off any half-column', () => {
    const grid = mirrorGrid([...at([0, 80], 100, 0), ...at([40, 120], 97, 2), ...at([30], 60, 4)])
    expect(grid.rows).toBe(2) // 100 and 97 are one row of four at a 40 mm spacing
    expect(grid.offGrid).toEqual([4]) // 30 is 0.75 spacings along, between two half-columns
  })

  it('reports cells that would overlap', () => {
    const grid = mirrorGrid([...at([0, 40, 80], 100, 0), ...at([20, 25], 60, 3)])
    expect(grid.clashes.sort()).toEqual([3, 4])
  })

  it('suggests the smallest verified screen that stays legible', () => {
    expect(suggestScreen(SCREENS, 'landscape', 5, 2).id).toBe('ili9341-24')
    // a staggered row adds half a cell, which still fits the small screen
    expect(suggestScreen(SCREENS, 'landscape', 5.5, 2).id).toBe('ili9341-24')
    // eight columns need the wider 480 px panels, which are all untested
    expect(suggestScreen(SCREENS, 'landscape', 8, 5).id).toMatch(/^st7796/)
  })
})

describe('quick layout', () => {
  it('grows the unit to hold the grid and stays on the layout grid', () => {
    const design = live()
    const { positions, unit } = quickGridPositions(design, { columns: 9, rows: 5, spacing: 70, grow: true })
    expect(positions).toHaveLength(45)
    expect(unit.width).toBeGreaterThanOrEqual(8 * 70)
    for (const p of positions) {
      expect(p.x % 10).toBe(0)
      expect(p.y % 10).toBe(0)
      expect(p.x).toBeLessThanOrEqual(unit.width)
      expect(p.y).toBeLessThanOrEqual(unit.height)
    }
  })
})

describe('pin formatting', () => {
  it('collapses ascending runs', () => {
    expect(formatPinRuns([33, 34, 35, 36, 15, 21, 22, 23])).toBe('33–36, 15, 21–23')
    expect(formatPinRuns([undefined, 4])).toBe('4')
  })
})

describe('device.json', () => {
  it('saves the unit colour as an RGB code and reads it back', () => {
    const design = live()
    design.colour = [10, 120, 200]
    const text = serialiseDevice(design)
    expect(JSON.parse(text).unit.colourCode).toEqual([10, 120, 200])
    expect(parseDevice(text).design!.colour).toEqual([10, 120, 200])
  })

  it('defaults to very dark grey when a file has no colour', () => {
    const file = JSON.parse(serialiseDevice(live()))
    delete file.unit.colourCode
    expect(parseDevice(JSON.stringify(file)).design!.colour).toEqual([38, 38, 38])
  })

  it('round-trips a design exactly', () => {
    for (const preset of PRESETS) {
      const design = preset.build()
      const text = serialiseDevice(design)
      const parsed = parseDevice(text)
      expect(parsed.errors).toBeUndefined()
      expect(serialiseDevice(parsed.design!)).toBe(text)
    }
  })

  it('rejects a file that does not match the schema', () => {
    const result = parseDevice('{"schemaVersion": 1}')
    expect(result.design).toBeUndefined()
    expect(result.errors!.length).toBeGreaterThan(0)
  })

  it('rejects navigation that points at a missing button', () => {
    const file = JSON.parse(serialiseDevice(live()))
    file.navigation.up = 99
    const result = parseDevice(JSON.stringify(file))
    expect(result.errors).toEqual(expect.arrayContaining([expect.stringContaining('99')]))
  })

  it('rejects an unknown screen', () => {
    const file = JSON.parse(serialiseDevice(live()))
    file.screen.catalogId = 'nope'
    expect(parseDevice(JSON.stringify(file)).errors).toEqual(expect.arrayContaining([expect.stringContaining('nope')]))
  })

  it('is not valid JSON', () => {
    expect(parseDevice('{').errors![0]).toMatch(/JSON/)
  })
})

describe('exports', () => {
  it('produces every file for the current unit', () => {
    const design = live()
    const art = buildArtifacts(design, analyse(design))
    expect(art.drawingSvg.startsWith('<svg')).toBe(true)
    expect(art.schematicSvg.startsWith('<svg')).toBe(true)
    expect(art.header).toContain('#define BUTTON_COUNT')
    expect(art.header).toContain('BUTTON_PINS[BUTTON_COUNT] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 15, 21, 22, 23 }')
    expect(art.header).toContain('SCREEN_DRIVER_ILI9341_T4')
    expect(art.header).toContain('SHIFT_REGISTERS')
    // every #define line must separate the macro name from its value with at
    // least one space - a name at or past the padding width (e.g. the 24-char
    // "SCREEN_DRIVER_ILI9341_T4") previously glued its value straight on:
    // "#define SCREEN_DRIVER_ILI9341_T41"
    for (const line of art.header.split('\n')) {
      if (line.startsWith('#define ') && line !== '#define MidiToolboxDevice_h') {
        expect(line).toMatch(/^#define \S+\s+\S/)
      }
    }
    expect(art.partsCsv.split('\n')[0]).toBe('Qty,Part,Notes')
    // 13 buttons x 3 channels = 39, in registers of 8 outputs = 5
    expect(art.parts.find((p) => p.part.startsWith('74HC595'))!.qty).toBe(5)
    expect(art.parts.find((p) => p.part.startsWith('Resistor, 220'))!.qty).toBe(39)
  })

  it('escapes anything typed into the design name', () => {
    const design = live()
    design.name = '<script>"&'
    const art = buildArtifacts(design, analyse(design))
    expect(art.drawingSvg).not.toContain('<script>')
    expect(art.schematicSvg).not.toContain('<script>')
    expect(art.header).toContain('\\"')
  })

  it('zips every file', () => {
    const design = live()
    const entries = zipEntries(buildArtifacts(design, analyse(design)), 'live-rig')
    const zip = buildZip(entries)
    expect(zip.length).toBeGreaterThan(1000)
    expect(Object.keys(entries)).toEqual(expect.arrayContaining(['device.json', 'live-rig-design.json']))
  })
})

describe('firmware device.json', () => {
  const validate = new Ajv({ allErrors: true, strict: false }).compile(profileSchema)

  it('matches the hand-checked profile for the original unit', () => {
    const design = live()
    expect(buildDeviceProfile(design, analyse(design))).toEqual(liveRigProfile)
  })

  it('is valid against its schema for every preset that uses a supported screen', () => {
    for (const preset of PRESETS) {
      const design = preset.build()
      const analysis = analyse(design)
      if (analysis.screen.driver !== 'ILI9341_T4') continue
      const profile = buildDeviceProfile(design, analysis)
      expect(validate(profile), `${preset.label}: ${JSON.stringify(validate.errors)}`).toBe(true)
    }
  })

  it('saves the design file and the firmware file separately', () => {
    const design = live()
    const art = buildArtifacts(design, analyse(design))
    expect(JSON.parse(art.designJson).unit).toBeDefined()
    expect(JSON.parse(art.deviceJson).unit).toBeUndefined()
    expect(JSON.parse(art.deviceJson).buttons.pins).toHaveLength(13)
  })

  it('refuses LED pins the firmware cannot use', () => {
    const design = live()
    design.pins.led.latch = 31
    const analysis = analyse(design)
    expect(analysis.blocked).toBe(true)
    expect(analysis.checks.find((c) => c.key === 'led-pins')?.severity).toBe('error')
  })
})
