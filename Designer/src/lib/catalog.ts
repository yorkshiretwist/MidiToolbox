// The hand-maintained parts catalogue. Add, edit or remove a part by editing the
// JSON files in src/data/catalog/ — nothing else needs to change.

import screensJson from '../data/catalog/screens.json'
import microcontrollersJson from '../data/catalog/microcontrollers.json'
import type { MicrocontrollerSpec, ScreenSpec } from '../types/device'

export const SCREENS = screensJson as unknown as ScreenSpec[]
export const MICROCONTROLLERS = microcontrollersJson as unknown as MicrocontrollerSpec[]

export const DEFAULT_SCREEN_ID = 'ili9341-28'
export const DEFAULT_MICROCONTROLLER_ID = 'teensy-4.1'

export function findScreen(id: string): ScreenSpec | undefined {
  return SCREENS.find((s) => s.id === id)
}

export function findMicrocontroller(id: string): MicrocontrollerSpec | undefined {
  return MICROCONTROLLERS.find((m) => m.id === id)
}

export function requireScreen(id: string): ScreenSpec {
  const spec = findScreen(id)
  if (!spec) throw new Error(`Unknown screen "${id}"`)
  return spec
}

export function requireMicrocontroller(id: string): MicrocontrollerSpec {
  const spec = findMicrocontroller(id)
  if (!spec) throw new Error(`Unknown microcontroller "${id}"`)
  return spec
}
