// Tests for lib/notes.ts: note-name / MIDI-number conversion.
import { describe, expect, it } from 'vitest'
import {
  PIANO_MAX_MIDI,
  PIANO_MIN_MIDI,
  isBlackKey,
  midiSetToNoteNames,
  midiToNoteName,
  noteNameToMidi,
  noteNamesToMidiSet,
} from './notes'

describe('noteNameToMidi', () => {
  it('uses the C4 = 60 convention', () => {
    expect(noteNameToMidi('C4')).toBe(60)
    expect(noteNameToMidi('A0')).toBe(PIANO_MIN_MIDI)
    expect(noteNameToMidi('C8')).toBe(PIANO_MAX_MIDI)
  })

  it('accepts sharps and flats, including enharmonic equivalents', () => {
    expect(noteNameToMidi('C#4')).toBe(61)
    expect(noteNameToMidi('Db4')).toBe(61)
    expect(noteNameToMidi('Cb4')).toBe(59)
    expect(noteNameToMidi('B#3')).toBe(60)
  })

  it('is case-insensitive and tolerates surrounding space', () => {
    expect(noteNameToMidi(' g3 ')).toBe(55)
  })

  it('returns null for anything that is not a note name', () => {
    expect(noteNameToMidi('')).toBeNull()
    expect(noteNameToMidi('H4')).toBeNull()
    expect(noteNameToMidi('C')).toBeNull()
    expect(noteNameToMidi('60')).toBeNull()
  })

  it('returns null when the pitch falls outside MIDI 0-127', () => {
    expect(noteNameToMidi('C-2')).toBeNull()
    expect(noteNameToMidi('G#9')).toBeNull()
  })
})

describe('midiToNoteName', () => {
  it('round-trips through noteNameToMidi with sharp spelling', () => {
    for (let midi = PIANO_MIN_MIDI; midi <= PIANO_MAX_MIDI; midi++) {
      expect(noteNameToMidi(midiToNoteName(midi))).toBe(midi)
    }
  })

  it('spells black keys with a sharp', () => {
    expect(midiToNoteName(61)).toBe('C#4')
    expect(midiToNoteName(21)).toBe('A0')
  })
})

describe('isBlackKey', () => {
  it('identifies the five black keys in each octave', () => {
    expect(isBlackKey(noteNameToMidi('C4')!)).toBe(false)
    expect(isBlackKey(noteNameToMidi('C#4')!)).toBe(true)
    expect(isBlackKey(noteNameToMidi('E4')!)).toBe(false)
    expect(isBlackKey(noteNameToMidi('F4')!)).toBe(false)
    expect(isBlackKey(noteNameToMidi('A#4')!)).toBe(true)
  })
})

describe('noteNamesToMidiSet', () => {
  it('parses, de-duplicates and drops unparseable entries', () => {
    const set = noteNamesToMidiSet(['C4', 'Db4', 'C#4', 'nonsense'])
    expect([...set].sort((a, b) => a - b)).toEqual([60, 61])
  })
})

describe('midiSetToNoteNames', () => {
  it('sorts low-to-high and formats with sharps', () => {
    expect(midiSetToNoteNames(new Set([64, 60, 67]))).toEqual(['C4', 'E4', 'G4'])
  })
})
