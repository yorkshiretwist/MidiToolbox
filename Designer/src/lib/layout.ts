// Layout maths: navigation roles, the on-screen button grid, id numbering,
// quick grids and screen suggestions.

import type { Design, DesignButton, NavRole, Numbering, Orientation, ScreenSpec } from '../types/device'
import {
  buttonDiameter,
  buttonHitsRect,
  buttonInsideUnit,
  screenDims,
  screenRect,
  snapTo,
  tidy,
} from './geometry'

export const NAV_ROLES: NavRole[] = ['select', 'down', 'up']

/** Smallest a button cell may be, in screen pixels, before it is hard to read. */
export const MIN_CELL_WIDTH = 44
export const MIN_CELL_HEIGHT = 40

// Screen.h lays the display out as header (28) + sub-header (40) + gap (4) +
// parameter row (34) + gap (4), then the button grid with a 4 px gutter.
const UI_TOP = 110
const UI_GAP = 4

export function navUids(design: Design): number[] {
  return NAV_ROLES.map((role) => design.nav[role])
}

export function isNavButton(design: Design, uid: number): boolean {
  return navUids(design).includes(uid)
}

export function navRoleOf(design: Design, uid: number): NavRole | undefined {
  return NAV_ROLES.find((role) => design.nav[role] === uid)
}

export function performanceButtons(design: Design): DesignButton[] {
  const nav = navUids(design)
  return design.buttons.filter((b) => !nav.includes(b.uid))
}

export function buttonByUid(design: Design, uid: number): DesignButton | undefined {
  return design.buttons.find((b) => b.uid === uid)
}

// ── The on-screen grid mirrors the performance buttons ─────────────────────

/** Buttons whose centres are this close vertically (mm) belong to the same screen row. */
const ROW_TOLERANCE_MM = 5
/** Gaps between neighbours narrower than this (mm) are overlaps, not a spacing to learn from. */
const MIN_PITCH_MM = 8
/** How far (in half-pitch units) a button may sit from a half-column before it is flagged. */
const HALF_SNAP_TOLERANCE = 0.4

export interface MirrorCell {
  uid: number
  /** Zero-based half-column this cell starts in. The cell spans two half-columns. */
  halfCol: number
  /** Zero-based row, top to bottom. */
  row: number
}

export interface MirrorGrid {
  /** Total half-columns across. Every cell is two wide, so `halfCols / 2` full cells fit. */
  halfCols: number
  rows: number
  cells: MirrorCell[]
  /** Buttons further than 20% of a button spacing from a half-column: moved to the nearest. */
  offGrid: number[]
  /** Buttons whose cells would overlap another cell on the screen. */
  clashes: number[]
}

/** How many full-width cells span the screen: a half-integer such as 5.5 for a staggered layout. */
export function gridCols(grid: MirrorGrid): number {
  return grid.halfCols / 2
}

/**
 * The on-screen arrangement of the performance buttons. Every cell is the same
 * width — one button spacing — and a row that is staggered against its
 * neighbours (offset by half a spacing, as on a keyboard) is drawn offset by
 * half a cell, so the screen looks like the unit's face while each cell keeps as
 * much room for its name as possible.
 *
 * The spacing is learnt from the closest neighbours in a row. Each button is then
 * snapped to the nearest half-spacing from the left-most one.
 */
export function mirrorGrid(buttons: DesignButton[]): MirrorGrid {
  if (buttons.length === 0) return { halfCols: 0, rows: 0, cells: [], offGrid: [], clashes: [] }

  const byY = [...buttons].sort((a, b) => a.y - b.y)
  const rows: DesignButton[][] = []
  for (const button of byY) {
    const row = rows[rows.length - 1]
    if (row && button.y - row[0]!.y <= ROW_TOLERANCE_MM) row.push(button)
    else rows.push([button])
  }

  // Neighbour gaps within a row. A layout with one button per row has none, so
  // fall back to the gaps between distinct columns (which then never stagger).
  const gapsOf = (xs: number[]) => {
    const sorted = [...xs].sort((a, b) => a - b)
    return sorted.slice(1).map((x, i) => x - sorted[i]!).filter((d) => d >= MIN_PITCH_MM)
  }
  let gaps = rows.flatMap((row) => gapsOf(row.map((b) => b.x)))
  if (gaps.length === 0) gaps = gapsOf(buttons.map((b) => b.x))
  let pitch = 1
  if (gaps.length > 0) {
    const smallest = Math.min(...gaps)
    // Average the single-spacing gaps so a long row's snapping does not drift.
    const singles = gaps.filter((d) => d <= smallest * 1.15)
    pitch = singles.reduce((sum, d) => sum + d, 0) / singles.length
  }

  const left = Math.min(...buttons.map((b) => b.x))
  const cells: MirrorCell[] = []
  const offGrid: number[] = []
  rows.forEach((row, rowIndex) => {
    for (const button of row) {
      const units = ((button.x - left) / pitch) * 2
      const halfCol = Math.round(units)
      if (Math.abs(units - halfCol) > HALF_SNAP_TOLERANCE) offGrid.push(button.uid)
      cells.push({ uid: button.uid, halfCol, row: rowIndex })
    }
  })

  const clashes = new Set<number>()
  rows.forEach((_, rowIndex) => {
    const inRow = cells.filter((c) => c.row === rowIndex).sort((a, b) => a.halfCol - b.halfCol)
    for (let i = 1; i < inRow.length; i++) {
      if (inRow[i]!.halfCol - inRow[i - 1]!.halfCol < 2) {
        clashes.add(inRow[i - 1]!.uid)
        clashes.add(inRow[i]!.uid)
      }
    }
  })

  const halfCols = Math.max(...cells.map((c) => c.halfCol)) + 2
  return { halfCols, rows: rows.length, cells, offGrid, clashes: [...clashes] }
}

/**
 * Pixel size of one button cell on the given screen. `cols` is how many
 * full-width cells span the screen, so it is fractional (5.5) when a staggered
 * row sticks out by half a cell.
 */
export function cellSize(
  spec: ScreenSpec,
  orientation: Orientation,
  cols: number,
  rows: number,
): { w: number; h: number } {
  const [lx, ly] = screenDims(spec, orientation).resolution
  const c = Math.max(1, cols)
  const r = Math.max(1, rows)
  return {
    w: (lx - UI_GAP * (c + 1)) / c,
    h: (ly - UI_TOP - UI_GAP * r) / r,
  }
}

export function screenFits(
  spec: ScreenSpec,
  orientation: Orientation,
  cols: number,
  rows: number,
): boolean {
  const cell = cellSize(spec, orientation, cols, rows)
  return cell.w >= MIN_CELL_WIDTH && cell.h >= MIN_CELL_HEIGHT
}

/**
 * The smallest screen whose button cells stay legible: verified screens win
 * over untested ones, then the smaller visible area wins. When nothing fits the
 * largest screen is returned.
 */
export function suggestScreen(
  screens: ScreenSpec[],
  orientation: Orientation,
  cols: number,
  rows: number,
): ScreenSpec {
  const area = (s: ScreenSpec) => s.activeMm[0] * s.activeMm[1]
  const fitting = screens.filter((s) => screenFits(s, orientation, cols, rows))
  if (fitting.length === 0) return [...screens].sort((a, b) => area(b) - area(a))[0]!
  const verified = fitting.filter((s) => s.status === 'verified')
  const pool = verified.length > 0 ? verified : fitting
  return [...pool].sort((a, b) => area(a) - area(b))[0]!
}

// ── Numbering ──────────────────────────────────────────────────────────────

/**
 * Computes button ids from position. Buttons are grouped into rows (buttons
 * whose centres are within half a dome of each other vertically), then numbered
 * left to right within each row. `bottom-left` starts at the bottom row and
 * moves up; `top-left` starts at the top row and moves down.
 */
export function computeIds(
  buttons: DesignButton[],
  numbering: Exclude<Numbering, 'manual'>,
  diameter: number,
): Map<number, number> {
  const bottomFirst = numbering === 'bottom-left'
  const sorted = [...buttons].sort((a, b) => (bottomFirst ? b.y - a.y : a.y - b.y))
  const rows: DesignButton[][] = []
  for (const button of sorted) {
    const row = rows[rows.length - 1]
    if (row && Math.abs(row[0]!.y - button.y) <= diameter / 2) row.push(button)
    else rows.push([button])
  }
  const ids = new Map<number, number>()
  let next = 0
  for (const row of rows) {
    row.sort((a, b) => a.x - b.x)
    for (const button of row) ids.set(button.uid, next++)
  }
  return ids
}

/** Re-numbers in place when the design uses an automatic numbering scheme. */
export function applyNumbering(design: Design): void {
  if (design.numbering === 'manual') return
  const ids = computeIds(design.buttons, design.numbering, buttonDiameter(design.purpose))
  for (const button of design.buttons) button.id = ids.get(button.uid) ?? button.id
}

// ── Adding buttons ─────────────────────────────────────────────────────────

/** True if a dome centred here would sit inside the unit and touch nothing. */
export function spotIsFree(design: Design, spec: ScreenSpec, x: number, y: number, ignoreUid?: number): boolean {
  const diameter = buttonDiameter(design.purpose)
  const probe: DesignButton = { uid: -1, id: -1, x, y, led: 'above' }
  if (!buttonInsideUnit(probe, design.unit, diameter)) return false
  if (buttonHitsRect(probe, screenRect(design, spec), diameter)) return false
  return design.buttons.every(
    (b) => b.uid === ignoreUid || Math.hypot(b.x - x, b.y - y) >= diameter,
  )
}

/** The first free grid node scanning left to right, top to bottom. */
export function firstFreeNode(design: Design, spec: ScreenSpec): { x: number; y: number } | null {
  const { pitch, width, height } = design.unit
  for (let y = pitch; y < height; y += pitch) {
    for (let x = pitch; x < width; x += pitch) {
      if (spotIsFree(design, spec, tidy(x), tidy(y))) return { x: tidy(x), y: tidy(y) }
    }
  }
  return null
}

export interface QuickGridOptions {
  columns: number
  rows: number
  /** Centre-to-centre distance, mm. */
  spacing: number
  /** Enlarge the unit if the grid does not fit. */
  grow: boolean
}

/**
 * Lays out `columns` × `rows` positions along the bottom of the unit, centred
 * horizontally, snapped to the grid. Returns the positions and the unit size
 * needed to hold them (grown only when `grow` is set).
 */
export function quickGridPositions(
  design: Design,
  options: QuickGridOptions,
): { positions: { x: number; y: number }[]; unit: Design['unit'] } {
  const { pitch } = design.unit
  const margin = Math.ceil((buttonDiameter(design.purpose) / 2 + 4) / pitch) * pitch
  const blockW = (options.columns - 1) * options.spacing
  const blockH = (options.rows - 1) * options.spacing
  let { width, height } = design.unit
  if (options.grow) {
    width = Math.max(width, snapTo(blockW + margin * 2, pitch) + pitch)
    // keep the top 100 mm clear for the screen and navigation buttons
    height = Math.max(height, snapTo(blockH + margin * 2 + 100, pitch) + pitch)
  }
  const left = snapTo((width - blockW) / 2, pitch)
  const bottom = snapTo(height - margin, pitch)
  const top = bottom - blockH
  const positions: { x: number; y: number }[] = []
  for (let r = 0; r < options.rows; r++) {
    for (let c = 0; c < options.columns; c++) {
      positions.push({ x: tidy(left + c * options.spacing), y: tidy(top + r * options.spacing) })
    }
  }
  return { positions, unit: { width, height, pitch } }
}
