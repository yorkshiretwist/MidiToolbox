// Colour helpers for the unit's face colour. Channels are sRGB, 0-255.

/** "#rrggbb" for an [r, g, b] triple, e.g. for an `<input type="color">` value. */
export function rgbToHex(rgb: readonly number[]): string {
  return '#' + rgb.slice(0, 3).map((n) => Math.max(0, Math.min(255, n)).toString(16).padStart(2, '0')).join('')
}

/** The inverse of rgbToHex: parses a "#rrggbb" colour-input value back to [r, g, b]. */
export function hexToRgb(hex: string): number[] {
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16))
}

/** True when white grid lines would struggle to show: a light unit face. */
export function isLight([r, g, b]: readonly number[]): boolean {
  return 0.2126 * r! + 0.7152 * g! + 0.0722 * b! > 140
}
