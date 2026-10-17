// Tests for lib/deviceLayout.ts: turning a device profile's button positions into grid layouts.
import { describe, expect, it } from 'vitest'
import { deviceBoardLayout, deviceScreenLayout } from './deviceLayout'
import { liveRigDevice } from '../data/liveRigDevice'
import type { DeviceFile } from '../types/device'

function slotFor(layout: ReturnType<typeof deviceBoardLayout>, id: number) {
  return layout.slots.find((s) => s.id === id)
}

describe('deviceBoardLayout', () => {
  it('reproduces the reference device’s known board layout exactly', () => {
    const layout = deviceBoardLayout(liveRigDevice)
    expect(layout.cols).toBe(5)
    expect(layout.rows).toBe(3)
    // bottom row (ids 0-4): row 3, cols 1-5
    expect(slotFor(layout, 0)).toMatchObject({ row: 3, col: 1, led: 'above', nav: false })
    expect(slotFor(layout, 4)).toMatchObject({ row: 3, col: 5, nav: false })
    // middle row (ids 5-9): row 2, cols 1-5
    expect(slotFor(layout, 5)).toMatchObject({ row: 2, col: 1, led: 'below', nav: false })
    expect(slotFor(layout, 9)).toMatchObject({ row: 2, col: 5, nav: false })
    // nav row (ids 10-12): row 1, cols 3-5
    expect(slotFor(layout, 10)).toMatchObject({ row: 1, col: 3, nav: true })
    expect(slotFor(layout, 11)).toMatchObject({ row: 1, col: 4, nav: true })
    expect(slotFor(layout, 12)).toMatchObject({ row: 1, col: 5, nav: true })
  })

  it('grids an irregular device by its own distinct x/y positions', () => {
    const odd: DeviceFile = {
      ...liveRigDevice,
      buttons: [
        { id: 0, xMm: 10, yMm: 50, led: 'above' },
        { id: 1, xMm: 60, yMm: 50, led: 'above' },
        { id: 2, xMm: 10, yMm: 100, led: 'above' },
        { id: 3, xMm: 60, yMm: 100, led: 'above' },
        { id: 8, xMm: 10, yMm: 10, led: 'above' }, // nav, its own row
        { id: 9, xMm: 60, yMm: 10, led: 'above' },
        { id: 7, xMm: 35, yMm: 10, led: 'above' },
      ],
      navigation: { select: 7, down: 8, up: 9 },
    }
    const layout = deviceBoardLayout(odd)
    expect(layout.cols).toBe(3) // distinct x: 10, 35, 60
    expect(layout.rows).toBe(3) // distinct y: 10, 50, 100
    expect(slotFor(layout, 8)).toMatchObject({ row: 1, col: 1, nav: true })
    expect(slotFor(layout, 7)).toMatchObject({ row: 1, col: 2, nav: true })
    expect(slotFor(layout, 0)).toMatchObject({ row: 2, col: 1 })
    expect(slotFor(layout, 3)).toMatchObject({ row: 3, col: 3 })
  })
})

describe('deviceScreenLayout', () => {
  it('reproduces the reference device’s known screen cell order', () => {
    const layout = deviceScreenLayout(liveRigDevice)
    expect(layout.cols).toBe(10) // 5 cells, each two half-columns wide
    expect(layout.rows).toBe(2)
    expect(layout.cells.map((c) => c.id).sort((a, b) => a - b)).toEqual([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
    // middle board row (ids 5-9) is screen row 1 — it mirrors the board, and
    // sits physically above the bottom row on the actual unit.
    expect(layout.cells.find((c) => c.id === 5)).toMatchObject({ row: 1, col: 1 })
    expect(layout.cells.find((c) => c.id === 0)).toMatchObject({ row: 2, col: 1 })
    // navigation ids never appear as screen cells — they have their own row.
    expect(layout.cells.some((c) => c.id === 10 || c.id === 11 || c.id === 12)).toBe(false)
    // cells sit on every other half-column
    expect(layout.cells.find((c) => c.id === 4)).toMatchObject({ row: 2, col: 9 })
  })

  it('draws a staggered row half a cell along, at full cell width', () => {
    // 3 buttons across the bottom at a 40 mm spacing, 2 above them offset by half
    const staggered: DeviceFile = {
      ...liveRigDevice,
      buttons: [
        { id: 0, xMm: 20, yMm: 100, led: 'above' },
        { id: 1, xMm: 60, yMm: 100, led: 'above' },
        { id: 2, xMm: 100, yMm: 100, led: 'above' },
        { id: 3, xMm: 40, yMm: 60, led: 'above' },
        { id: 4, xMm: 80, yMm: 60, led: 'above' },
        { id: 10, xMm: 20, yMm: 10, led: 'above' },
        { id: 11, xMm: 60, yMm: 10, led: 'above' },
        { id: 12, xMm: 100, yMm: 10, led: 'above' },
      ],
      navigation: { select: 10, down: 11, up: 12 },
    }
    const layout = deviceScreenLayout(staggered)
    expect(layout.cols).toBe(6) // 3 cells across; the staggered pair sits inside that span
    expect(layout.rows).toBe(2)
    const at = (id: number) => layout.cells.find((c) => c.id === id)
    expect(at(0)).toMatchObject({ row: 2, col: 1 })
    expect(at(2)).toMatchObject({ row: 2, col: 5 })
    expect(at(3)).toMatchObject({ row: 1, col: 2 })
    expect(at(4)).toMatchObject({ row: 1, col: 4 })
  })

  it('does not grow extra columns for a row a few mm out of line', () => {
    const wobbly: DeviceFile = {
      ...liveRigDevice,
      buttons: [
        { id: 0, xMm: 20, yMm: 100, led: 'above' },
        { id: 1, xMm: 100, yMm: 100, led: 'above' },
        { id: 2, xMm: 60, yMm: 97, led: 'above' },
        { id: 3, xMm: 141, yMm: 97, led: 'above' },
        { id: 10, xMm: 20, yMm: 10, led: 'above' },
        { id: 11, xMm: 60, yMm: 10, led: 'above' },
        { id: 12, xMm: 100, yMm: 10, led: 'above' },
      ],
      navigation: { select: 10, down: 11, up: 12 },
    }
    // ids 0-3 are one row (3 mm apart) - the old rounding made two rows
    expect(deviceScreenLayout(wobbly).rows).toBe(1)
  })
})
