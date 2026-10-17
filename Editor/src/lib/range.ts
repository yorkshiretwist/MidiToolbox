// Keeping a typed number inside the range a field allows.

/**
 * `value` as a whole number no lower than `min` and no higher than `max` (either may be left out).
 * Used for the 7-bit MIDI boxes (controller number, control values, program number, all 0-127),
 * where anything outside the range is not a valid MIDI data byte.
 */
export function clampToRange(value: number, min?: number, max?: number): number {
  let result = Math.round(value)
  if (min !== undefined) result = Math.max(min, result)
  if (max !== undefined) result = Math.min(max, result)
  return result
}

/** Whether `value` is already a whole number inside the range. */
export function isInRange(value: number, min?: number, max?: number): boolean {
  return clampToRange(value, min, max) === value
}
