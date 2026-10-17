// Comfortable-spacing guidance. It only ever produces warnings: a design that
// breaks it can still be exported. Distances are between button centres, so a
// 50 mm rule with 16 mm foot buttons leaves 34 mm of clear space between two.
// The unit's own edge is deliberately not checked: buttons at the edge are
// bounded by the edge itself.

import type { Design, Purpose } from '../types/device'
import { buttonDiameter } from './geometry'

/** Minimum distance between the centres of two neighbouring buttons, in mm. */
export const REQUIRED_SPACING: Record<Purpose, number> = { foot: 50, finger: 20 }

export interface SpacingViolation {
  a: number
  b: number
  /** Actual distance between the two buttons' centres, mm. */
  distance: number
}

/** Every pair of buttons closer than the spacing for the design's purpose, tightest first. */
export function spacingViolations(design: Design): SpacingViolation[] {
  const required = REQUIRED_SPACING[design.purpose]
  const diameter = buttonDiameter(design.purpose)
  const found: SpacingViolation[] = []
  const buttons = design.buttons
  for (let i = 0; i < buttons.length; i++) {
    for (let j = i + 1; j < buttons.length; j++) {
      const a = buttons[i]!
      const b = buttons[j]!
      const distance = Math.hypot(a.x - b.x, a.y - b.y)
      // an overlap is reported as an error elsewhere; don't double up on it here
      if (distance >= diameter && distance < required) found.push({ a: a.uid, b: b.uid, distance })
    }
  }
  return found.sort((x, y) => x.distance - y.distance)
}
