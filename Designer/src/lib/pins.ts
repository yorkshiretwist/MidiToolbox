// Direct-GPIO pin planning for a Teensy: one pin per button, the screen's
// signals, and the three control pins of the LED shift-register chain.
//
// Every pin has a default, and the user may override any of them. Defaults are
// chosen around the pins that are already claimed (by an override, by the screen,
// by the LED chain, or reserved by the board), so a design that never touches a
// pin never has two things on one. Conflicts can only come from a choice the
// user made, and are reported rather than prevented.
//
// Ground and power are not pins in this model: they are shared by design.

import type {
  Design,
  DesignPins,
  LedPin,
  MicrocontrollerSpec,
  PinAssignment,
  ScreenSpec,
} from '../types/device'

export const LED_PIN_KEYS: LedPin[] = ['latch', 'data', 'clock']

/** The LED chain's pins, as wired in MIDI-Toolbox.ino (ShiftPWM, bit-banged). */
export const DEFAULT_LED_PINS: Record<LedPin, number> = { latch: 28, data: 29, clock: 30 }
/** Chip-select for an SD module on a board with no built-in slot. */
export const SD_MODULE_CS_PIN = 7
/** Pins the existing unit's buttons use, tried first so a familiar layout keeps its numbers. */
const PREFERRED_BUTTON_PINS = [33, 34, 35, 36, 37, 38, 39, 40, 41, 15, 21, 22, 23]

export function emptyPins(): DesignPins {
  return { buttons: {}, led: {}, screen: {} }
}

export interface PinConflict {
  pin: number
  /** What is on the pin, e.g. "Button 3", "Screen CS", "the built-in SD card". */
  owners: string[]
}

export interface PinUse {
  pin: number
  owner: string
  group: 'button' | 'screen' | 'led' | 'sd'
  /** The button's uid, for a button. */
  uid?: number
}

/** The pins in effect, before any conflict analysis. */
interface Resolved {
  buttonPins: Map<number, number | undefined>
  screenPins: PinAssignment[]
  ledPins: Record<LedPin, number>
}

export interface PinPlan {
  needed: number
  /** Pins the design can use: every pin except those the built-in SD card owns. */
  available: number
  over: boolean
  usage: { buttons: number; screen: number; led: number; sd: number }
  /** The pin for each button uid; undefined when there were none left. */
  buttonPins: Map<number, number | undefined>
  screenPins: PinAssignment[]
  ledPins: Record<LedPin, number>
  /** An external SD module's pins, on a board with no built-in slot. */
  sdPins: PinAssignment[]
  /** Pins the built-in SD card owns. */
  reservedPins: number[]
  /** 74HC595 registers needed: 3 channels per RGB LED, 8 outputs per register. */
  registers: number
  /** The same three groups with no overrides, to show what has been changed. */
  defaults: Resolved
  /** Every pin in use and by what, lowest pin first. */
  uses: PinUse[]
  /** Pins used by more than one thing. */
  conflicts: PinConflict[]
  /** Problems with the screen's bus pins (not a real hardware SPI / I2C bus). */
  busIssues: string[]
  /** In-use pins that are underside solder pads. */
  padPinsUsed: number[]
}

export function shiftRegistersFor(buttonCount: number): number {
  return Math.ceil((buttonCount * 3) / 8)
}

/** The order pins are handed out in: familiar ones, then edge pins, then the awkward underside pads. */
function poolOrder(mcu: MicrocontrollerSpec, reserved: number[]): number[] {
  const edge: number[] = []
  const pads: number[] = []
  for (let pin = 0; pin < mcu.pins; pin++) {
    if (reserved.includes(pin)) continue
    ;(mcu.padPins.includes(pin) ? pads : edge).push(pin)
  }
  const out: number[] = []
  for (const pin of [...PREFERRED_BUTTON_PINS, ...edge, ...pads]) {
    if (pin < mcu.pins && !reserved.includes(pin) && !out.includes(pin)) out.push(pin)
  }
  return out
}

/** Default pins with nothing changed: buttons take the pool in id order, around the screen and LED pins. */
function baselineButtonPins(
  design: Design,
  mcu: MicrocontrollerSpec,
  screen: ScreenSpec,
): Map<number, number | undefined> {
  const reserved = mcu.sdBuiltIn ? mcu.sdPins : []
  const external = mcu.sdBuiltIn ? [] : [SD_MODULE_CS_PIN]
  const taken = new Set<number>([...screen.pins.map((p) => p.pin), ...Object.values(DEFAULT_LED_PINS), ...reserved, ...external])
  const pool = poolOrder(mcu, reserved).filter((pin) => !taken.has(pin))
  const out = new Map<number, number | undefined>()
  let next = 0
  for (const b of [...design.buttons].sort((a, c) => a.id - c.id)) out.set(b.uid, pool[next++])
  return out
}

function resolve(
  design: Design,
  mcu: MicrocontrollerSpec,
  screen: ScreenSpec,
  overrides: DesignPins,
): Resolved {
  const screenPins = screen.pins.map((p) => ({ ...p, pin: overrides.screen[p.role] ?? p.pin }))
  const ledPins = { ...DEFAULT_LED_PINS }
  for (const key of LED_PIN_KEYS) ledPins[key] = overrides.led[key] ?? DEFAULT_LED_PINS[key]

  const reserved = mcu.sdBuiltIn ? mcu.sdPins : []
  const external = mcu.sdBuiltIn ? [] : [SD_MODULE_CS_PIN]
  const taken = new Set<number>([...screenPins.map((p) => p.pin), ...Object.values(ledPins), ...reserved, ...external])
  for (const b of design.buttons) {
    const chosen = overrides.buttons[b.uid]
    if (chosen !== undefined) taken.add(chosen)
  }

  // A button that has not been changed keeps the pin it would have had with no
  // changes at all, so editing one pin never rewires the rest. Only a button
  // whose default has been taken by something else moves, to a pin nobody else
  // is using or would have used.
  const base = baselineButtonPins(design, mcu, screen)
  const stays = new Set<number>()
  for (const b of design.buttons) {
    const pin = base.get(b.uid)
    if (overrides.buttons[b.uid] === undefined && pin !== undefined && !taken.has(pin)) stays.add(pin)
  }
  const spare = poolOrder(mcu, reserved).filter((pin) => !taken.has(pin) && !stays.has(pin))

  const buttonPins = new Map<number, number | undefined>()
  let next = 0
  for (const b of [...design.buttons].sort((a, c) => a.id - c.id)) {
    const chosen = overrides.buttons[b.uid]
    const pin = base.get(b.uid)
    if (chosen !== undefined) buttonPins.set(b.uid, chosen)
    else if (pin !== undefined && stays.has(pin)) buttonPins.set(b.uid, pin)
    else buttonPins.set(b.uid, spare[next++])
  }
  return { buttonPins, screenPins, ledPins }
}

function checkBuses(mcu: MicrocontrollerSpec, screenPins: PinAssignment[]): string[] {
  const pinOf = (role: string) => screenPins.find((p) => p.role === role)?.pin
  const issues: string[] = []
  const sck = pinOf('SCK')
  const mosi = pinOf('MOSI')
  const miso = pinOf('MISO')
  if (sck !== undefined && mosi !== undefined) {
    const isBus = mcu.spiBuses.some((b) => b.sck === sck && b.mosi === mosi && (miso === undefined || b.miso === miso))
    if (!isBus) {
      const asked = `SCK ${sck}, MOSI ${mosi}${miso === undefined ? '' : `, MISO ${miso}`}`
      const known = mcu.spiBuses.map((b) => `${b.name}: SCK ${b.sck}, MOSI ${b.mosi}, MISO ${b.miso}`).join('; ')
      issues.push(`Screen ${asked} are not one hardware SPI bus (${known})`)
    }
  }
  const sda = pinOf('SDA')
  const scl = pinOf('SCL')
  if (sda !== undefined && scl !== undefined) {
    if (!mcu.i2cBuses.some((b) => b.sda === sda && b.scl === scl)) {
      const known = mcu.i2cBuses.map((b) => `${b.name}: SDA ${b.sda}, SCL ${b.scl}`).join('; ')
      issues.push(`Screen SDA ${sda}, SCL ${scl} are not one hardware I2C bus (${known})`)
    }
  }
  return issues
}

export function planPins(design: Design, mcu: MicrocontrollerSpec, screen: ScreenSpec): PinPlan {
  const now = resolve(design, mcu, screen, design.pins)
  const defaults = resolve(design, mcu, screen, emptyPins())
  const reservedPins = mcu.sdBuiltIn ? mcu.sdPins : []
  const sdPins: PinAssignment[] = mcu.sdBuiltIn ? [] : [{ role: 'SD_CS', pin: SD_MODULE_CS_PIN }]

  const idOf = new Map(design.buttons.map((b) => [b.uid, b.id]))
  const uses: PinUse[] = []
  for (const [uid, pin] of now.buttonPins) {
    if (pin !== undefined) uses.push({ pin, owner: `Button ${(idOf.get(uid) ?? 0) + 1}`, group: 'button', uid })
  }
  for (const p of now.screenPins) uses.push({ pin: p.pin, owner: `Screen ${p.role}`, group: 'screen' })
  for (const key of LED_PIN_KEYS) uses.push({ pin: now.ledPins[key], owner: `LED ${key}`, group: 'led' })
  for (const p of sdPins) uses.push({ pin: p.pin, owner: 'SD module CS', group: 'sd' })

  // the built-in card only shows up on a pin someone else has also claimed
  const owners = new Map<number, string[]>()
  for (const use of uses) owners.set(use.pin, [...(owners.get(use.pin) ?? []), use.owner])
  const conflicts: PinConflict[] = []
  for (const [pin, who] of [...owners].sort((a, b) => a[0] - b[0])) {
    const all = reservedPins.includes(pin) ? [...who, 'the built-in SD card'] : who
    if (all.length > 1) conflicts.push({ pin, owners: all })
  }

  const usage = {
    buttons: design.buttons.length,
    screen: now.screenPins.length,
    led: LED_PIN_KEYS.length,
    sd: sdPins.length,
  }
  const needed = usage.buttons + usage.screen + usage.led + usage.sd
  const available = mcu.pins - reservedPins.length
  return {
    needed,
    available,
    over: needed > available,
    usage,
    buttonPins: now.buttonPins,
    screenPins: now.screenPins,
    ledPins: now.ledPins,
    sdPins,
    reservedPins,
    registers: shiftRegistersFor(design.buttons.length),
    defaults,
    uses: uses.sort((a, b) => a.pin - b.pin || a.owner.localeCompare(b.owner)),
    conflicts,
    busIssues: checkBuses(mcu, now.screenPins),
    padPinsUsed: [...new Set(uses.map((u) => u.pin).filter((p) => mcu.padPins.includes(p)))].sort((a, b) => a - b),
  }
}

/** "33, 34, 35, 15" → "33–35, 15": collapses consecutive ascending runs. */
export function formatPinRuns(pins: (number | undefined)[]): string {
  const out: string[] = []
  let start: number | null = null
  let prev = 0
  const flush = () => {
    if (start !== null) out.push(start === prev ? `${start}` : `${start}–${prev}`)
  }
  for (const pin of pins) {
    if (pin === undefined) continue
    if (start === null) {
      start = prev = pin
    } else if (pin === prev + 1) {
      prev = pin
    } else {
      flush()
      start = prev = pin
    }
  }
  flush()
  return out.join(', ')
}
