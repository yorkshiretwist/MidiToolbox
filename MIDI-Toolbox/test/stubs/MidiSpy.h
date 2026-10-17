#pragma once
#include <vector>
#include <cstdint>

// Records every MIDI message the code under test sends, instead of a
// hardware-connected MIDI device receiving it. This is a "test spy": a
// stand-in that captures what happened so tests can assert on it
// afterwards (as opposed to a "mock", which would also assert *inside*
// itself). Tests call clearMidiLog() in their setup, then inspect
// getMidiLog() after triggering some behaviour.
struct MidiCall {
  enum Kind { NoteOn, NoteOff, ProgramChange, ControlChange } kind;
  int a; // note number / program number / controller number
  int b; // velocity / (unused for ProgramChange) / control value
  int channel;

  bool operator==(const MidiCall &other) const {
    return kind == other.kind && a == other.a && b == other.b && channel == other.channel;
  }
};

inline std::vector<MidiCall> &midiLog() {
  static std::vector<MidiCall> log;
  return log;
}

inline void clearMidiLog() {
  midiLog().clear();
}

class usbMIDIClass {
  public:
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
      midiLog().push_back({MidiCall::NoteOn, note, velocity, channel});
    }
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
      midiLog().push_back({MidiCall::NoteOff, note, velocity, channel});
    }
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
      midiLog().push_back({MidiCall::ControlChange, control, value, channel});
    }
    void sendProgramChange(uint8_t program, uint8_t channel) {
      midiLog().push_back({MidiCall::ProgramChange, program, 0, channel});
    }
    bool read() { return false; }
};
