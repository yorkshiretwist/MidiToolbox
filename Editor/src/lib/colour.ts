// Colour-contrast helpers, ported from MIDI-Toolbox/ColourCode.h so the editor's
// screen preview picks the same text colour the hardware would. All inputs are
// sRGB channels in 0–255.

/** Undo the sRGB gamma curve for one 0–255 channel (WCAG step 1). */
function srgbToLinear(channel: number): number {
  const c = channel / 255
  return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4)
}

/**
 * WCAG relative luminance: 0 for black, 1 for white. Green is weighted far
 * above blue because the eye is more sensitive to it.
 */
export function relativeLuminance([r, g, b]: readonly number[]): number {
  return (
    0.2126 * srgbToLinear(r) +
    0.7152 * srgbToLinear(g) +
    0.0722 * srgbToLinear(b)
  )
}

/**
 * WCAG contrast ratio between two colours: 1 when identical, up to 21 for pure
 * black against pure white. Symmetric.
 */
export function contrastRatio(
  a: readonly number[],
  b: readonly number[],
): number {
  const la = relativeLuminance(a)
  const lb = relativeLuminance(b)
  const lighter = Math.max(la, lb)
  const darker = Math.min(la, lb)
  return (lighter + 0.05) / (darker + 0.05)
}

const BLACK = [0, 0, 0] as const
const WHITE = [255, 255, 255] as const

/**
 * Pure black or pure white — whichever a caller should draw text in so it
 * stands out most against `background`. Greater contrast wins; a dead tie
 * (vanishingly unlikely) goes to white. Mirrors contrastingTextColour() in
 * MIDI-Toolbox/ColourCode.h.
 */
export function contrastingTextColour(
  background: readonly number[],
): '#000000' | '#ffffff' {
  return contrastRatio(background, WHITE) >= contrastRatio(background, BLACK)
    ? '#ffffff'
    : '#000000'
}

/** "#rrggbb" for an [r, g, b] triple, e.g. for an `<input type="color">` value. */
export function rgbToHex(rgb: number[] | undefined, fallback = '#333333'): string {
  if (!rgb || rgb.length < 3) return fallback
  return (
    '#' +
    rgb
      .slice(0, 3)
      .map((n) => Math.max(0, Math.min(255, n)).toString(16).padStart(2, '0'))
      .join('')
  )
}

/** The inverse of rgbToHex — parses a "#rrggbb" colour-input value back to [r, g, b]. */
export function hexToRgb(hex: string): number[] {
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16))
}
