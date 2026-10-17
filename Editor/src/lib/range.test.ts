// Tests for lib/range.ts: keeping typed numbers inside a field's range.
import { describe, expect, it } from 'vitest'
import { clampToRange, isInRange } from './range'

describe('clampToRange', () => {
  it('leaves a value inside the range alone, including both ends', () => {
    expect(clampToRange(0, 0, 127)).toBe(0)
    expect(clampToRange(64, 0, 127)).toBe(64)
    expect(clampToRange(127, 0, 127)).toBe(127)
  })

  it('pulls a value above the range down to the top, and one below up to the bottom', () => {
    expect(clampToRange(128, 0, 127)).toBe(127)
    expect(clampToRange(5000, 0, 127)).toBe(127)
    expect(clampToRange(-1, 0, 127)).toBe(0)
    expect(clampToRange(-300, 0, 127)).toBe(0)
  })

  it('rounds a fractional value to a whole number first', () => {
    expect(clampToRange(64.4, 0, 127)).toBe(64)
    expect(clampToRange(64.6, 0, 127)).toBe(65)
    expect(clampToRange(127.4, 0, 127)).toBe(127)
    expect(clampToRange(-0.4, 0, 127)).toBe(0)
  })

  it('only applies the limit that is given', () => {
    expect(clampToRange(500, 0)).toBe(500)
    expect(clampToRange(-5, 0)).toBe(0)
    expect(clampToRange(500, undefined, 127)).toBe(127)
    expect(clampToRange(-5, undefined, 127)).toBe(-5)
    expect(clampToRange(7.6)).toBe(8)
  })
})

describe('isInRange', () => {
  it('is true only for a whole number inside the range', () => {
    expect(isInRange(0, 0, 127)).toBe(true)
    expect(isInRange(127, 0, 127)).toBe(true)
    expect(isInRange(128, 0, 127)).toBe(false)
    expect(isInRange(-1, 0, 127)).toBe(false)
    expect(isInRange(64.5, 0, 127)).toBe(false)
  })
})
