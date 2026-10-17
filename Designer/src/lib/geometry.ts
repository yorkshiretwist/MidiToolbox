// Plan-view geometry. Everything is in millimetres, origin at the unit's
// top-left corner, x to the right, y downwards.

import type { Design, DesignButton, Orientation, Purpose, ScreenSpec } from '../types/device'

/**
 * Diameter of a button, in mm, by how the unit is operated. Foot switches are
 * chunky; finger buttons can be smaller because they need not be so robust.
 * These are drawing assumptions, not measurements of a real part.
 */
export const BUTTON_DIAMETERS: Record<Purpose, number> = { foot: 16, finger: 12 }

/** Colour of the unit's face when a device file doesn't say: very dark grey. */
export const DEFAULT_UNIT_COLOUR: readonly number[] = [38, 38, 38]

export function buttonDiameter(purpose: Purpose): number {
  return BUTTON_DIAMETERS[purpose]
}

// The indicator LED scales with its button: 5 mm across, 13 mm from the centre
// of a 16 mm button.
export const ledDiameter = (diameter: number) => (diameter * 5) / 16
export const ledOffset = (diameter: number) => (diameter * 13) / 16

export const MIN_UNIT_SIZE = 50
export const MAX_UNIT_SIZE = 3000
export const GRID_PITCHES = [5, 10, 20, 25, 50]

export interface Rect {
  x0: number
  y0: number
  x1: number
  y1: number
}

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value))
}

/** Rounds away floating-point noise (0.1 + 0.2) without hiding real decimals. */
export function tidy(value: number): number {
  return Math.round(value * 100) / 100
}

export function snapTo(value: number, pitch: number): number {
  return tidy(Math.round(value / pitch) * pitch)
}

export function buttonInsideUnit(button: DesignButton, unit: Design['unit'], diameter: number): boolean {
  const r = diameter / 2
  return button.x - r >= 0 && button.y - r >= 0 && button.x + r <= unit.width && button.y + r <= unit.height
}

/** Clear space between two buttons' edges (negative = overlapping). */
export function buttonGap(a: DesignButton, b: DesignButton, diameter: number): number {
  return Math.hypot(a.x - b.x, a.y - b.y) - diameter
}

export function screenDims(spec: ScreenSpec, orientation: Orientation) {
  const swap = orientation === 'portrait'
  const flip = <T extends [number, number]>(pair: T): [number, number] =>
    swap ? [pair[1], pair[0]] : [pair[0], pair[1]]
  return {
    module: flip(spec.moduleMm),
    active: flip(spec.activeMm),
    resolution: flip(spec.resolution),
  }
}

export function screenRect(design: Design, spec: ScreenSpec): Rect {
  const { module } = screenDims(spec, design.screen.orientation)
  return {
    x0: design.screen.x,
    y0: design.screen.y,
    x1: design.screen.x + module[0],
    y1: design.screen.y + module[1],
  }
}

export function screenInsideUnit(rect: Rect, unit: Design['unit']): boolean {
  return rect.x0 >= 0 && rect.y0 >= 0 && rect.x1 <= unit.width && rect.y1 <= unit.height
}

/** True if the button's dome touches the rectangle. */
export function buttonHitsRect(button: DesignButton, rect: Rect, diameter: number): boolean {
  const r = diameter / 2
  const nearestX = clamp(button.x, rect.x0, rect.x1)
  const nearestY = clamp(button.y, rect.y0, rect.y1)
  return Math.hypot(button.x - nearestX, button.y - nearestY) < r
}

/** Positions (mm) for the ruler labels: a multiple of the grid pitch, few enough to read. */
export function rulerTicks(extent: number, pitch: number, maxLabels: number): number[] {
  const steps = [5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000]
  const step = steps.find((s) => s >= pitch && s % pitch === 0 && extent / s <= maxLabels) ?? 2000
  const ticks: number[] = []
  for (let at = 0; at <= extent; at += step) ticks.push(at)
  return ticks
}
