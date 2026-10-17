// "const char version[] = ..." - see ConfigStore.h's note on this
// "C string" array style. This is a plain, unused-by-any-logic version
// marker for humans reading the code/serial output - nothing in this
// project currently reads it back out.
const char version[] = "0.1.0";

//----------------------------------------------------------------------------//
//                                                                            //
//    MIDI Toolbox                                                            //
//                                                                            //
//    A highly-configurable MIDI controller operated by footswitches,         //
//    designed for use by a musician already using their hands (e.g. a        //
//    guitarist).                                                             //
//                                                                            //
//    Copyright (C) 2024 Chris Taylor                                         //
//    chris@stillbreathing.co.uk                                              //
//    https://stillbreathing.co.uk                                            //
//                                                                            //
//----------------------------------------------------------------------------//
//                                                                            //
//    This program is free software: you can redistribute it and/or modify    //
//    it under the terms of the GNU General Public License as published by    //
//    the Free Software Foundation, either version 3 of the License, or       //
//    (at your option) any later version.                                     //
//                                                                            //
//    This program is distributed in the hope that it will be useful,         //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of          //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
//    GNU General Public License for more details.                            //
//                                                                            //
//    You should have received a copy of the GNU General Public License       //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.  //
//                                                                            //
//----------------------------------------------------------------------------//

// This is the "main" file of the project - Arduino calls it a ".ino"
// sketch file. It's a bit like a Main.cs/Program.cs in C# or a
// __main__.py/app.py in Python, but with an important Arduino-specific
// twist covered further down (search for "prototype" in the comments
// below): the Arduino build tool automatically scans THIS file (and only
// this file, not any of the .h files it includes) for function
// definitions and quietly generates forward declarations for them,
// inserted near the top of the file before compiling. This project works
// around several of the rough edges that causes by keeping most real
// logic in separate .h files instead (Runtime.h, ConfigStore.h, Screen.h)
// and only having simple, "safe" functions defined directly here.
//
// There's also no explicit "Main()"/entry point to find here, unlike C#
// or Python - Arduino expects exactly two specific functions to exist:
// setup() (runs once at power-on) and loop() (runs over and over, forever,
// after that) - both defined near the bottom of this file. The Arduino
// core itself supplies the real entry point behind the scenes and calls
// these two for you.

// ============================================================================
// Debugging

// Enum to represent log levels - see Enums.h for a fuller explanation of
// C++ enums
enum LogLevel { NONE, DEBUG, INFO, WARNING, ERROR, CRITICAL }; 

// the level of logs to print to the Serial Monitor
const LogLevel printLogLevel = LogLevel::DEBUG;

// function to print a message to the Serial Monitor
//
// Declared here with its default argument values, and defined below without
// them: Arduino's automatic function-prototype generator would otherwise
// insert its own declaration (repeating the defaults) immediately before
// this function - and C++ doesn't allow a default argument to be specified
// twice for the same function. Declaring it ourselves, before its first use,
// stops Arduino generating (and duplicating) one of its own.
//
// "String msg, LogLevel logLevel = LogLevel::INFO, bool newLine = true" -
// the "= LogLevel::INFO" and "= true" are default argument values, exactly
// like C#'s "LogLevel logLevel = LogLevel.Info" default parameters. Python
// achieves the same thing with "logLevel=LogLevel.INFO" in a def. This
// means callers can leave logLevel/newLine off entirely and get sensible
// defaults - see the many "print(someText, LogLevel::WARNING)" calls
// throughout this project, which rely on newLine defaulting to true.
void print(String msg, LogLevel logLevel = LogLevel::INFO, bool newLine = true);

// this is the actual body/implementation - note it repeats the parameter
// NAMES but not the "=" default values (those are only allowed once, on
// the declaration above)
void print(String msg, LogLevel logLevel, bool newLine) {
  // "<" compares enum values by their underlying number (see Enums.h - a
  // plain enum's members count up from 0 in the order they're listed, so
  // DEBUG < INFO < WARNING etc.) - this means "only print this message if
  // its level is at least as serious as printLogLevel", the same idea as
  // typical logging frameworks in C#/Python (e.g. .NET's LogLevel, or
  // Python's logging module) that let you set a minimum severity to show.
  if (logLevel < printLogLevel) {
    return;
  }
  // Every log line starts with "# ". The same USB serial connection carries
  // the desktop app's file-transfer protocol (see SerialProtocol.h), and the
  // app ignores any line beginning with "#", so log output can never be
  // mistaken for a reply. "static" inside a function means this one variable
  // keeps its value between calls (unlike an ordinary local, which starts
  // afresh each time) - here it remembers whether the last message finished
  // its line, so a print(..., false) followed by another print() is still
  // only prefixed once.
  static bool atLineStart = true;
  if (atLineStart) {
    Serial.print("# ");
  }
  if (newLine) {
    Serial.println(msg);
  } else {
    Serial.print(msg);
  }
  atLineStart = newLine;
}

// ============================================================================
// runtime API used by Action classes
//
// Action classes (NoteAction, ProgramChangeAction, etc.) need to send MIDI
// messages, and to change which Mode/Bank/Patch/parameter is selected. But
// they're defined before Config exists (Config depends on Mode, which
// depends on ModeBank/Button, which depends on the actions) - so instead of
// reaching into Config directly, actions call these free functions. They're
// declared here, before the action class headers are included below, and
// implemented further down in this file, next to the runtime state they use.

// include the library needed to read JSON data
#include <ArduinoJson.h>

#include "Enums.h"
#include "Note.h"
#include "ColourCode.h"

// Every line below this point, down to the "====" separator, is a forward
// declaration - the function's "signature" (name, parameters, return type)
// without a body - a promise to the compiler that a real definition exists
// somewhere later in this file (see Runtime.h/ConfigStore.h/Screen.h,
// where these are actually implemented). This is a plain, ordinary C++
// forward declaration, the same idea as a C# interface method signature
// or an abstract method - Python doesn't really have an equivalent, since
// Python never needs to know a function exists before it's called, only
// before the CALL actually happens at runtime.
//
// resolves an action's effective MIDI channel by checking, in order: the
// action's own channel, the current Patch's midiChannel, the current
// ModeBank's midiChannel, then the globally-selected MIDI channel
// (currentMidiChannel) - see the fuller explanation in Runtime.h.
// actionMidiChannel of -1 means "this action didn't configure its own
// channel".
int resolveMidiChannel(int actionMidiChannel);

// send a single MIDI note on message for the given Note, on the given MIDI channel
void sendMidiNoteOn(Note &note, int midiChannel);
// send a single MIDI note off message for the given Note, on the given MIDI channel
void sendMidiNoteOff(Note &note, int midiChannel);
// send a MIDI Program Change message
void sendMidiProgramChange(int programNumber, int midiChannel);
// send a MIDI Control Change message
void sendMidiControlChange(int controllerNumber, int controlValue, int midiChannel);

// move which system parameter the selector points at (Patch/Bank/Mode/
// MidiChannel/Config) - the UI cursor SetParameterAction / CycleParameterAction
// steer. The value side lives in Navigation.h (see below).
void setCurrentParameter(Parameter parameter);
void cycleCurrentParameterSelection(CycleDirection direction);
// fire the actions for, and mark on, any Button in the current Mode/Bank/
// Patch whose defaultStatus is On - called whenever the selection changes
// and once when a config file finishes loading. Sweeps every page of a
// multi-page patch. patchLevelOnly (default false) restricts the sweep to
// ids that resolve to a Patch-level Button, skipping Bank/Mode/Config-level
// ones entirely - see the call site in applySelection() (Navigation.h) for
// why: a same-bank patch change must not re-seed a Bank-level button a
// second time, only the freshly-selected Patch's own buttons.
void applyDefaultButtonStatuses(bool patchLevelOnly = false);
// if the landing Patch has more than one page, point the parameter selector
// at the Page parameter - defined in Runtime.h, called from
// resetRuntimeSelection() in ConfigStore.h (included before Runtime.h)
void autoSelectPageParameterIfMultiPage();
// commit whatever hierarchy change the player has navigated to but not yet
// applied - defined in Navigation.h (included after Runtime.h), used here by
// ActivateSelectionAction. A no-op when nothing is pending.
void activateSelection();
// true iff a hierarchy/MIDI-channel change is pending - defined in
// Navigation.h, forward-declared here so CycleActivateParameterAction can gate its
// advance-vs-activate behaviour on it.
bool navigationDirty();

// the entry points the navigation Action classes call - defined in
// Navigation.h, forward-declared here since the action headers are included
// long before it. Cycle* move the pending cursor (deferred); Set* apply
// immediately via applySelection(). See docs/navigation.md.
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
// page navigation - always immediate (a page change tears nothing down)
void navCyclePage(CycleDirection direction);
void navSetPage(int page);

// updates the screen to reflect the current runtime state - defined in
// Screen.h, forward-declared here since Runtime.h and ConfigStore.h (both
// included before Screen.h) call it whenever relevant state changes
void display();
// shows a simple "Starting up..." message - defined in Screen.h,
// forward-declared here since displayWelcome() (below) is defined before
// Screen.h is included
void showStartupMessage();

// ============================================================================

// include the debounce library
#include <Bounce.h>

// include standard libraries
#include <vector>
#include <string>
#include <stdexcept>

// include the pins setup - this also pulls in DeviceProfile.h (which holds
// `device`, the running description of THIS unit: its button count/pins,
// navigation ids, shift-register count and screen grid) and, through it,
// MidiToolboxDevice.h, whose macros are used again further down for the
// screen and shift-register sections below
#include "Pins.h"

// include the class definitions
#include "IAction.h"                  // an interface for all actions that may occur on a button
#include "NoteAction.h"               // represents an action to play one or more MIDI notes
#include "ProgramChangeAction.h"      // represents an action to send a MIDI Program Change message
#include "ControlChangeAction.h"      // represents an action to send a MIDI Control Change message
#include "SetParameterAction.h"       // represents an action to directly set which parameter is current
#include "CycleParameterAction.h"     // represents an action to cycle which parameter is current, up or down
#include "SetParameterValueAction.h"  // represents an action to directly set the current parameter's value
#include "CycleParameterValueAction.h" // represents an action to cycle the current parameter's value up or down
#include "SetPatchAction.h"           // represents an action to directly select a Patch
#include "CyclePatchAction.h"         // represents an action to cycle the Patch up or down, directly
#include "SetBankAction.h"            // represents an action to directly select a Bank
#include "CycleBankAction.h"          // represents an action to cycle the Bank up or down, directly
#include "SetModeAction.h"            // represents an action to directly select a Mode
#include "CycleModeAction.h"          // represents an action to cycle the Mode up or down, directly
#include "SetConfigAction.h"          // represents an action to directly select a config file
#include "CycleConfigAction.h"        // represents an action to cycle the current config file up or down
#include "SetPageAction.h"            // represents an action to directly select a page within the current Patch
#include "CyclePageAction.h"          // represents an action to cycle the page within the current Patch up or down
#include "Button.h"                   // represents a button
#include "Patch.h"                    // represents a patch
#include "ModeBank.h"                 // represents a bank, the class name "Bank" seemed to cause problems
#include "Mode.h"                     // represents a mode for the unit, a mode is the highest level of organisation
#include "Config.h"                   // represents the entire configuration stored in the JSON config file

// The hierarchy of configuration classes is:
// Config -> Mode[] -> ModeBank[] -> Patch[] -> Button[] -> IButtonAction[]

// ============================================================================
// SD card

// "#define CS_SD BUILTIN_SDCARD" - a preprocessor macro (see Screen.h for
// what these are) giving a clearer, project-specific name to a constant
// the Teensy core itself defines (BUILTIN_SDCARD is the special pin value
// meaning "use the Teensy's built-in SD card slot", not a real GPIO pin
// number). This kind of "rename a library constant to something more
// readable in my own code" pattern is common in both directions - C#/
// Python code does the equivalent with a local alias/constant instead.
//
// set the chipSet for the SD card - MidiToolboxDevice.h's SD_BUILTIN picks
// between the Teensy's own SD socket (BUILTIN_SDCARD, a special sentinel
// value the Teensy core defines, not a real pin number) and SD_PIN_CS, the
// chip-select pin for an external SD module on a board with no built-in
// slot. v1 only ever ships SD_BUILTIN 1 - the Designer app only offers
// Teensy models that have one - so the #else branch is here for a
// hand-edited MidiToolboxDevice.h rather than anything the app generates.
#if SD_BUILTIN
#define CS_SD BUILTIN_SDCARD
#else
#define CS_SD SD_PIN_CS
#endif

#include <SD.h>
#include <SPI.h>

// reads and checks /device.json, the unit's wiring file, into `device` - see
// DeviceStore.h and setup() below
#include "DeviceStore.h"

// ============================================================================
// Screen
//
// The screen driver and LVGL setup/widgets live in Screen.h - see the
// comment at the top of that file for why (same reason as Runtime.h and
// ConfigStore.h). Its pins (SCREEN_PIN_DC / SCREEN_PIN_CS / ...) come
// straight from MidiToolboxDevice.h now, so there's nothing to define here
// any more - this section used to give Screen.h its DC/CS pins under the
// names TFT_DC/TFT_CS.

// ============================================================================
// Shift register
//
// A "shift register" is a small hardware chip that lets a handful of
// microcontroller pins control many more LEDs than there are pins
// available, by sending the LED states out one bit at a time ("shifting"
// them in). The ShiftPWM library (a third-party library, not written in
// this project) handles that hardware protocol; this section just
// configures it.

// "unsigned char" is another fixed-width integer type (see ColourCode.h's
// note on uint16_t) - 8 bits, holding 0-255, which happens to be exactly
// the useful range for a brightness/PWM value. "unsigned char" and
// "uint8_t" mean the same thing in practice; you'll see both spellings
// used across different libraries in this project.
//
// LED settings. numRegisters/numRGBleds used to be written out by hand to
// match the 13-button reference unit; they now come from `device` (see DeviceProfile.h)
// so a differently-sized unit gets the right shift-register count (and the
// right number of LEDs in the startup rainbow, displayWelcome() above)
// without this file needing to change.
unsigned char maxBrightness = 255;
unsigned char pwmFrequency = 50;
int numRegisters = device.shiftRegisters;
int numRGBleds = device.buttonCount;

// If your LED's turn on if the pin is low, set this to true, otherwise set it to false.
const bool ShiftPWM_invertOutputs = true; 

// You can enable the option below to shift the PWM phase of each shift register by 8 compared to the previous.
// This will slightly increase the interrupt load, but will prevent all PWM signals from becoming high at the same time.
// This will be a bit easier on your power supply, because the current peaks are distributed.
const bool ShiftPWM_balanceLoad = false;

// shift register pins - from MidiToolboxDevice.h; the chain is bit-banged
// (SHIFTPWM_NOSPI below), so these can be any three free pins, not
// necessarily a hardware SPI bus.
const int ShiftPWM_latchPin = LED_PIN_LATCH;
#define SHIFTPWM_NOSPI
const int ShiftPWM_dataPin = LED_PIN_DATA;
const int ShiftPWM_clockPin = LED_PIN_CLOCK;

// this #include has to come AFTER the ShiftPWM_... pin constants above, so
// the library can see them while it's being defined - C++ processes a file
// strictly top-to-bottom, so #include order like this genuinely matters,
// unlike C#'s "using" or Python's "import", where import order essentially
// never affects behaviour like this
//
// include ShiftPWM.h after setting the pins!
#include <ShiftPWM.h>

// ============================================================================
// Runtime variables
//
// Plain global variables - visible to every function in every file in this
// project from this point onwards (since headers included after this point
// can see them). Comparable to public static fields on a single "global"
// class in C#, or plain module-level variables in Python - C++ just allows
// them to exist directly, with no enclosing class or module required.

// the populated config
Config config;

// the deserialised JSON file
JsonDocument jsonDoc;

// an error deserialising the JSoN file (if any)
DeserializationError jsonError;

// which Mode/Bank/Patch is currently selected
int currentModeIndex = 0;
int currentBankIndex = -1;   // -1 = no Bank selected (this Mode has none, or one hasn't been chosen yet)
int currentPatchIndex = -1;  // -1 = no Patch selected

// which page of the current Patch is on screen (1-based). Pages let one
// Patch define more buttons than the 13-button hardware has - see Button.page
// and docs/navigation.md. Reset to 1 on every Patch / Bank
// / Mode / Config change and on config load.
int currentPage = 1;

// the MIDI channel used by actions that don't configure their own
int currentMidiChannel = 1;  // 1-16

// which system parameter (Patch/Bank/MIDIChannel) the CycleParameter
// buttons currently affect - an index into whatever getCurrentParameters()
// currently returns
int currentParameterIndex = 0;

// the name of the config file currently loaded (see ConfigStore.h), e.g.
// "Live Rig.json" - empty until the first config file is loaded in setup()
String currentConfigFilename = "";

// ============================================================================
// screen functions

// runs the welcome LED rainbow. The startup screen itself (see Screen.h) is
// already up by this point - shown from setup() right after initScreen(),
// as early in boot as possible - and stays up underneath this for the rest
// of setup(), until display() (see Screen.h) replaces it once the config
// file has loaded.
void displayWelcome() {
  rgbLedRainbow(numRGBleds, 5, 3, numRegisters*8/3);

  delay(1500);

  ShiftPWM.SetAll(0);
}

// make the RGB LEDs flash in a pretty rainbow pattern
// used when the unit is starting up
void rgbLedRainbow(int numRGBLeds, int delayVal, int numCycles, int rainbowWidth){
  // Displays a rainbow spread over a few LED's (numRGBLeds), which shifts in hue. 
  // The rainbow can be wider then the real number of LED's.

  ShiftPWM.SetAll(0);
  // three nested "for" loops - C++'s for loop, like C#'s, is written as
  // "for (start; condition; step)"; Python has no direct equivalent
  // syntax, instead normally reaching for "for cycle in range(numCycles):"
  // to get the same counting behaviour. Nesting loops like this (a loop
  // inside a loop inside a loop) works identically in all three languages.
  for(int cycle=0;cycle<numCycles;cycle++){ // loop through the hue shift a number of times (numCycles)
    for(int colourshift=0;colourshift<360;colourshift++){ // Shift over full color range (like the hue slider in photoshop)
      for(int led=0;led<numRGBLeds;led++){ // loop over all LED's
        int hue = ((led)*360/(rainbowWidth-1)+colourshift)%360; // Set hue from 0 to 360 from first to last led and shift the hue
        ShiftPWM.SetHSV(led, hue, 255, 255); // write the HSV values, with saturation and value at maximum
      }
      delay(delayVal); // this delay value determines the speed of hue shift
    } 
  }  
}

// ============================================================================
// Config file storage - discovering, loading, and remembering the choice of
// config file across multiple files on the SD card
//
// This lives in ConfigStore.h rather than here - see the comment at the top
// of that file (and of Runtime.h) for why.

#include "ConfigStore.h"

// ============================================================================
// Runtime state helpers, MIDI-sending, and button dispatch
//
// These live in Runtime.h rather than here - see the comment at the top of
// that file for why.

#include "Runtime.h"

// ============================================================================
// Navigation - the "pending cursor" behind deferred Config/Mode/Bank/Patch/
// MIDI-channel changes and the flashing screen previews (see
// docs/navigation.md). Included after Runtime.h and ConfigStore.h (it
// uses their state and functions) and before Screen.h (which renders its
// previews). See the comment at the top of Navigation.h.

#include "Navigation.h"

// ============================================================================
// Screen - LVGL setup, widgets, and display() live in Screen.h - see the
// comment at the top of that file for why.

#include "Screen.h"

// ============================================================================
// setup
//
// setup() is one of the two functions every Arduino sketch must define
// (see the note at the top of this file) - the Arduino core calls this
// exactly once, automatically, when the board powers on or resets, before
// loop() starts running. There's no explicit call to setup() anywhere in
// this project's own code - much like C#'s Main() or Python's
// "if __name__ == '__main__':" block, something outside this file (here,
// Arduino's own startup code) is what actually invokes it.

// ============================================================================
// Serial protocol
//
// The desktop app manages the SD card (device.json, config files), restarts
// the unit and puts it into the Teensy bootloader over the same USB serial
// connection the log output uses - see SerialProtocol.h and
// docs/serial-protocol.md. The protocol itself lives in that header (so the
// unit tests can run it); this section supplies what it needs from the real
// hardware.

#include "SerialProtocol.h"

// whether the SD card started up, and where the unit's wiring came from
// ("file" = /device.json, "built-in" = MidiToolboxDevice.h, "none" = safe
// mode) - both reported by INFO
bool sdIsReady = false;
const char *deviceSource = "built-in";
bool inSafeMode = false;

// the real USB serial connection, as a ProtocolPort
class UsbSerialPort : public ProtocolPort {
  public:
    int available() override { return Serial.available(); }
    int read() override { return Serial.read(); }
    size_t readAvailable(uint8_t *buffer, size_t max) override {
      size_t count = Serial.available();
      if (count > max) {
        count = max;
      }
      return count == 0 ? 0 : Serial.readBytes((char *)buffer, count);
    }
    size_t write(const uint8_t *buffer, size_t length) override { return Serial.write(buffer, length); }
    void flush() override { Serial.flush(); }
    unsigned long now() override { return millis(); }
};

bool protocolSdReady() { return sdIsReady; }

bool protocolNotesActive() { return anyButtonActive(); }

// turns everything off and makes sure the resulting MIDI messages have left
// the unit before it restarts
void protocolReleaseAll() {
  releaseAllActiveButtons();
  usbMIDI.send_now();
}

// the reply to INFO: one line of JSON describing this unit
String protocolInfoJson() {
  JsonDocument doc;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["protocol"] = PROTOCOL_VERSION;
  doc["device"] = device.name;
  doc["deviceSource"] = deviceSource;
  doc["buttons"] = device.buttonCount;
  doc["sd"] = sdIsReady;
  doc["safeMode"] = inSafeMode;
  doc["config"] = currentConfigFilename;
  doc["buttonsActive"] = anyButtonActive();
  String out;
  serializeJson(doc, out);
  return out;
}

// both of these give the reply a moment to leave before the unit goes away
void protocolReboot() {
  delay(100);
  SCB_AIRCR = 0x05FA0004; // ARM "system reset request"
  while (true) {}
}

void protocolBootloader() {
  delay(100);
  _reboot_Teensyduino_();
}

UsbSerialPort usbSerialPort;
SerialProtocol serialProtocol(usbSerialPort, ProtocolHooks{
  protocolSdReady, protocolNotesActive, protocolReleaseAll, protocolInfoJson, protocolReboot, protocolBootloader
});

// Safe mode: used when the unit's wiring file (device.json) is missing or
// wrong. It deliberately touches NO pins - no screen, no LEDs, no buttons -
// because a wrong pin number could drive a pin that is really wired to a
// footswitch. It says why over USB serial, and keeps answering the serial
// protocol (which only uses the USB connection and the SD card), so the
// desktop app can upload a correct device.json and restart the unit. Never
// returns.
void enterSafeMode(String reason) {
  inSafeMode = true;
  deviceSource = "none";
  unsigned long lastReport = 0;
  bool reported = false;
  while (true) {
    serialProtocol.poll();
    if (!reported || millis() - lastReport >= 2000) {
      print("SAFE MODE - " + reason, LogLevel::CRITICAL);
      print("No pins are being driven. Put a valid /device.json on the SD card (or upload one with the desktop app) and restart.", LogLevel::CRITICAL);
      lastReport = millis();
      reported = true;
    }
    delay(1);
  }
}

void setup() {
  // Serial.begin(...) starts up the USB serial connection used for
  // debugging output (everything print() sends via Serial.println/print)
  // - 115200 is the "baud rate" (signalling speed), a detail that has to
  // match whatever's reading the output on the other end (e.g. the
  // Arduino IDE's Serial Monitor).
  Serial.begin(115200);

  print("setup() called", LogLevel::INFO);

  // The SD card comes first now, before the screen or any pin is touched,
  // because it holds device.json - the description of how this unit is
  // wired - and nothing may be driven until that has been checked.
  sdIsReady = SD.begin(CS_SD);
  if (sdIsReady) {
    String problem;
    DeviceLoadResult loaded = loadDeviceProfile(device, problem);
    if (loaded == DeviceLoadResult::Loaded) {
      print("Unit wiring read from " + String(DEVICE_FILE_PATH) + ": " + String(device.name), LogLevel::INFO);
      // numRegisters/numRGBleds were set from the compiled-in wiring when
      // the program started; bring them in line with the file
      numRegisters = device.shiftRegisters;
      numRGBleds = device.buttonCount;
      deviceSource = "file";
    } else if (loaded == DeviceLoadResult::Invalid) {
      enterSafeMode(String(DEVICE_FILE_PATH) + " cannot be used: " + problem);
    } else if (DEVICE_JSON_REQUIRED) {
      enterSafeMode(String(DEVICE_FILE_PATH) + " was not found on the SD card");
    } else {
      print(String(DEVICE_FILE_PATH) + " not found - using the wiring compiled into this firmware", LogLevel::WARNING);
    }
  } else if (DEVICE_JSON_REQUIRED) {
    enterSafeMode("the SD card failed or is missing, so the unit's wiring file cannot be read");
  }

  // initialise the screen, build the widget tree, and put up the startup
  // screen (gradient + wordmark) immediately - before the Serial wait below,
  // SD init, config loading, or anything else - so it's the first thing the
  // player sees and stays up for the rest of setup(). display() (which
  // needs the config file loaded) replaces it at the very end of setup().
  initScreen();
  showStartupMessage();

  // "while (!Serial && millis() < 3000);" - an empty-bodied while loop
  // (just a ";"), busy-waiting until either the Serial connection becomes
  // ready, OR 3 seconds have passed, whichever comes first - so the board
  // doesn't hang forever waiting for a USB connection that might never
  // arrive (e.g. running on battery power with nothing plugged in).
  // millis() is an Arduino built-in returning the number of milliseconds
  // since the board was powered on - the closest comparisons are C#'s
  // Environment.TickCount or Python's time.monotonic(), though neither
  // language needs this kind of manual busy-wait very often.
  //
  // wait for the Serial to be available (no more than 3 seconds)
  while (!Serial && millis () < 3000);
  screenSetStartupProgress(15);

  // the SD card was initialised above. Anything setup() can't recover from -
  // a missing card, or the "no config file at all" check further down -
  // swaps the progress bar for a red failure message (see
  // showStartupFailure() in Screen.h) and returns, since there's nothing
  // more setup() can safely do.
  if (!sdIsReady) {
    print("SD card failed, or not present", LogLevel::CRITICAL);
    showStartupFailure();
    return;
  }
  print("SD card initialized", LogLevel::INFO);
  screenSetStartupProgress(35);

  // setup the pins, including handling debouncing
  setupPins();
  screenSetStartupProgress(50);

  // load whichever config file was last selected (or the first one found,
  // or the legacy MidiToolbox.json, if none has been selected yet)
  loadStartupConfig();
  if (config.modes.empty()) {
    print("No usable config file found", LogLevel::CRITICAL);
    showStartupFailure();
    return;
  }
  screenSetStartupProgress(75);

  // sync the deferred-navigation pending cursor to the just-loaded selection
  // (see Navigation.h) so nothing shows as pending / flashing at boot
  resetPendingToLive();

  // set up the LEDs
  ShiftPWM.SetAmountOfRegisters(numRegisters);
  ShiftPWM.SetPinGrouping(1);
  ShiftPWM.Start(pwmFrequency,maxBrightness);
  // the shift-register driver is up now - let screenLedWrite() through (it
  // no-ops before this, so the flash timer's early syncs don't hit ShiftPWM
  // before it can accept writes - see screenLedsReady in Screen.h)
  screenLedsReady = true;
  screenSetStartupProgress(90);

  displayWelcome();
  screenSetStartupProgress(100);

  // now that the config is loaded, show the real screen
  display();
}

// ============================================================================
// main loop
//
// loop() is the other function every Arduino sketch must define - once
// setup() finishes, the Arduino core calls loop() over and over, forever,
// automatically, for as long as the board has power. This takes the place
// of an explicit "while (true) { ... }" you might write yourself in a
// long-running C#/Python program - here, the repetition is handled for
// you, you just write what should happen on each pass.

// this whole section (thresholdMin through triggerKick-adjacent commented
// code) implements a small hand-rolled state machine for detecting a
// "kick" on a piezo sensor, using a plain int "state" variable rather than
// an enum or class - state == 0/1/2 mean different things depending on
// context, tracked purely by convention/comments rather than being
// enforced by the type system the way ButtonMode (see Enums.h) is
// elsewhere in this project. This code is currently not wired up to
// anything (search below - it's never used to trigger an action), so
// treat it as inactive/in-progress rather than part of the working
// button-handling logic.
const int thresholdMin = 1008;  // minimum reading, avoid "noise"
const int aftershockMillis = 60; // time of aftershocks & vibration

int state=0; // 0=idle, 1=looking for peak, 2=ignore aftershocks
int peak;    // remember the highest reading

// the device.navSelectId button needs to know how long it's been held, to tell a
// tap (advance selector / activate a pending change) from a long-press
// (revert) - see resolveButton10() / NAV_LONG_PRESS_MS in Navigation.h.
uint32_t navSelectPressedMs = 0;
// "elapsedMillis" is an Arduino/Teensy-specific type (not standard C++)
// that behaves like an integer holding "milliseconds since I was last set
// to 0", automatically counting up in the background - assigning it 0 (as
// seen further down, "msec = 0;") resets the timer, and just reading it
// (e.g. "msec >= 10") tells you how long it's been since that reset. This
// specific "make a variable that quietly counts elapsed time on its own"
// behaviour isn't something C#/Python have a built-in type for; you'd
// normally track a start timestamp yourself and subtract.
elapsedMillis msec; // timer to end states 1 and 2

void loop() {

  // let LVGL process timers, animations, and redraws
  lv_timer_handler();

  // answer any command from the desktop app (see SerialProtocol.h)
  serialProtocol.poll();

  // Update all the buttons.  There should not be any long
  // delays in loop(), so this runs repetitively at a rate
  // faster than the buttons could be pressed and released.
  for (int id = 0; id < device.buttonCount; id++) {
    footswitches[id].update();
  }

  //externalTrigger1.update();

  // analogRead() is an Arduino built-in reading a voltage on an analogue
  // input pin as a number (0-1023 on most Arduino-family boards) - used
  // here to read the piezo sensor. There's no C#/Python equivalent since
  // neither language talks to raw hardware pins like this directly.
  int piezo1 = analogRead(externalTrigger1Pin);
  
  if (state == 0) {
    // IDLE state: if any reading is above a threshold, begin peak
    if (piezo1 > thresholdMin) {
      //Serial.println("begin state 1");
      state = 1;
      peak = piezo1;
      msec = 0;
    }
  } else if (state == 1) {

    // Peak Tracking state: for 10 ms, capture largest reading
    if (piezo1 > peak) {
      peak = piezo1;
    }
    if (msec >= 10) {
      
      //triggerKick(peak);
      //Serial.println("begin state 2");
      state = 2;
      msec = 0;
    }
  } else {
    // Ignore Aftershock state: wait for things to be quiet again
    if (piezo1 > thresholdMin) {
      msec = 0; // keep resetting timer if above threshold
    } else if (msec > 30) {
      //Serial.println("begin state 0");
      state = 0; // go back to idle after 30 ms below threshold
    }
  }

  // The loop below checks every physical button's Bounce object (see
  // Pins.h) for a state change since the last loop() pass. "fallingEdge()"
  // means the button was just pressed down; "risingEdge()" means it was
  // just released - "falling"/"rising" refer to the electrical voltage on
  // the pin dropping or rising, a detail that comes from the INPUT_PULLUP
  // wiring (see Pins.h).
  //
  // An ordinary button routes through performanceButtonPress() (Navigation.h):
  // it logs the press, commits any pending hierarchy navigation first
  // (unless the button carries its own immediate SetPatch/SetMode/... - see
  // docs/navigation.md), then runs the button's own
  // actions via triggerButton().
  //
  // device.navSelectId/device.navDownId/device.navUpId (from DeviceProfile.h) are the
  // three ids that instead run the built-in parameter-navigation role
  // (docs/navigation.md) by DEFAULT - like any other
  // button, a config's Patch/Bank/Mode/Config buttons array may bind its own
  // action(s) to one of these ids instead (see buttonHasExplicitOverride() /
  // builtInNavButtons() in Runtime.h), in which case the footswitch runs
  // that override exactly like an ordinary button. The on-screen parameter
  // row (Screen.h's screenParamDown/Name/UpCb) is a separate code path and
  // always keeps the built-in behaviour below regardless of any override, so
  // there is always a way to navigate even if a config repurposes the
  // physical footswitches - this duplication is deliberate, do not
  // "simplify" it away.
  //   device.navSelectId: while at its built-in default, tap = advance the
  //           parameter selector, or - while a change is pending ("Set?") -
  //           activate it; long-press = revert. Once overridden, behaves
  //           like an ordinary button - hold duration is ignored, since
  //           long-press-revert has no meaning once this id isn't the
  //           parameter selector any more (it's still always reachable from
  //           the screen).
  //   device.navDownId / device.navUpId: step the selected parameter's value down /
  //           up, unless overridden.

  // any press or release this pass means the on-screen button grid's on/off
  // appearance needs a redraw - performanceButtonPress() / releaseButton()
  // don't call display() themselves. Just flag it here; the actual redraw
  // happens once at the very end of loop(), AFTER every Bounce.update()/edge
  // check, so a slow display() can't sit between the pin scan and the next
  // one and widen the debounce sampling gap.
  bool mainButtonEdge = false;

  for (int id = 0; id < device.buttonCount; id++) {
    bool pressed = footswitches[id].fallingEdge();
    bool released = footswitches[id].risingEdge();
    if (!pressed && !released) {
      continue;
    }
    mainButtonEdge = true;

    if (id == device.navSelectId) {
      if (pressed) {
        if (buttonHasExplicitOverride(device.navSelectId)) {
          performanceButtonPress(id, "physical"); // logs its own "pressed"
        } else {
          logButton(id, "pressed", "physical");
        }
        navSelectPressedMs = millis();
      }
      if (released) {
        logButton(id, "released", "physical");
        if (buttonHasExplicitOverride(device.navSelectId)) {
          performanceButtonRelease(id);
        } else {
          navButton10Completed((uint32_t)(millis() - navSelectPressedMs));
        }
      }
    } else if (id == device.navDownId) {
      if (pressed) {
        if (buttonHasExplicitOverride(device.navDownId)) {
          performanceButtonPress(id, "physical"); // logs its own "pressed"
        } else {
          logButton(id, "pressed", "physical");
          navStepValue(CycleDirection::CycleDown);
        }
      }
      if (released) {
        logButton(id, "released", "physical");
        if (buttonHasExplicitOverride(device.navDownId)) {
          performanceButtonRelease(id);
        }
      }
    } else if (id == device.navUpId) {
      if (pressed) {
        if (buttonHasExplicitOverride(device.navUpId)) {
          performanceButtonPress(id, "physical"); // logs its own "pressed"
        } else {
          logButton(id, "pressed", "physical");
          navStepValue(CycleDirection::CycleUp);
        }
      }
      if (released) {
        logButton(id, "released", "physical");
        if (buttonHasExplicitOverride(device.navUpId)) {
          performanceButtonRelease(id);
        }
      }
    } else {
      if (pressed) {
        performanceButtonPress(id, "physical");
      }
      if (released) {
        logButton(id, "released", "physical");
        performanceButtonRelease(id);
      }
    }
  }

  // usbMIDI.read() checks whether an incoming MIDI message has arrived
  // over USB (e.g. from a connected computer) and returns true if so -
  // this project doesn't currently do anything with incoming messages, so
  // this loop just keeps calling read() (which also advances past the
  // message internally) until there's nothing left, discarding
  // everything, so incoming MIDI data doesn't pile up unread.
  while (usbMIDI.read()) {
    // ignore incoming messages
  }

  // one coalesced redraw per loop(), after all the Bounce sampling is done -
  // keeps the on-screen button grid in step with the footswitches without
  // ever running display() mid-scan. The built-in navigation actions
  // (navStepValue(), activateSelection(), ...) still call display() of their
  // own accord when they run; this only covers ordinary button presses.
  if (mainButtonEdge) {
    display();
  }
}
