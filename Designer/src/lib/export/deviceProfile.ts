// The device.json the firmware reads from the SD card at boot: the resolved wiring
// of the unit, in the same terms as the generated MidiToolboxDevice.h but as data.
// Shape defined by config/device-schema.json. The Designer's own design file
// (<name>-design.json, config/design-schema.json) is a different file: it keeps the
// millimetre layout the Designer needs and leaves the pin plan implicit.

import type { Design } from '../../types/device'
import type { Analysis } from '../checks'
import { screenDims } from '../geometry'
import { gridCellRows } from './firmware'

export interface DeviceProfile {
  schemaVersion: 1
  name: string
  microcontroller: string
  buttons: {
    pins: number[]
    navigation: { select: number; down: number; up: number }
  }
  screen: {
    driver: string
    width: number
    height: number
    orientation: 'landscape' | 'portrait'
    touch: string | null
    pins: Record<string, number>
    grid: { rows: number; halfCols: number; cells: number[][] }
  }
  leds: { registers: number }
}

export function buildDeviceProfile(design: Design, analysis: Analysis): DeviceProfile {
  const { plan, screen, mcu, grid } = analysis
  const idOf = new Map(design.buttons.map((b) => [b.uid, b.id]))
  const dims = screenDims(screen, design.screen.orientation)
  // -1 marks a button the plan ran out of pins for; the schema rejects it, and the
  // pin-budget check has already blocked the downloads in that case
  const pins = [...design.buttons].sort((a, b) => a.id - b.id).map((b) => plan.buttonPins.get(b.uid) ?? -1)

  return {
    schemaVersion: 1,
    name: design.name,
    microcontroller: mcu.name,
    buttons: {
      pins,
      navigation: {
        select: idOf.get(design.nav.select) ?? 0,
        down: idOf.get(design.nav.down) ?? 0,
        up: idOf.get(design.nav.up) ?? 0,
      },
    },
    screen: {
      driver: screen.driver,
      width: dims.resolution[0],
      height: dims.resolution[1],
      orientation: design.screen.orientation,
      touch: screen.touch.type === 'none' ? null : (screen.touch.controller?.toUpperCase() ?? null),
      pins: Object.fromEntries(plan.screenPins.map((p) => [p.role.toUpperCase(), p.pin])),
      grid: { rows: grid.rows, halfCols: grid.halfCols, cells: gridCellRows(design, analysis) },
    },
    leds: { registers: plan.registers },
  }
}

export function serialiseDeviceProfile(profile: DeviceProfile): string {
  return JSON.stringify(profile, null, 2) + '\n'
}
