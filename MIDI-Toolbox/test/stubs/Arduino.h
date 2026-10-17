#pragma once
// Desktop stand-in for the Teensyduino Arduino.h core header, used only to
// compile this project's logic for unit testing on a normal machine - see
// test/README.md for the full explanation of why this exists and what it
// deliberately doesn't try to emulate (real timing, real hardware I/O).

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstdio>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 0x1
#define LOW  0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define PROGMEM
#define DMAMEM
#define DEC 10
#define HEX 16

class __FlashStringHelper;
#define F(string_literal) ((const __FlashStringHelper *)(string_literal))

#include "WString.h"
#include "Stream.h"

class Print {
  public:
    virtual size_t write(uint8_t) { return 0; }
    virtual size_t write(const uint8_t *buffer, size_t size) { (void)buffer; return size; }
    virtual ~Print() {}
};

class Printable {
  public:
    virtual size_t printTo(Print &p) const = 0;
    virtual ~Printable() {}
};

inline uint8_t pgm_read_byte(const void *addr) { return *(const uint8_t *)addr; }

// pin/timing functions are no-ops: tests drive behaviour by calling
// functions like triggerButton()/releaseButton() directly, never by
// simulating real electrical signals or real elapsed time
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return LOW; }
inline int analogRead(uint8_t) { return 0; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline unsigned long millis() { return 0; }
inline unsigned long micros() { return 0; }
inline long random(long howbig) { return howbig > 0 ? 0 : 0; }
inline long random(long howsmall, long howbig) { return howsmall < howbig ? howsmall : howbig; }

class elapsedMillis {
  public:
    elapsedMillis() : ms_(0) {}
    elapsedMillis(unsigned long v) : ms_(v) {}
    operator unsigned long() const { return ms_; }
    elapsedMillis &operator=(unsigned long v) { ms_ = v; return *this; }
    elapsedMillis &operator+=(unsigned long v) { ms_ += v; return *this; }
  private:
    unsigned long ms_;
};

// ---- Serial ----
// captures output rather than printing it, so running the test suite
// doesn't spam the console with every print() call the code under test
// makes - see test/README.md if you want to see it anyway
class HardwareSerialStub : public Stream {
  public:
    void begin(unsigned long) {}
    void print(const String &) {}
    void print(const char *) {}
    void print(int) {}
    void print(unsigned long, int = 10) {}
    void println(const String &) {}
    void println(const char *) {}
    void println(int) {}
    void println(unsigned long, int = 10) {}
    void println() {}
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t size) { return size; }
    operator bool() const { return true; }
};
extern HardwareSerialStub Serial;

// the real MIDI "device" is replaced with a spy that records every message
// sent, so tests can assert on exactly what was sent - see MidiSpy.h
#include "MidiSpy.h"
extern usbMIDIClass usbMIDI;
