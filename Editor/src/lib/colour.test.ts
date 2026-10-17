// Tests for lib/colour.ts: luminance, contrast ratio and the contrasting text colour.
import { describe, expect, it } from 'vitest'
import {
  contrastRatio,
  contrastingTextColour,
  relativeLuminance,
} from './colour'

describe('relativeLuminance', () => {
  it('is 0 for black and 1 for white', () => {
    expect(relativeLuminance([0, 0, 0])).toBe(0)
    expect(relativeLuminance([255, 255, 255])).toBeCloseTo(1, 10)
  })

  it('weights green well above blue', () => {
    expect(relativeLuminance([0, 255, 0])).toBeGreaterThan(
      relativeLuminance([0, 0, 255]),
    )
  })
})

describe('contrastRatio', () => {
  it('is 21 for black against white and symmetric', () => {
    expect(contrastRatio([0, 0, 0], [255, 255, 255])).toBeCloseTo(21, 5)
    expect(contrastRatio([255, 255, 255], [0, 0, 0])).toBeCloseTo(21, 5)
  })

  it('is 1 for a colour against itself', () => {
    expect(contrastRatio([18, 52, 86], [18, 52, 86])).toBe(1)
  })
})

describe('contrastingTextColour', () => {
  it('matches the hardware examples: yellow fill → black, dark-grey fill → white', () => {
    expect(contrastingTextColour([255, 255, 0])).toBe('#000000')
    expect(contrastingTextColour([0x33, 0x33, 0x33])).toBe('#ffffff')
  })

  it('picks white on pure black and black on pure white', () => {
    expect(contrastingTextColour([0, 0, 0])).toBe('#ffffff')
    expect(contrastingTextColour([255, 255, 255])).toBe('#000000')
  })

  it('follows the WCAG maths, not intuition, on saturated primaries', () => {
    // Black actually out-contrasts white on pure red (5.25 vs 4.0).
    expect(contrastingTextColour([255, 0, 0])).toBe('#000000')
    expect(contrastingTextColour([0, 0, 255])).toBe('#ffffff')
    expect(contrastingTextColour([0, 128, 0])).toBe('#ffffff')
  })

  it('crosses over around mid-grey', () => {
    expect(contrastingTextColour([100, 100, 100])).toBe('#ffffff')
    expect(contrastingTextColour([150, 150, 150])).toBe('#000000')
  })
})
