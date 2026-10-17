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
//                                                                            //
//    Reads the unit's wiring from /device.json on the SD card into the       //
//    DeviceProfile (see DeviceProfile.h), and checks it thoroughly first.    //
//                                                                            //
//    device.json is written by the Designer app (schema: config/device-      //
//    schema.json). A wrong pin number is not a harmless mistake on this      //
//    hardware - a pin that is really wired to a footswitch could be driven   //
//    as an output - so the file is only used if EVERY rule below holds.      //
//    If any fails, the caller must not touch a single pin (see "safe mode"   //
//    in the .ino) and the reason is reported over USB serial.                //
//                                                                            //
//    Like ConfigStore.h this has no hardware dependency beyond the SD        //
//    library, so the host unit tests build it too. Include SD.h and          //
//    ArduinoJson.h before this file.                                         //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef DeviceStore_h
#define DeviceStore_h

// Whether a missing /device.json is an error. 1 (the default) means the unit
// refuses to run without one: it enters safe mode, where the desktop app can
// upload one over USB. 0 means a missing file falls back to the wiring
// compiled in from MidiToolboxDevice.h - for people who build the unit with
// that header and never use device.json. A file that IS present but fails
// the checks is always an error. Define this before including this file (for
// example in MidiToolboxDevice.h) to override the default.
#ifndef DEVICE_JSON_REQUIRED
#define DEVICE_JSON_REQUIRED 1
#endif

// where the file lives on the SD card, and the largest file accepted (the
// largest valid profile is a few kilobytes)
const char DEVICE_FILE_PATH[] = "/device.json";
const size_t DEVICE_FILE_MAX_BYTES = 8192;

// the Teensy 4.1 has pins 0-54; 42-47 belong to its built-in SD socket and
// can never be used for the unit's own wiring
const int DEVICE_MAX_PIN = 54;
const int DEVICE_SD_PIN_FIRST = 42;
const int DEVICE_SD_PIN_LAST = 47;

// the fewest footswitches a unit can have, and the most shift registers
// accepted - both match config/device-schema.json
const int DEVICE_MIN_BUTTONS = 6;
const int DEVICE_MAX_REGISTERS = 12;

// ----------------------------------------------------------------------------
// reading helpers

// true if `value` is a whole number between min and max inclusive, storing
// it in `result`. A missing key, a string, or a fractional number all fail.
static bool deviceReadInt(JsonVariantConst value, int minValue, int maxValue, int &result) {
  if (!value.is<int>()) {
    return false;
  }
  int number = value.as<int>();
  if (number < minValue || number > maxValue) {
    return false;
  }
  result = number;
  return true;
}

// who is using a pin, for the "used twice" message. Buttons are stored as
// their id (0 or more); the other owners are negative codes.
const int8_t PIN_OWNER_NONE = -1;
const int8_t PIN_OWNER_SCREEN = -2;
const int8_t PIN_OWNER_LED = -3;

static String devicePinOwnerName(int8_t owner) {
  if (owner == PIN_OWNER_SCREEN) {
    return "the screen";
  }
  if (owner == PIN_OWNER_LED) {
    return "the LED chain";
  }
  // button ids print one higher than they are numbered (see the Editor)
  return "button " + String((int)owner + 1);
}

// Claims `pin` for `owner`. Fails (and says why) if it is out of range,
// belongs to the SD socket, or is already claimed.
static bool deviceClaimPin(int pin, int8_t owner, int8_t *owners, String &error) {
  if (pin < 0 || pin > DEVICE_MAX_PIN) {
    error = devicePinOwnerName(owner) + " uses pin " + String(pin) + ", which does not exist";
    return false;
  }
  if (pin >= DEVICE_SD_PIN_FIRST && pin <= DEVICE_SD_PIN_LAST) {
    error = devicePinOwnerName(owner) + " uses pin " + String(pin) + ", which the built-in SD socket owns";
    return false;
  }
  if (owners[pin] != PIN_OWNER_NONE) {
    error = "pin " + String(pin) + " is used by both " + devicePinOwnerName(owners[pin])
        + " and " + devicePinOwnerName(owner);
    return false;
  }
  owners[pin] = owner;
  return true;
}

// ----------------------------------------------------------------------------
// the screen is still built into the firmware at compile time (it sizes its
// buffers and layout from the SCREEN_* macros), so for now device.json must
// describe the SAME screen as MidiToolboxDevice.h. Anything else would make
// the firmware drive the compiled-in pins while believing in different ones.
// This check goes when the screen moves to runtime values.

static bool deviceCheckScreen(JsonVariantConst screen, int8_t *owners, String &error) {
  const char *driver = screen["driver"] | "";
  if (strcmp(driver, "ILI9341_T4") != 0) {
    error = "screen driver \"" + String(driver) + "\" is not supported (only ILI9341_T4)";
    return false;
  }

#if defined(SCREEN_ORIENTATION_PORTRAIT)
  const char *builtOrientation = "portrait";
#else
  const char *builtOrientation = "landscape";
#endif
  int width = 0;
  int height = 0;
  if (!deviceReadInt(screen["width"], 1, 320, width) || !deviceReadInt(screen["height"], 1, 320, height)
      || width != SCREEN_LX || height != SCREEN_LY
      || strcmp(screen["orientation"] | "", builtOrientation) != 0) {
    error = "the screen's size or orientation differs from the one this firmware was built for ("
        + String(SCREEN_LX) + "x" + String(SCREEN_LY) + " " + builtOrientation + ")";
    return false;
  }

  // every pin the firmware drives for the screen, by the name the Designer uses
  struct ScreenPin { const char *name; int pin; bool required; };
  const ScreenPin builtPins[] = {
    { "SCK", SCREEN_PIN_SCK, true },
    { "MOSI", SCREEN_PIN_MOSI, true },
    { "MISO", SCREEN_PIN_MISO, true },
    { "CS", SCREEN_PIN_CS, true },
    { "DC", SCREEN_PIN_DC, true },
    { "RESET", SCREEN_PIN_RESET, true },
#if defined(SCREEN_PIN_T_CS)
    { "T_CS", SCREEN_PIN_T_CS, false },
#endif
#if defined(SCREEN_PIN_T_IRQ)
    { "T_IRQ", SCREEN_PIN_T_IRQ, false },
#endif
  };
  for (const ScreenPin &built : builtPins) {
    JsonVariantConst value = screen["pins"][built.name];
    if (value.isNull() && !built.required) {
      continue;
    }
    int pin = 0;
    if (!deviceReadInt(value, 0, DEVICE_MAX_PIN, pin) || pin != built.pin) {
      error = "screen pin " + String(built.name) + " differs from the one this firmware was built for ("
          + String(built.pin) + ")";
      return false;
    }
    if (!deviceClaimPin(pin, PIN_OWNER_SCREEN, owners, error)) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// the whole profile

// Checks `root` against every rule and, only if all pass, copies it into
// `out`. On failure `out` is left untouched and `error` says what is wrong.
bool parseDeviceProfile(JsonVariantConst root, DeviceProfile &out, String &error) {
  if (!root.is<JsonObjectConst>()) {
    error = "device.json is not a JSON object";
    return false;
  }

  int version = 0;
  if (!deviceReadInt(root["schemaVersion"], 1, 1000, version) || version != 1) {
    error = "schemaVersion must be 1 (this firmware cannot read other versions)";
    return false;
  }

  DeviceProfile profile;
  memset(&profile, 0, sizeof(profile));

  // name
  const char *name = root["name"] | "";
  if (strlen(name) < 1 || strlen(name) > (size_t)MAX_DEVICE_NAME) {
    error = "name must be 1 to " + String(MAX_DEVICE_NAME) + " characters";
    return false;
  }
  strncpy(profile.name, name, MAX_DEVICE_NAME);

  if (strcmp(root["microcontroller"] | "", "Teensy 4.1") != 0) {
    error = "microcontroller must be \"Teensy 4.1\"";
    return false;
  }

  // who owns each pin, so a clash anywhere is caught whichever side comes second
  int8_t owners[DEVICE_MAX_PIN + 1];
  for (int pin = 0; pin <= DEVICE_MAX_PIN; pin++) {
    owners[pin] = PIN_OWNER_NONE;
  }
  // the LED chain's pins are fixed in the firmware (the ShiftPWM library takes
  // them at compile time), so no button may use them
  const int ledPins[] = { LED_PIN_LATCH, LED_PIN_DATA, LED_PIN_CLOCK };
  for (int pin : ledPins) {
    owners[pin] = PIN_OWNER_LED;
  }

  if (!deviceCheckScreen(root["screen"], owners, error)) {
    return false;
  }

  // buttons
  JsonArrayConst pins = root["buttons"]["pins"].as<JsonArrayConst>();
  if (pins.isNull() || (int)pins.size() < DEVICE_MIN_BUTTONS || (int)pins.size() > MAX_BUTTONS) {
    error = "buttons.pins must list " + String(DEVICE_MIN_BUTTONS) + " to " + String(MAX_BUTTONS) + " pins";
    return false;
  }
  profile.buttonCount = (int)pins.size();
  for (int id = 0; id < profile.buttonCount; id++) {
    int pin = 0;
    if (!deviceReadInt(pins[id], 0, DEVICE_MAX_PIN, pin)) {
      error = "button " + String(id + 1) + " has no valid pin number";
      return false;
    }
    if (!deviceClaimPin(pin, (int8_t)id, owners, error)) {
      return false;
    }
    profile.buttonPins[id] = (int8_t)pin;
  }

  // the three navigation buttons
  JsonVariantConst navigation = root["buttons"]["navigation"];
  if (!deviceReadInt(navigation["select"], 0, profile.buttonCount - 1, profile.navSelectId)
      || !deviceReadInt(navigation["down"], 0, profile.buttonCount - 1, profile.navDownId)
      || !deviceReadInt(navigation["up"], 0, profile.buttonCount - 1, profile.navUpId)) {
    error = "each navigation button must be the id of an existing button";
    return false;
  }
  if (profile.navSelectId == profile.navDownId || profile.navSelectId == profile.navUpId
      || profile.navDownId == profile.navUpId) {
    error = "the three navigation buttons must be different buttons";
    return false;
  }

  // the on-screen grid
  JsonVariantConst grid = root["screen"]["grid"];
  if (!deviceReadInt(grid["rows"], 1, MAX_GRID_ROWS, profile.gridRows)
      || !deviceReadInt(grid["halfCols"], 1, MAX_GRID_HALF_COLS, profile.gridHalfCols)) {
    error = "the screen grid needs rows 1-" + String(MAX_GRID_ROWS) + " and halfCols 1-" + String(MAX_GRID_HALF_COLS);
    return false;
  }
  JsonArrayConst rows = grid["cells"].as<JsonArrayConst>();
  if (rows.isNull() || (int)rows.size() != profile.gridRows) {
    error = "the screen grid has the wrong number of rows";
    return false;
  }
  for (int row = 0; row < MAX_GRID_ROWS; row++) {
    for (int col = 0; col < MAX_GRID_HALF_COLS; col++) {
      profile.gridButtonIds[row][col] = GRID_NO_CELL;
    }
  }
  bool placed[MAX_BUTTONS] = {};
  for (int row = 0; row < profile.gridRows; row++) {
    JsonArrayConst cells = rows[row].as<JsonArrayConst>();
    if (cells.isNull() || (int)cells.size() != profile.gridHalfCols) {
      error = "screen grid row " + String(row + 1) + " has the wrong number of columns";
      return false;
    }
    for (int col = 0; col < profile.gridHalfCols; col++) {
      int id = 0;
      if (!deviceReadInt(cells[col], 0, GRID_NO_CELL, id)) {
        error = "screen grid row " + String(row + 1) + " has an invalid entry";
        return false;
      }
      if (id != GRID_NO_CELL) {
        if (id >= profile.buttonCount) {
          error = "the screen grid refers to button " + String(id + 1) + ", which does not exist";
          return false;
        }
        if (id == profile.navSelectId || id == profile.navDownId || id == profile.navUpId) {
          error = "the screen grid has a cell for navigation button " + String(id + 1);
          return false;
        }
        if (placed[id]) {
          error = "the screen grid has two cells for button " + String(id + 1);
          return false;
        }
        placed[id] = true;
      }
      profile.gridButtonIds[row][col] = (uint8_t)id;
    }
  }

  // the LED chain: 3 channels per RGB LED, 8 outputs per shift register
  if (!deviceReadInt(root["leds"]["registers"], 1, DEVICE_MAX_REGISTERS, profile.shiftRegisters)) {
    error = "leds.registers must be 1 to " + String(DEVICE_MAX_REGISTERS);
    return false;
  }
  if (profile.shiftRegisters * 8 < profile.buttonCount * 3) {
    error = String(profile.shiftRegisters) + " shift registers cannot drive " + String(profile.buttonCount) + " RGB LEDs";
    return false;
  }

  out = profile;
  return true;
}

// ----------------------------------------------------------------------------
// reading the file

enum class DeviceLoadResult {
  Loaded,   // device.json was read, passed every check, and is now in `out`
  NotFound, // there is no device.json on the card
  Invalid   // there is one, but it cannot be used: see `error`
};

DeviceLoadResult loadDeviceProfile(DeviceProfile &out, String &error) {
  File file = SD.open(DEVICE_FILE_PATH);
  if (!file) {
    return DeviceLoadResult::NotFound;
  }
  if (file.size() > DEVICE_FILE_MAX_BYTES) {
    file.close();
    error = "device.json is larger than " + String((int)DEVICE_FILE_MAX_BYTES) + " bytes";
    return DeviceLoadResult::Invalid;
  }

  JsonDocument doc;
  DeserializationError parseError = deserializeJson(doc, file, DeserializationOption::NestingLimit(10));
  file.close();
  if (parseError != DeserializationError::Ok) {
    error = String("device.json is not valid JSON: ") + parseError.c_str();
    return DeviceLoadResult::Invalid;
  }

  if (!parseDeviceProfile(doc.as<JsonVariantConst>(), out, error)) {
    return DeviceLoadResult::Invalid;
  }
  return DeviceLoadResult::Loaded;
}

#endif
