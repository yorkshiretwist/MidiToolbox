# MIDI Toolbox - unit tests

## Running the tests

```
./run_tests.sh
```

This needs the ArduinoJson library (the same one the real firmware
depends on) to be findable. The script checks the usual Arduino libraries
locations automatically; if yours lives somewhere else:

```
ARDUINOJSON_DIR=/path/to/ArduinoJson/src ./run_tests.sh
```

GoogleTest itself is vendored in `vendor/googletest` (a trimmed copy, no
build step or download needed) - this is the one dependency it's normal
practice to vendor directly, for exactly this kind of "should just work"
reason.

Everything here builds and runs as a normal desktop program - none of it
runs on, or needs, a Teensy.

## What's covered, and what isn't

**Covered**, roughly in order of how much attention each got:

- **Button behaviour** (`tests/button_behavior_tests.inc`) - the priority
  for this suite. Momentary/Latched/Exclusive semantics, the Patch > Bank >
  Mode override rule, Exclusive sets being scoped to their own level even
  when button ids collide across levels, multiple actions firing from one
  button press, and MIDI channel resolution - including the full
  Button-action > Patch > ModeBank > global cascade.
- **JSON parsing** (`tests/config_parsing_tests.inc`) - the constructors on
  `Button`/`Patch`/`ModeBank`/`Mode`/`Config` that turn a config file into
  the in-memory object tree.
- **Config file discovery/persistence** (`tests/configstore_tests.inc`) -
  `ConfigStore.h`'s EEPROM round-tripping, directory scanning, cycling, and
  its fallback chain (last-selected -> first available -> legacy file).
- **Pure logic** (`tests/note_tests.inc`, `tests/colourcode_tests.inc`,
  `tests/enums_tests.inc`) - note-name parsing, RGB565 packing, and the
  JSON string <-> enum conversions used throughout the config format.

**Not covered, on purpose:**

- **`Screen.h`** (LVGL/`ILI9341_T4` rendering) - this is almost entirely
  hardware side-effects (drawing pixels, reading touch input) rather than
  logic worth unit-testing in isolation, and pulling in real LVGL source
  just for this test suite didn't seem worth the weight. If you want
  confidence here, an integration test on real hardware is a better fit
  than a unit test anyway.
- **`setup()`/`loop()`** in the `.ino` itself - this is Arduino's lifecycle
  glue (debounce polling, `lv_timer_handler()`, reading incoming MIDI) more
  than logic; the actual behaviour it triggers (`triggerButton()`,
  `releaseButton()`) is what's under test instead.
- **Real timing** (debounce, `elapsedMillis`, `delay()`) - all of this is
  stubbed to be instantaneous/no-op. Tests call `triggerButton()`/
  `releaseButton()` directly rather than simulating electrical signals on a
  pin over time.

## How this is structured, and why

Every function in this project's `.h` files is a plain, non-`inline` free
function - completely normal for an Arduino sketch that's only ever
compiled once, but it means two separate `.cpp` files both `#include`-ing,
say, `Runtime.h` would fail to *link* ("multiple definition of
`triggerButton`"), even though each one compiles fine on its own.

Because of that, this suite is built as a **single translation unit**:
`test_main.cpp` does the same `#include` chain and declares the same
global variables the real `MIDI-Toolbox.ino` does (mirroring its structure
closely - see the comments at the top of `test_main.cpp`), then pulls in
each topic's actual `TEST()` cases via `#include "tests/*.inc"` at the
bottom. The `.inc` files contain nothing but test bodies - no `#include`s
of their own - so this gets the readability of separate files per topic
without hitting the linker problem.

If you want true multi-file test compilation later, the underlying fix is
marking the project's header-defined functions `inline` - a reasonable
change, but a real one to the shipped firmware, so it wasn't made as a side
effect of adding tests.

## How the "hardware" is faked

`stubs/` contains minimal stand-ins for the pieces this project depends on
that don't exist on a desktop: `Arduino.h`/`WString.h`/`Stream.h` (String,
`Serial`, `millis()`, etc. - all as fast no-ops), `SD.h` (backed by the real
filesystem - see `ConfigStoreTest`, which genuinely reads/writes temp
files), `EEPROM.h` (backed by an in-memory buffer with a `.reset()` method
tests use for isolation), and `MidiSpy.h`.

`MidiSpy.h` is the one worth calling out: instead of a no-op stand-in for
`usbMIDI`, it *records* every message sent (note on/off, program change,
control change) into a log the tests can inspect. Without this, tests could
only check "did the code crash" - with it, they check "did pressing this
button send exactly this MIDI message, on exactly this channel" - which is
what actually matters for button behaviour.

## A note on trustworthiness

A test suite that never fails hasn't proven anything. Before relying on
this one, a bug was deliberately introduced into `Runtime.h`'s Exclusive-
button handling (inverting the on/off check) and the suite was re-run: the
4 tests that specifically exercise Exclusive-button behaviour failed, and
only those 4 - nothing else went red, nothing stayed green that shouldn't
have. That's the level of confidence this suite is meant to provide; it's
worth repeating that check yourself after any change to `Runtime.h` you're
not sure about.
