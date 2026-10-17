// Tests for Teensy pin assignment and the pin detail shown in the wiring schematic.
import { describe, expect, it } from 'vitest'
import { requireMicrocontroller, requireScreen } from './catalog'
import { analyse } from './checks'
import { parseDevice, serialiseDevice } from './deviceFile'
import { buildArtifacts } from './export/artifacts'
import { planPins } from './pins'
import { buildPreset } from './presets'
import type { Design } from '../types/device'

const live = () => buildPreset('live-rig')
const uidOfId = (design: Design, id: number) => design.buttons.find((b) => b.id === id)!.uid
const planOf = (design: Design) =>
  planPins(design, requireMicrocontroller(design.microcontroller), requireScreen(design.screen.catalogId))
const check = (design: Design, key: string) => analyse(design).checks.find((c) => c.key === key)

describe('pin assignment', () => {
  it('the defaults share no pin, and the budget excludes the SD card pins', () => {
    const design = live()
    const plan = planOf(design)
    expect(plan.conflicts).toEqual([])
    expect(plan.needed).toBe(24)
    expect(plan.available).toBe(49) // 55 pins, 6 of them wired to the built-in SD card
    expect(check(design, 'pin-conflicts')!.severity).toBe('ok')
  })

  it('never hands out a pin the SD card owns', () => {
    const design = buildPreset('large-foot')
    const plan = planOf(design)
    const pins = [...plan.buttonPins.values()]
    expect(pins.some((p) => p !== undefined && plan.reservedPins.includes(p))).toBe(false)
    expect(plan.conflicts).toEqual([])
  })

  it('an override takes effect and the other defaults step around it', () => {
    const design = live()
    const target = uidOfId(design, 0) // defaults to 33
    design.pins.buttons[target] = 5
    const plan = planOf(design)
    expect(plan.buttonPins.get(target)).toBe(5)
    expect(plan.defaults.buttonPins.get(target)).toBe(33)
    expect(plan.conflicts).toEqual([])
    // overriding button 1 onto what button 2 would have used: button 2 moves on
    const other = uidOfId(design, 1)
    design.pins.buttons[target] = 34
    const after = planOf(design)
    expect(after.buttonPins.get(other)).not.toBe(34)
    expect(after.conflicts).toEqual([])
  })

  it('changing one pin does not rewire the others', () => {
    const design = live()
    const before = planOf(design)
    design.pins.buttons[uidOfId(design, 0)] = 5 // frees 33
    const after = planOf(design)
    for (const b of design.buttons) {
      if (b.id !== 0) expect(after.buttonPins.get(b.uid), `button ${b.id + 1}`).toBe(before.buttonPins.get(b.uid))
    }
    expect(after.conflicts).toEqual([])
    // moving a screen pin onto a button's default moves only that button
    const design2 = live()
    design2.pins.screen.CS = 34 // button 2's default
    const plan2 = planOf(design2)
    const moved = design2.buttons.filter((b) => plan2.buttonPins.get(b.uid) !== before.buttonPins.get(b.uid))
    expect(moved.map((b) => b.id)).toEqual([1])
    expect(plan2.conflicts).toEqual([])
  })

  it('warns when two buttons share a pin, without blocking export', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 40
    design.pins.buttons[uidOfId(design, 1)] = 40
    const analysis = analyse(design)
    expect(analysis.plan.conflicts).toEqual([{ pin: 40, owners: ['Button 1', 'Button 2'] }])
    const found = check(design, 'pin-conflict-40')!
    expect(found.severity).toBe('warn')
    expect(found.text).toBe('Pin 40 is used by Button 1 and Button 2')
    expect(analysis.blocked).toBe(false)
  })

  it('warns when a button sits on a screen or LED pin', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 2)] = 10 // the screen's CS
    design.pins.buttons[uidOfId(design, 3)] = 29 // the LED data pin
    expect(analyse(design).plan.conflicts).toEqual([
      { pin: 10, owners: ['Button 3', 'Screen CS'] },
      { pin: 29, owners: ['Button 4', 'LED data'] },
    ])
  })

  it('warns when a pin is one the built-in SD card owns', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 43
    expect(planOf(design).conflicts).toEqual([{ pin: 43, owners: ['Button 1', 'the built-in SD card'] }])
    expect(analyse(design).blocked).toBe(false)
  })

  it('does not treat the screen and touch sharing one SPI bus as a clash', () => {
    const pins = planOf(live()).screenPins.map((p) => p.pin)
    expect(new Set(pins).size).toBe(pins.length)
  })

  it('warns about underside solder pads', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 50
    expect(check(design, 'pin-pads')!.severity).toBe('warn')
    // a big enough unit runs out of edge pins and needs them by default
    expect(analyse(buildPreset('large-foot')).checks.find((c) => c.key === 'pin-pads')).toBeDefined()
  })

  it('checks the screen signals form a real hardware bus', () => {
    const design = live()
    expect(planOf(design).busIssues).toEqual([])
    design.pins.screen.SCK = 5
    expect(planOf(design).busIssues[0]).toMatch(/not one hardware SPI bus/)
    expect(check(design, 'pin-bus-0')!.severity).toBe('warn')
    // SPI1 is a real bus, so moving all three together is fine
    design.pins.screen = { SCK: 27, MOSI: 26, MISO: 1 }
    const plan = planOf(design)
    expect(plan.busIssues).toEqual([])
    expect(plan.conflicts).toEqual([])
    expect([...plan.buttonPins.values()]).not.toContain(1) // a button default stepped around it
  })

  it('checks the I2C signals of a capacitive-touch screen', () => {
    const design = live()
    design.screen.catalogId = 'st7796-35'
    expect(planOf(design).busIssues).toEqual([])
    design.pins.screen.SDA = 5
    expect(planOf(design).busIssues[0]).toMatch(/not one hardware I2C bus/)
  })

  it('writes the full pin list, and reads it back as the same design', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 5
    design.pins.led.data = 20
    const text = serialiseDevice(design)
    const file = JSON.parse(text)
    expect(file.pins.buttons).toHaveLength(13)
    expect(file.pins.buttons[0]).toEqual({ id: 0, pin: 5 })
    expect(file.pins.ledChain).toEqual({ latch: 28, data: 20, clock: 30 })
    expect(file.pins.screen.CS).toBe(10)
    const parsed = parseDevice(text)
    expect(parsed.errors).toBeUndefined()
    expect(serialiseDevice(parsed.design!)).toBe(text)
    expect(planOf(parsed.design!).buttonPins.get(uidOfId(parsed.design!, 0))).toBe(5)
  })

  it('round-tripping an untouched design keeps every pin a default', () => {
    const design = live()
    const parsed = parseDevice(serialiseDevice(design))
    expect(parsed.errors).toBeUndefined()
    expect(parsed.design!.pins).toEqual({ buttons: {}, led: {}, screen: {} })
    expect(planOf(parsed.design!).conflicts).toEqual([])
  })

  it('still opens a file that has no pins', () => {
    const file = JSON.parse(serialiseDevice(live()))
    delete file.pins
    const parsed = parseDevice(JSON.stringify(file))
    expect(parsed.errors).toBeUndefined()
    expect(planOf(parsed.design!).conflicts).toEqual([])
  })

  it('rejects a pin the board does not have, and a pin for a button that does not exist', () => {
    const file = JSON.parse(serialiseDevice(live()))
    file.pins.buttons[0].pin = 99
    expect(parseDevice(JSON.stringify(file)).errors).toEqual(expect.arrayContaining([expect.stringContaining('pin 99')]))
    const file2 = JSON.parse(serialiseDevice(live()))
    file2.pins.buttons.push({ id: 77, pin: 3 })
    expect(parseDevice(JSON.stringify(file2)).errors).toEqual(expect.arrayContaining([expect.stringContaining('77')]))
  })

  it('the firmware header uses the chosen pins', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 5
    design.pins.screen.CS = 4
    design.pins.led.latch = 20
    const header = buildArtifacts(design, analyse(design)).header
    expect(header).toContain('BUTTON_PINS[BUTTON_COUNT] = { 5, 34')
    expect(header).toMatch(/SCREEN_PIN_CS\s+4/)
    expect(header).toMatch(/LED_PIN_LATCH\s+20/)
  })
})

describe('wiring schematic pin detail', () => {
  const svgFor = (design: Design) => buildArtifacts(design, analyse(design)).schematicSvg

  it('labels every connection with its pin, in a table by Teensy pin', () => {
    const svg = svgFor(live())
    expect(svg).toContain('Wiring by Teensy 4.1 pin')
    expect(svg).toContain('→  Button 1</text>')
    expect(svg).toContain('→  Screen SCK')
    expect(svg).toContain('→  LED latch')
    expect(svg).toContain('→  Button 11 (select)')
    expect(svg).toContain('pin 13')
    expect(svg).toContain('28 latch')
    expect(svg).toContain('Pins 42–47 are wired to the built-in SD card')
  })

  it('has no conflict banner when nothing is shared', () => {
    expect(svgFor(live())).not.toContain('used by more than one connection')
  })

  it('shows a red banner and marks the pins when two things share one', () => {
    const design = live()
    design.pins.buttons[uidOfId(design, 0)] = 10
    const svg = svgFor(design)
    expect(svg).toContain('A pin is used by more than one connection')
    expect(svg).toContain('Pin 10: Button 1 and Screen CS')
    expect(svg).toContain('⚠ 10')
    expect(svg).toContain('#c62828')
  })

  it('points at the table when button pins are too scattered for a wire label', () => {
    expect(svgFor(buildPreset('large-foot'))).toContain('pins (table below)')
  })
})
