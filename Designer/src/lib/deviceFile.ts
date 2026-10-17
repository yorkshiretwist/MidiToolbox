// Reading and writing device.json. Reading validates against
// config/design-schema.json and then checks the things a schema cannot.

import Ajv from 'ajv'
import schema from '../generated/design-schema.json'
import type { Design, DeviceFile, NavRole } from '../types/device'
import { findMicrocontroller, findScreen, requireMicrocontroller, requireScreen } from './catalog'
import { NAV_ROLES } from './layout'
import { DEFAULT_UNIT_COLOUR, tidy } from './geometry'
import { DEFAULT_LED_PINS, emptyPins, LED_PIN_KEYS, planPins } from './pins'

const ajv = new Ajv({ allErrors: true, strict: false })
const validateFn = ajv.compile<DeviceFile>(schema)

export function toDeviceFile(design: Design): DeviceFile {
  const idOf = new Map(design.buttons.map((b) => [b.uid, b.id]))
  const navigation = {} as Record<NavRole, number>
  for (const role of NAV_ROLES) navigation[role] = idOf.get(design.nav[role]) ?? 0
  // pins are written out in full, defaults included, so a saved file always
  // describes the same wiring even if the defaults change in a later version
  const plan = planPins(design, requireMicrocontroller(design.microcontroller), requireScreen(design.screen.catalogId))
  const buttonPins: { id: number; pin: number }[] = []
  for (const b of [...design.buttons].sort((x, y) => x.id - y.id)) {
    const pin = plan.buttonPins.get(b.uid)
    if (pin !== undefined) buttonPins.push({ id: b.id, pin })
  }
  return {
    schemaVersion: 1,
    name: design.name,
    purpose: design.purpose,
    unit: { widthMm: design.unit.width, heightMm: design.unit.height, gridPitchMm: design.unit.pitch, colourCode: [...design.colour] },
    numbering: design.numbering,
    buttons: [...design.buttons]
      .sort((a, b) => a.id - b.id)
      .map((b) => ({ id: b.id, xMm: tidy(b.x), yMm: tidy(b.y), led: b.led })),
    navigation,
    screen: {
      catalogId: design.screen.catalogId,
      orientation: design.screen.orientation,
      xMm: tidy(design.screen.x),
      yMm: tidy(design.screen.y),
    },
    microcontroller: design.microcontroller,
    pins: {
      buttons: buttonPins,
      screen: Object.fromEntries(plan.screenPins.map((p) => [p.role, p.pin])),
      ledChain: { ...plan.ledPins },
    },
  }
}

export function serialiseDevice(design: Design): string {
  return JSON.stringify(toDeviceFile(design), null, 2) + '\n'
}

export type ParseResult = { design: Design; errors?: undefined } | { design?: undefined; errors: string[] }

export function parseDevice(text: string): ParseResult {
  let data: unknown
  try {
    data = JSON.parse(text)
  } catch (error) {
    return { errors: [`Not valid JSON: ${(error as Error).message}`] }
  }
  if (!validateFn(data)) {
    const errors = (validateFn.errors ?? []).map((e) => `${e.instancePath || '/'} ${e.message ?? 'is invalid'}`)
    return { errors: errors.slice(0, 6) }
  }
  const file = data

  const errors: string[] = []
  const ids = file.buttons.map((b) => b.id)
  if (new Set(ids).size !== ids.length) errors.push('Two buttons share the same id')
  const navIds = NAV_ROLES.map((role) => file.navigation[role])
  if (new Set(navIds).size !== 3) errors.push('The three navigation roles must be three different buttons')
  for (const id of navIds) if (!ids.includes(id)) errors.push(`Navigation refers to button id ${id}, which does not exist`)
  if (!findScreen(file.screen.catalogId)) errors.push(`Unknown screen "${file.screen.catalogId}"`)
  if (!findMicrocontroller(file.microcontroller)) errors.push(`Unknown microcontroller "${file.microcontroller}"`)
  if (file.buttons.length - 3 < 3) errors.push('A unit needs at least 3 performance buttons plus the 3 navigation buttons')
  if (errors.length > 0) return { errors }

  const mcu = requireMicrocontroller(file.microcontroller)
  const screen = requireScreen(file.screen.catalogId)
  const pins = emptyPins()
  const inRange = (pin: number) => pin >= 0 && pin < mcu.pins
  for (const entry of file.pins?.buttons ?? []) {
    if (!ids.includes(entry.id)) errors.push(`A pin is assigned to button id ${entry.id}, which does not exist`)
    else if (!inRange(entry.pin)) errors.push(`Button id ${entry.id} uses pin ${entry.pin}, which ${mcu.name} does not have`)
  }
  for (const [role, pin] of Object.entries(file.pins?.screen ?? {})) {
    if (!inRange(pin)) errors.push(`Screen ${role} uses pin ${pin}, which ${mcu.name} does not have`)
  }
  for (const [key, pin] of Object.entries(file.pins?.ledChain ?? {})) {
    if (!inRange(pin)) errors.push(`LED ${key} uses pin ${pin}, which ${mcu.name} does not have`)
  }
  if (errors.length > 0) return { errors }

  const design: Design = {
    name: file.name,
    purpose: file.purpose,
    unit: { width: file.unit.widthMm, height: file.unit.heightMm, pitch: file.unit.gridPitchMm },
    colour: file.unit.colourCode ? [...file.unit.colourCode] : [...DEFAULT_UNIT_COLOUR],
    numbering: file.numbering,
    buttons: file.buttons.map((b, i) => ({ uid: i + 1, id: b.id, x: b.xMm, y: b.yMm, led: b.led })),
    nav: { select: 0, down: 0, up: 0 },
    screen: {
      catalogId: file.screen.catalogId,
      orientation: file.screen.orientation,
      x: file.screen.xMm,
      y: file.screen.yMm,
    },
    microcontroller: file.microcontroller,
    pins,
    nextUid: file.buttons.length + 1,
  }
  for (const role of NAV_ROLES) {
    design.nav[role] = design.buttons.find((b) => b.id === file.navigation[role])!.uid
  }
  // A file lists every pin, but only the ones that differ from what the design
  // would get by default count as the user's own choices. That keeps "changed"
  // meaningful after a reload, and still pins down the wiring if the defaults
  // ever change: a pin that no longer matches the default is kept as a choice.
  // Signals this screen doesn't have are ignored.
  for (const [role, pin] of Object.entries(file.pins?.screen ?? {})) {
    const fallback = screen.pins.find((p) => p.role === role)
    if (fallback && pin !== fallback.pin) pins.screen[role] = pin
  }
  for (const key of LED_PIN_KEYS) {
    const pin = file.pins?.ledChain?.[key]
    if (pin !== undefined && pin !== DEFAULT_LED_PINS[key]) pins.led[key] = pin
  }
  const wanted = new Map<number, number>()
  for (const entry of file.pins?.buttons ?? []) {
    wanted.set(design.buttons.find((b) => b.id === entry.id)!.uid, entry.pin)
  }
  // one button taking a different pin can move another's default, so settle it
  for (let round = 0; round < 8; round++) {
    const plan = planPins(design, mcu, screen)
    let settled = true
    for (const [uid, pin] of wanted) {
      if (pins.buttons[uid] === undefined && plan.buttonPins.get(uid) !== pin) {
        pins.buttons[uid] = pin
        settled = false
      }
    }
    if (settled) break
  }
  return { design }
}
