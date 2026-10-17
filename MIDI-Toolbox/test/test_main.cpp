//----------------------------------------------------------------------------//
//    MIDI Toolbox - unit test suite                                          //
//                                                                            //
//    See test/README.md for how to build and run this, and for an          //
//    explanation of the structure below.                                   //
//----------------------------------------------------------------------------//

// ============================================================================
// Harness setup
//
// This mirrors the real MIDI-Toolbox.ino's own include order and global
// variables, because the project's header files are written assuming that
// exact context (see Runtime.h's own top-of-file comment for why). This is
// deliberately ONE translation unit, not several: almost every function in
// this project is defined directly in a header, without "inline" - fine for
// a sketch that's only ever compiled once, but it means two separate .cpp
// files both #including, say, Runtime.h would fail to link ("multiple
// definition of triggerButton"). Splitting the actual TEST() bodies into
// separate tests/*.inc files (included below, after all of this setup) gets
// the organizational benefit of separate files without hitting that problem
// - see test/README.md for more on this trade-off.

#include "Arduino.h"

// pulls in `device` (device.buttonCount, device.navSelectId/navDownId/navUpId
// and friends, filled from MidiToolboxDevice.h) -
// Runtime.h (included below) now uses these directly (it loops over every
// physical button id, and builds the navigation fallback from the three nav
// ids), so the host build needs them defined too, exactly like the real
// .ino gets them via Pins.h. The stub Arduino.h above doesn't provide any
// hardware, so only the macros from this header are actually used here -
// the pin/screen/LED wiring it also describes is meaningless off real
// hardware and simply goes unused by every test.
#include "DeviceProfile.h"

// ---- Debugging (mirrors the .ino's "Debugging" section) ----
enum LogLevel { NONE, DEBUG, INFO, WARNING, ERROR, CRITICAL };
// silence all output during tests - the assertions are what matter, not a
// wall of log lines
const LogLevel printLogLevel = LogLevel::CRITICAL;
void print(String msg, LogLevel logLevel = LogLevel::INFO, bool newLine = true);
void print(String msg, LogLevel logLevel, bool newLine) {
  (void)msg; (void)logLevel; (void)newLine;
}

// ---- runtime API forward declarations (mirrors the .ino) ----
#include <ArduinoJson.h>
#include "Enums.h"
#include "Note.h"
#include "ColourCode.h"

int resolveMidiChannel(int actionMidiChannel);
void sendMidiNoteOn(Note &note, int midiChannel);
void sendMidiNoteOff(Note &note, int midiChannel);
void sendMidiProgramChange(int programNumber, int midiChannel);
void sendMidiControlChange(int controllerNumber, int controlValue, int midiChannel);
void setCurrentParameter(Parameter parameter);
void cycleCurrentParameterSelection(CycleDirection direction);
void applyDefaultButtonStatuses(bool patchLevelOnly = false);
void autoSelectPageParameterIfMultiPage();
void activateSelection();
bool navigationDirty();
void navCyclePatch(CycleDirection direction);
void navCycleBank(CycleDirection direction);
void navCycleMode(CycleDirection direction);
void navCycleConfig(CycleDirection direction);
void navCycleParameterValue(CycleDirection direction);
void navSetPatch(int patchIndex, int bankIndex = -1, int modeIndex = -1);
void navSetBank(int bankIndex, int modeIndex = -1);
void navSetMode(int modeIndex);
void navSetConfig(String filename);
void navSetParameterValue(String value);
void navCyclePage(CycleDirection direction);
void navSetPage(int page);

// display()/showStartupMessage() are normally implemented in Screen.h,
// which this test suite deliberately doesn't build against (see
// test/README.md - LVGL/screen rendering isn't in scope for these unit
// tests) - they're given empty bodies here instead, since Runtime.h and
// ConfigStore.h both call display() whenever relevant state changes.
void display();
void showStartupMessage();
void display() {}
void showStartupMessage() {}

// ---- class definitions (mirrors the .ino) ----
#include <vector>
#include <string>

#include "IAction.h"
#include "NoteAction.h"
#include "ProgramChangeAction.h"
#include "ControlChangeAction.h"
#include "SetParameterAction.h"
#include "CycleParameterAction.h"
#include "CycleActivateParameterAction.h"
#include "SetParameterValueAction.h"
#include "CycleParameterValueAction.h"
#include "SetPatchAction.h"
#include "CyclePatchAction.h"
#include "SetBankAction.h"
#include "CycleBankAction.h"
#include "SetModeAction.h"
#include "CycleModeAction.h"
#include "SetConfigAction.h"
#include "CycleConfigAction.h"
#include "SetPageAction.h"
#include "CyclePageAction.h"
#include "ActivateSelectionAction.h"
#include "Button.h"
#include "Patch.h"
#include "ModeBank.h"
#include "Mode.h"
#include "Config.h"

#include "SD.h"
#include "EEPROM.h"

// ---- Runtime variables (mirrors the .ino) ----
Config config;
JsonDocument jsonDoc;
DeserializationError jsonError;
int currentModeIndex = 0;
int currentBankIndex = -1;
int currentPatchIndex = -1;
int currentPage = 1;
int currentMidiChannel = 1;
int currentParameterIndex = 0;
String currentConfigFilename = "";

#include "Runtime.h"
#include "ConfigStore.h"
#include "Navigation.h"
#include "DeviceStore.h"
#include "SerialProtocol.h"

#include <gtest/gtest.h>

// ============================================================================
// shared test helpers

// Resets every piece of global state this project keeps, so no test can
// accidentally depend on another test having run first or in a particular
// order - independence between tests is a core unit-testing principle.
// Test fixtures below call this from SetUp(), which GoogleTest runs fresh
// before every single TEST_F case.
void resetTestState() {
  config = Config();
  currentModeIndex = 0;
  currentBankIndex = -1;
  currentPatchIndex = -1;
  currentPage = 1;
  currentMidiChannel = 1;
  currentParameterIndex = 0;
  currentConfigFilename = "";
  resetNavigationState();
  clearMidiLog();
  EEPROM.reset();
}

// parses a JSON fixture string (a small, hand-written config snippet - see
// each test file for examples) into a JsonDocument. Building small in-memory
// fixtures like this instead of reading real files off an SD card keeps
// tests fast, deterministic, and free of any hardware/filesystem dependency
// - another core unit-testing principle.
JsonDocument parseJson(const char *jsonText) {
  JsonDocument doc;
  // matches the NestingLimit used by the real loadConfigFile()/
  // loadLegacyConfigFile() in ConfigStore.h - without this, ArduinoJson's
  // much shallower default limit rejects some of the more deeply-nested
  // fixtures used below (Mode -> Bank -> Patch -> Button -> actions)
  DeserializationError err = deserializeJson(doc, jsonText, DeserializationOption::NestingLimit(50));
  EXPECT_EQ(err, DeserializationError::Ok) << "Test fixture JSON failed to parse: " << err.c_str();
  return doc;
}

// ============================================================================
// test suites (see test/README.md for what each file covers)

#include "tests/enums_tests.inc"
#include "tests/note_tests.inc"
#include "tests/colourcode_tests.inc"
#include "tests/config_parsing_tests.inc"
#include "tests/button_behavior_tests.inc"
#include "tests/configstore_tests.inc"
#include "tests/navigation_tests.inc"
#include "tests/deviceprofile_tests.inc"
#include "tests/devicestore_tests.inc"
#include "tests/serialprotocol_tests.inc"
