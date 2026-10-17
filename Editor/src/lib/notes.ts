// MIDI note-name helpers.
//
// The firmware (MIDI-Toolbox/Note.h) uses the common convention where
// C4 = MIDI note 60, so MIDI 0 = C-1. A full-size piano keyboard has 88 keys
// running from A0 (MIDI 21) to C8 (MIDI 108).

/** Lowest key on an 88-note keyboard: A0. */
export const PIANO_MIN_MIDI = 21
/** Highest key on an 88-note keyboard: C8. */
export const PIANO_MAX_MIDI = 108

// Sharp spelling for each semitone within an octave. Names are written back
// out with sharps; flats are still accepted on the way in.
const SEMITONE_NAMES = [
  'C',
  'C#',
  'D',
  'D#',
  'E',
  'F',
  'F#',
  'G',
  'G#',
  'A',
  'A#',
  'B',
] as const

const LETTER_SEMITONE: Record<string, number> = {
  C: 0,
  D: 2,
  E: 4,
  F: 5,
  G: 7,
  A: 9,
  B: 11,
}

/**
 * Parse a note name such as "C4", "C#4" or "Db4" into a MIDI note number.
 * Returns null when the text isn't a note name or lands outside 0-127.
 */
export function noteNameToMidi(name: string): number | null {
  const match = /^\s*([A-Ga-g])([#b]?)(-?\d{1,2})\s*$/.exec(name)
  if (!match) return null
  let semitone = LETTER_SEMITONE[match[1].toUpperCase()]
  if (match[2] === '#') semitone += 1
  else if (match[2] === 'b') semitone -= 1
  const octave = Number(match[3])
  const midi = (octave + 1) * 12 + semitone
  return midi >= 0 && midi <= 127 ? midi : null
}

/** Format a MIDI note number as a sharp-spelled note name, e.g. 61 -> "C#4". */
export function midiToNoteName(midi: number): string {
  const octave = Math.floor(midi / 12) - 1
  return SEMITONE_NAMES[((midi % 12) + 12) % 12] + octave
}

/** True when the MIDI note falls on a black (sharp) key. */
export function isBlackKey(midi: number): boolean {
  return SEMITONE_NAMES[((midi % 12) + 12) % 12].length === 2
}

/**
 * Turn a list of note names into a de-duplicated set of MIDI numbers,
 * silently dropping anything that doesn't parse.
 */
export function noteNamesToMidiSet(names: readonly string[]): Set<number> {
  const set = new Set<number>()
  for (const name of names) {
    const midi = noteNameToMidi(name)
    if (midi != null) set.add(midi)
  }
  return set
}

/** Sort a set of MIDI numbers low-to-high and format each as a note name. */
export function midiSetToNoteNames(midi: Iterable<number>): string[] {
  return [...midi].sort((a, b) => a - b).map(midiToNoteName)
}
