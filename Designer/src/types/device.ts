// Types for the Designer. `Design` is the in-memory working model (buttons carry
// a stable `uid` so ids can be renumbered without losing which button plays
// which navigation role); `DeviceFile` is the on-disk shape defined by
// config/design-schema.json. Keep DeviceFile in step with that schema.

export type Purpose = 'foot' | 'finger'
export type LedSide = 'above' | 'below'
export type Orientation = 'landscape' | 'portrait'
export type Numbering = 'bottom-left' | 'top-left' | 'manual'
export type NavRole = 'select' | 'down' | 'up'
export type LedPin = 'latch' | 'data' | 'clock'

/**
 * Pins the user has set by hand. Anything not listed here uses the default, so a
 * design that never touches pins always gets sensible ones. Buttons are keyed by
 * their `uid` (a pin stays with its physical button when ids are renumbered);
 * screen signals by the name they have in the catalogue.
 */
export interface DesignPins {
  buttons: Record<number, number>
  led: Partial<Record<LedPin, number>>
  screen: Record<string, number>
}

export interface DesignButton {
  /** Stable identity within a design; never shown, never saved. */
  uid: number
  /** Physical button index that performance configs refer to. */
  id: number
  /** Centre, mm from the unit's left edge. */
  x: number
  /** Centre, mm from the unit's top edge. */
  y: number
  led: LedSide
}

export interface Design {
  name: string
  purpose: Purpose
  unit: { width: number; height: number; pitch: number }
  /** Colour of the unit's face as [r, g, b], each 0-255. */
  colour: number[]
  numbering: Numbering
  buttons: DesignButton[]
  /** uids of the buttons playing each navigation role. */
  nav: Record<NavRole, number>
  screen: { catalogId: string; orientation: Orientation; x: number; y: number }
  microcontroller: string
  pins: DesignPins
  nextUid: number
}

export interface DeviceFile {
  schemaVersion: 1
  name: string
  purpose: Purpose
  unit: { widthMm: number; heightMm: number; gridPitchMm: number; colourCode?: number[] }
  numbering: Numbering
  buttons: { id: number; xMm: number; yMm: number; led: LedSide }[]
  navigation: Record<NavRole, number>
  screen: { catalogId: string; orientation: Orientation; xMm: number; yMm: number }
  microcontroller: string
  /** The pin every connection uses, in full (optional so older files still load). */
  pins?: {
    buttons?: { id: number; pin: number }[]
    screen?: Record<string, number>
    ledChain?: Partial<Record<LedPin, number>>
  }
}

// ── Catalogue entries (src/data/catalog/*.json, maintained by hand) ─────────

/** What a signal needs its pin to be able to do. */
export type PinKind = 'gpio' | 'spi-sck' | 'spi-mosi' | 'spi-miso' | 'i2c-sda' | 'i2c-scl'

export interface PinAssignment {
  role: string
  pin: number
  kind?: PinKind
}

export interface SpiBus {
  name: string
  sck: number
  mosi: number
  miso: number
}

export interface I2cBus {
  name: string
  sda: number
  scl: number
}

export interface ScreenSpec {
  id: string
  name: string
  controller: string
  /** Native landscape resolution: [width, height] with width >= height. */
  resolution: [number, number]
  /**
   * Landscape size in mm of the screen as it is visible through the panel
   * cut-out, bevel included. Not the whole PCB: the mounting "ears" sit behind
   * the panel and are not counted. Approximate; check your own part.
   */
  moduleMm: [number, number]
  /** Landscape size in mm of the lit display area, centred in `moduleMm`. */
  activeMm: [number, number]
  touch: { type: 'none' | 'resistive' | 'capacitive'; controller?: string }
  /** Firmware driver family. Only ILI9341_T4 exists in the firmware today. */
  driver: string
  status: 'verified' | 'untested'
  pins: PinAssignment[]
  bomName: string
  sourceUrl: string
}

export interface MicrocontrollerSpec {
  id: string
  name: string
  /** Every digital pin number the board has, 0 to pins - 1. */
  pins: number
  sdBuiltIn: boolean
  /** Pins the built-in SD socket is wired to: not available while it is used. */
  sdPins: number[]
  /** Pins that are solder pads on the underside rather than edge pins. */
  padPins: number[]
  /** Hardware SPI buses: a screen's SCK, MOSI and MISO must be one of these. */
  spiBuses: SpiBus[]
  /** Hardware I2C buses: a screen's SDA and SCL must be one of these. */
  i2cBuses: I2cBus[]
  bomName: string
  sourceUrl: string
}
