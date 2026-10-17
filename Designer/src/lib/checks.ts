// Turns a design into a list of findings. Errors make the design unbuildable
// and disable downloads; warnings are advice and never block anything.

import type { Design, MicrocontrollerSpec, ScreenSpec } from '../types/device'
import { requireMicrocontroller, requireScreen } from './catalog'
import {
  buttonDiameter,
  buttonGap,
  buttonHitsRect,
  buttonInsideUnit,
  screenInsideUnit,
  screenRect,
} from './geometry'
import { cellSize, gridCols, mirrorGrid, NAV_ROLES, performanceButtons, screenFits, type MirrorGrid } from './layout'
import { DEFAULT_LED_PINS, LED_PIN_KEYS, planPins, type PinPlan } from './pins'
import { REQUIRED_SPACING, spacingViolations, type SpacingViolation } from './spacing'

export type Severity = 'ok' | 'warn' | 'error'

export interface Check {
  key: string
  severity: Severity
  /** The design step (1-5) this finding belongs to. */
  step: number
  text: string
}

export interface Analysis {
  checks: Check[]
  errors: number
  warnings: number
  /** True when there is at least one error: downloads are disabled. */
  blocked: boolean
  screen: ScreenSpec
  mcu: MicrocontrollerSpec
  plan: PinPlan
  /** Buttons involved in an error (overlap, outside the unit, under the screen). */
  conflictUids: Set<number>
  /** Buttons involved in a too-close-together warning. */
  tightUids: Set<number>
  screenConflict: boolean
  violations: SpacingViolation[]
  grid: MirrorGrid
}

const printed = (id: number) => id + 1

function listNumbers(numbers: (number | string)[]): string {
  if (numbers.length <= 3) return numbers.join(', ')
  return `${numbers.slice(0, 3).join(', ')} and ${numbers.length - 3} more`
}

export function analyse(design: Design): Analysis {
  const screen = requireScreen(design.screen.catalogId)
  const mcu = requireMicrocontroller(design.microcontroller)
  const plan = planPins(design, mcu, screen)
  const diameter = buttonDiameter(design.purpose)
  const perf = performanceButtons(design)
  const grid = mirrorGrid(perf)
  const checks: Check[] = []
  const add = (key: string, severity: Severity, step: number, text: string) =>
    checks.push({ key, severity, step, text })
  const idOf = new Map(design.buttons.map((b) => [b.uid, b.id]))

  // ── Unit: comfortable spacing (warning only) ─────────────────────────────
  const required = REQUIRED_SPACING[design.purpose]
  const violations = spacingViolations(design)
  const tightUids = new Set<number>()
  for (const v of violations) {
    tightUids.add(v.a)
    tightUids.add(v.b)
  }
  if (violations.length > 0) {
    const worst = violations[0]!
    add(
      'spacing',
      'warn',
      1,
      `${violations.length} pair${violations.length === 1 ? '' : 's'} of buttons are closer than ${required} mm centre to centre ` +
        `(tightest: ${printed(idOf.get(worst.a)!)} and ${printed(idOf.get(worst.b)!)}, ${Math.round(worst.distance)} mm)`,
    )
  } else {
    add('spacing', 'ok', 1, `Buttons are at least ${required} mm apart, centre to centre`)
  }

  // ── Buttons ──────────────────────────────────────────────────────────────
  const conflictUids = new Set<number>()
  if (perf.length >= 3) add('perf', 'ok', 2, `${design.buttons.length} buttons placed (${perf.length} performance)`)
  else add('perf', 'error', 2, `Needs at least 3 performance buttons (has ${perf.length})`)

  const overlaps: [number, number][] = []
  for (let i = 0; i < design.buttons.length; i++) {
    for (let j = i + 1; j < design.buttons.length; j++) {
      const a = design.buttons[i]!
      const b = design.buttons[j]!
      if (buttonGap(a, b, diameter) < 0) {
        overlaps.push([a.id, b.id])
        conflictUids.add(a.uid)
        conflictUids.add(b.uid)
      }
    }
  }
  if (overlaps.length > 0) {
    add('overlap', 'error', 2, `Buttons overlap: ${listNumbers(overlaps.map(([a, b]) => `${printed(a)}/${printed(b)}`))}`)
  } else {
    add('overlap', 'ok', 2, 'No two buttons overlap')
  }

  const outside = design.buttons.filter((b) => !buttonInsideUnit(b, design.unit, diameter))
  outside.forEach((b) => conflictUids.add(b.uid))
  if (outside.length > 0) add('outside', 'error', 2, `Outside the unit: button ${listNumbers(outside.map((b) => printed(b.id)))}`)
  else add('outside', 'ok', 2, 'All buttons are inside the unit')

  // ── Navigation ───────────────────────────────────────────────────────────
  const navOk = NAV_ROLES.every((role) => design.buttons.some((b) => b.uid === design.nav[role]))
  add('nav', navOk ? 'ok' : 'error', 3, navOk ? 'Three different buttons run navigation' : 'A navigation role has no button')

  // ── Screen ───────────────────────────────────────────────────────────────
  const rect = screenRect(design, screen)
  let screenConflict = false
  if (!screenInsideUnit(rect, design.unit)) {
    screenConflict = true
    add('screen-outside', 'error', 4, 'The screen is not fully inside the unit')
  }
  const under = design.buttons.filter((b) => buttonHitsRect(b, rect, diameter))
  if (under.length > 0) {
    screenConflict = true
    under.forEach((b) => conflictUids.add(b.uid))
    add('screen-overlap', 'error', 4, `The screen overlaps button ${listNumbers(under.map((b) => printed(b.id)))}`)
  }
  if (!screenConflict) add('screen-place', 'ok', 4, 'The screen fits without overlaps')

  const cols = Math.max(1, gridCols(grid))
  const rows = Math.max(1, grid.rows)
  const idsOf = (uids: number[]) => listNumbers(uids.map((uid) => printed(idOf.get(uid)!)))
  if (grid.clashes.length > 0) {
    add('screen-clash', 'warn', 4, `On the screen, button ${idsOf(grid.clashes)} would overlap another cell: space those buttons out`)
  }
  if (grid.offGrid.length > 0) {
    add(
      'screen-offgrid',
      'warn',
      4,
      `Button ${idsOf(grid.offGrid)} ${grid.offGrid.length === 1 ? 'does' : 'do'} not line up with the others, so ${grid.offGrid.length === 1 ? 'its' : 'their'} on-screen position is approximate. ` +
        'Rows should either line up or be offset by half a button spacing',
    )
  }
  if (!screenFits(screen, design.screen.orientation, cols, rows)) {
    const cell = cellSize(screen, design.screen.orientation, cols, rows)
    add('screen-fit', 'warn', 4, `Button cells on ${screen.name} would be only ${Math.round(cell.w)}×${Math.round(cell.h)} px — hard to read`)
  }
  if (screen.status === 'untested') add('screen-status', 'warn', 4, `${screen.name} has no verified firmware driver yet`)
  else add('screen-status', 'ok', 4, `${screen.name} driver verified`)
  if (screen.touch.type === 'none') add('screen-touch', 'warn', 4, 'No touch: on-screen navigation is unavailable')
  else add('screen-touch', 'ok', 4, 'On-screen navigation available')

  // ── Teensy ───────────────────────────────────────────────────────────────
  if (plan.over) add('pins', 'error', 5, `Pin budget exceeded: ${plan.needed} needed, ${mcu.name} has ${plan.available}`)
  else add('pins', 'ok', 5, `Pin budget: ${plan.needed} of ${plan.available} on ${mcu.name}`)

  // Two connections on one pin is a wiring fault, but only a warning: the user
  // chose the pins, and can still export while they sort it out. Ground and
  // power are not pins in this model, so they are never flagged.
  if (plan.conflicts.length === 0) {
    add('pin-conflicts', 'ok', 5, 'No pin is used twice')
  } else {
    for (const conflict of plan.conflicts.slice(0, 4)) {
      add(`pin-conflict-${conflict.pin}`, 'warn', 5, `Pin ${conflict.pin} is used by ${conflict.owners.join(' and ')}`)
    }
    if (plan.conflicts.length > 4) {
      add('pin-conflicts-more', 'warn', 5, `${plan.conflicts.length - 4} more pins are used twice`)
    }
  }
  // The firmware compiles the LED chain's pins in (the ShiftPWM library cannot take
  // them from a file), so a design that moves them cannot be run by the stock firmware.
  const movedLed = LED_PIN_KEYS.filter((k) => plan.ledPins[k] !== DEFAULT_LED_PINS[k])
  if (movedLed.length > 0) {
    add(
      'led-pins',
      'error',
      5,
      `The firmware fixes the LED chain on pins ${LED_PIN_KEYS.map((k) => DEFAULT_LED_PINS[k]).join(', ')} (latch, data, clock): put the LED pins back to their defaults`,
    )
  }
  plan.busIssues.forEach((text, i) => add(`pin-bus-${i}`, 'warn', 5, text))
  if (plan.padPinsUsed.length > 0) {
    add(
      'pin-pads',
      'warn',
      5,
      `Pin${plan.padPinsUsed.length === 1 ? '' : 's'} ${plan.padPinsUsed.join(', ')} ${plan.padPinsUsed.length === 1 ? 'is a solder pad' : 'are solder pads'} on the underside of the ${mcu.name}, not edge pins`,
    )
  }

  const errors = checks.filter((c) => c.severity === 'error').length
  const warnings = checks.filter((c) => c.severity === 'warn').length
  return {
    checks,
    errors,
    warnings,
    blocked: errors > 0,
    screen,
    mcu,
    plan,
    conflictUids,
    tightUids,
    screenConflict,
    violations,
    grid,
  }
}

/** The worst severity among a step's checks (step 6 summarises everything). */
export function stepSeverity(analysis: Analysis, step: number): Severity {
  const relevant = step === 6 ? analysis.checks : analysis.checks.filter((c) => c.step === step)
  if (relevant.some((c) => c.severity === 'error')) return 'error'
  if (relevant.some((c) => c.severity === 'warn')) return 'warn'
  return 'ok'
}
