// Turns a device profile's physical button positions (xMm/yMm) into a CSS
// grid: which column/row each button sits in, and how many columns/rows the
// grid needs. Mirrors mirrorGrid() in Designer/src/lib/layout.ts — the
// Designer already solved "snap continuous mm positions onto a discrete
// grid" for its own on-screen preview, so the Editor reuses the same idea
// (distinct rounded x/y values become grid lines) rather than inventing a
// second algorithm. For the reference 13-button device this reproduces
// today's layout exactly (nav row on top, two 5-wide performance rows below).

import screens from '../generated/screens.json'
import type { DeviceFile, LedSide } from '../types/device'

export interface BoardSlot {
  id: number
  col: number
  row: number
  led: LedSide
  nav: boolean
}

export interface BoardLayout {
  cols: number
  rows: number
  slots: BoardSlot[]
}

export interface ScreenCell {
  id: number
  /** 1-based half-column the cell starts in; it spans two (`${col} / span 2`). */
  col: number
  row: number
}

export interface ScreenLayout {
  /** Half-columns across: every cell is two wide, so `cols / 2` full cells fit. */
  cols: number
  rows: number
  cells: ScreenCell[]
}

/** Buttons whose centres are this close vertically (mm) share a screen row. */
const ROW_TOLERANCE_MM = 5
/** Neighbour gaps narrower than this (mm) are overlaps, not a spacing to learn from. */
const MIN_PITCH_MM = 8

/** Distinct rounded values, ascending — becomes a grid's line positions (1-indexed via indexOf + 1). */
function axis(values: number[]): number[] {
  return [...new Set(values.map((v) => Math.round(v)))].sort((a, b) => a - b)
}

/**
 * Every physical button (performance and navigation) placed on a grid sized
 * to the device's own distinct x/y positions. Ascending y = top to bottom,
 * matching both the physical unit (smaller yMm is closer to the top edge)
 * and how xMm/yMm are defined in config/design-schema.json.
 */
export function deviceBoardLayout(device: DeviceFile): BoardLayout {
  const xs = axis(device.buttons.map((b) => b.xMm))
  const ys = axis(device.buttons.map((b) => b.yMm))
  const navIds = new Set(Object.values(device.navigation))
  const slots = device.buttons.map((b) => ({
    id: b.id,
    col: xs.indexOf(Math.round(b.xMm)) + 1,
    row: ys.indexOf(Math.round(b.yMm)) + 1,
    led: b.led,
    nav: navIds.has(b.id),
  }))
  return { cols: xs.length, rows: ys.length, slots }
}

/**
 * The on-screen mini-grid: performance buttons only (the three navigation
 * ids get their own dedicated row on the screen — see
 * device-screen__navrow in DeviceScreen.vue — not a cell in this grid).
 *
 * Mirrors mirrorGrid() in Designer/src/lib/layout.ts, and what the firmware
 * draws from SCREEN_GRID_BUTTON_IDS: every cell is the same width — one button
 * spacing, learnt from the closest neighbours in a row — and a row staggered
 * against its neighbours by half a spacing is drawn half a cell along, so the
 * screen resembles the unit's face while each cell keeps as much room for its
 * name as possible. Each button snaps to the nearest half-spacing from the
 * left-most one.
 */
export function deviceScreenLayout(device: DeviceFile): ScreenLayout {
  const navIds = new Set(Object.values(device.navigation))
  const performance = device.buttons.filter((b) => !navIds.has(b.id))
  if (performance.length === 0) return { cols: 0, rows: 0, cells: [] }

  const byY = [...performance].sort((a, b) => a.yMm - b.yMm)
  const rows: (typeof performance)[] = []
  for (const button of byY) {
    const row = rows[rows.length - 1]
    if (row && button.yMm - row[0]!.yMm <= ROW_TOLERANCE_MM) row.push(button)
    else rows.push([button])
  }

  // Neighbour gaps within a row; a layout with one button per row has none, so
  // fall back to the gaps between distinct columns (which then never stagger).
  const gapsOf = (xs: number[]) => {
    const sorted = [...xs].sort((a, b) => a - b)
    return sorted.slice(1).map((x, i) => x - sorted[i]!).filter((d) => d >= MIN_PITCH_MM)
  }
  let gaps = rows.flatMap((row) => gapsOf(row.map((b) => b.xMm)))
  if (gaps.length === 0) gaps = gapsOf(performance.map((b) => b.xMm))
  let pitch = 1
  if (gaps.length > 0) {
    const smallest = Math.min(...gaps)
    // Average the single-spacing gaps so a long row's snapping does not drift.
    const singles = gaps.filter((d) => d <= smallest * 1.15)
    pitch = singles.reduce((sum, d) => sum + d, 0) / singles.length
  }

  const left = Math.min(...performance.map((b) => b.xMm))
  const cells: ScreenCell[] = []
  rows.forEach((row, rowIndex) => {
    for (const button of row) {
      cells.push({
        id: button.id,
        col: Math.round(((button.xMm - left) / pitch) * 2) + 1,
        row: rowIndex + 1,
      })
    }
  })
  return { cols: Math.max(...cells.map((c) => c.col)) + 1, rows: rows.length, cells }
}

/** Button diameter (mm) by purpose — the same drawing assumption the Designer uses. */
const BUTTON_MM = { foot: 16, finger: 12 } as const
/** Used when a device names a screen the bundled catalogue does not know (ILI9341 2.8"). */
const FALLBACK_SCREEN = { moduleMm: [64, 47], activeMm: [57.5, 43] }

export interface DeviceGeometry {
  widthMm: number
  heightMm: number
  buttonMm: number
  /** Each button's centre as a percentage of the unit's width / height. */
  buttons: { id: number; xPct: number; yPct: number }[]
  screen: {
    /** The visible window (bevel included): top-left, width and width / height. */
    xPct: number
    yPct: number
    widthPct: number
    aspect: number
    /** The lit display's width as a fraction of the window's width, and its width / height. */
    activeFrac: number
    activeAspect: number
    /** The screen's width in pixels. */
    lx: number
  }
}

/**
 * The unit drawn to scale: every position and size as a percentage of the
 * unit's face, so the Editor's device panel matches the Designer's plan view
 * and the screen can never land on a button the Designer placed clear of it.
 */
export function deviceGeometry(device: DeviceFile): DeviceGeometry {
  const { widthMm, heightMm } = device.unit
  const spec = screens.find((s) => s.id === device.screen.catalogId) ?? FALLBACK_SCREEN
  const flip = <T,>(pair: T[]) => (device.screen.orientation === 'portrait' ? [pair[1]!, pair[0]!] : pair)
  const [mw, mh] = flip(spec.moduleMm as number[]) as [number, number]
  const [aw, ah] = flip(spec.activeMm as number[]) as [number, number]
  const lx = flip((spec as { resolution?: number[] }).resolution ?? [320, 240])[0]!
  return {
    widthMm,
    heightMm,
    buttonMm: BUTTON_MM[device.purpose],
    buttons: device.buttons.map((b) => ({ id: b.id, xPct: (b.xMm / widthMm) * 100, yPct: (b.yMm / heightMm) * 100 })),
    screen: {
      xPct: (device.screen.xMm / widthMm) * 100,
      yPct: (device.screen.yMm / heightMm) * 100,
      widthPct: (mw / widthMm) * 100,
      aspect: mw / mh,
      activeFrac: aw / mw,
      activeAspect: aw / ah,
      lx,
    },
  }
}
