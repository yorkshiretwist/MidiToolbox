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
//    Supports having multiple config files on the SD card, letting the      //
//    user switch between them at runtime (see SetConfigAction and         //
//    CycleConfigAction), and remembering the last one chosen so it's        //
//    reloaded automatically on the next boot.                               //
//                                                                            //
//    Config files live in the CONFIG_DIR directory on the SD card, one      //
//    ".json" file per config (e.g. "/configs/Live Rig.json",                //
//    "/configs/Practice.json"). Whichever one was last selected is          //
//    remembered in EEPROM (a magic byte plus the filename - see below), not //
//    on the SD card itself, so the choice survives an SD card swap and      //
//    doesn't cost an SD card write every time it's changed.                 //
//                                                                            //
//    This lives in a .h file, like Runtime.h, rather than directly in the   //
//    .ino - see the comment at the top of Runtime.h for why: functions      //
//    whose signatures use custom types (Config, std::vector<String>...)     //
//    break Arduino's automatic function-prototype generator if they're      //
//    defined in the .ino.                                                   //
//                                                                            //
//    This file assumes it's included from MIDI-Toolbox.ino, after Config.h  //
//    and after the "Runtime variables" section (config, jsonDoc,            //
//    currentModeIndex etc.) and the "screen functions" section (display())  //
//    - it uses all of those directly rather than declaring its own copies.  //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef ConfigStore_h
#define ConfigStore_h

// EEPROM.h is a Teensy/Arduino library for reading and writing the chip's
// small amount of long-term storage - the same physical memory keeps its
// contents even when the Teensy is powered off, unlike ordinary RAM. It's
// what lets this file remember the chosen config file across a reboot.
// There's no direct C#/Python equivalent - the closest comparison is
// something like writing to a tiny settings file, except EEPROM is a
// separate physical memory chip with a limited number of write cycles, so
// code that uses it (like saveLastConfigFilename() below) has to be a bit
// careful about not writing to it unnecessarily.
#include <EEPROM.h>
// gives us std::sort, used below
#include <algorithm>

// "const char CONFIG_DIR[] = ..." declares a fixed-size array of
// characters (C++'s original, more primitive way of representing text,
// which Arduino's String class is built on top of) rather than a String.
// Plain "const char[]" text like this is called a "C string" and is
// common for small, fixed, never-changing pieces of text like this one -
// it avoids the small overhead of creating a full String object for
// something that's set once and never modified. You'll see this pattern
// used for path constants throughout this file, then usually converted to
// a String (e.g. "String(CONFIG_DIR)") when it needs to be combined with
// other text.
//
// the directory on the SD card that holds config files
const char CONFIG_DIR[] = "/configs";

// the legacy single config file location, used as a last resort for SD
// cards set up before multiple config files were supported
const char LEGACY_CONFIG_FILENAME[] = "MidiToolbox.json";

// ----------------------------------------------------------------------------
// EEPROM layout for remembering which config file was last selected:
//   byte 0:              magic byte - EEPROM_MAGIC_VALUE if a filename has
//                        been saved, anything else means "nothing saved
//                        yet" (e.g. a brand new Teensy)
//   bytes 1..(N-1):      the filename, null-terminated

const int EEPROM_MAGIC_ADDRESS = 0;
// "uint8_t" is an 8-bit (1 byte) unsigned integer, holding values 0-255 -
// exactly the size of a single EEPROM cell/byte. "0xA5" is a number
// written in hexadecimal (base 16) - C++ supports this "0x..." prefix the
// same way C# does; Python's equivalent is also "0x...". It's an
// arbitrary, unlikely-to-occur-by-accident value used purely as a marker
// (a "magic number") to distinguish "this EEPROM byte was deliberately set
// by this program" from "this is just whatever random value was already
// sitting in a brand new, never-written chip".
const uint8_t EEPROM_MAGIC_VALUE = 0xA5;
const int EEPROM_FILENAME_ADDRESS = 1;
const int EEPROM_FILENAME_MAX_LENGTH = 48; // includes the null terminator

// reads the name of the config file that was last selected, or an empty
// String if none has been saved yet (e.g. first boot of a new Teensy)
String getLastConfigFilename() {
  if (EEPROM.read(EEPROM_MAGIC_ADDRESS) != EEPROM_MAGIC_VALUE) {
    return "";
  }

  // "char buffer[N + 1];" - a fixed-size array of N+1 characters, allocated
  // right here on the stack (see Pins.h's note on stack vs heap). Unlike a
  // C# array or a Python list, a plain C++ array like this has a size
  // that's fixed forever once created - you can't grow or shrink it
  // (that's what std::vector is for instead, seen throughout this
  // project). The "+1" leaves room for a null terminator (see below).
  char buffer[EEPROM_FILENAME_MAX_LENGTH + 1];
  int i = 0;
  // this "for" loop has no starting statement before the first ";" - "i"
  // was already declared and set to 0 just above, so there's nothing more
  // to do there. C++ (like C#) allows any of the three for-loop parts
  // (start; condition; step) to be left empty.
  for (; i < EEPROM_FILENAME_MAX_LENGTH; i++) {
    buffer[i] = (char)EEPROM.read(EEPROM_FILENAME_ADDRESS + i);
    // '\0' is the "null terminator" character (value 0) - old-style C
    // strings (like this "char buffer[]" array) mark where the text ends
    // by placing a '\0' right after the last real character, rather than
    // separately tracking a length the way String/std::string do. Hitting
    // one here means we've read the whole saved filename and can stop.
    if (buffer[i] == '\0') {
      break;
    }
  }
  buffer[i] = '\0';

  // "String(buffer)" builds a proper Arduino String from this raw
  // character array - comparable to C#'s "new string(charArray)" or
  // Python's simply already having a str from the start.
  return String(buffer);
}

// persists the name of the config file that should be loaded automatically
// on the next boot. Uses EEPROM.update(), which only writes a byte if its
// value is actually changing, to avoid unnecessary EEPROM wear.
void saveLastConfigFilename(String filename) {
  EEPROM.update(EEPROM_MAGIC_ADDRESS, EEPROM_MAGIC_VALUE);

  int length = (int)filename.length();
  if (length > EEPROM_FILENAME_MAX_LENGTH - 1) {
    length = EEPROM_FILENAME_MAX_LENGTH - 1;
  }

  for (int i = 0; i < length; i++) {
    // "filename[i]" indexes into the String to get a single character,
    // same as C#/Python string indexing
    EEPROM.update(EEPROM_FILENAME_ADDRESS + i, (uint8_t)filename[i]);
  }
  // null-terminate
  EEPROM.update(EEPROM_FILENAME_ADDRESS + length, 0);
}

// ----------------------------------------------------------------------------
// discovering config files

// lists the .json config files available in CONFIG_DIR, sorted
// alphabetically. Returns bare filenames (e.g. "Live Rig.json"), not full
// paths.
std::vector<String> listConfigFiles() {
  std::vector<String> files;

  // "File" is a type from the SD card library (not written in this
  // project) representing either an open file or an open directory - the
  // same type serves both roles, which is why the code below has to check
  // "isDirectory()" to know which one it has.
  File dir = SD.open(CONFIG_DIR);
  // "!dir" - the "!" (not) operator, checking whether the File failed to
  // open at all (File defines a conversion to bool for exactly this
  // purpose, similar to how you might check "if (file == null)" in C#).
  // "||" is "or" - same meaning and short-circuiting behaviour as C#/Python.
  if (!dir || !dir.isDirectory()) {
    print("Config directory not found: " + String(CONFIG_DIR), LogLevel::WARNING);
    return files;
  }

  // "while (true) { ... if (...) break; ... }" is a common C++/C#/Python
  // pattern for "loop until some condition found partway through the
  // loop body, not known up front" - all three languages support this
  // exact shape.
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }
    if (!entry.isDirectory()) {
      String name = entry.name();
      // only keep files whose name ends in ".json" - .substring() with one
      // argument returns everything from that position to the end (see
      // Note.h for more on .substring())
      if (name.length() > 5 && name.substring(name.length() - 5) == ".json") {
        files.push_back(name);
      }
    }
    entry.close();
  }
  dir.close();

  // std::sort re-orders the vector in place. The third argument is a
  // "lambda" - a small, anonymous, inline function, written as
  // "[](parameters) { body }". C# has an almost identical syntax for the
  // same idea ("(a, b) => a.CompareTo(b) < 0"); Python's closest
  // equivalent is a "lambda a, b: ..." expression, though Python's sort
  // instead usually takes a "key" function rather than a full comparison
  // function like this one. This lambda says "a should come before b if
  // a.compareTo(b) is negative" - i.e. plain alphabetical order.
  std::sort(files.begin(), files.end(), [](const String &a, const String &b) {
    return a.compareTo(b) < 0;
  });

  return files;
}

// ----------------------------------------------------------------------------
// loading

// resets Mode/Bank/Patch/parameter selection to the start - called after
// loading a config file, since whatever was previously selected almost
// certainly doesn't apply to it
void resetRuntimeSelection() {
  currentModeIndex = 0;
  currentBankIndex = -1;
  currentPatchIndex = -1;
  currentParameterIndex = 0;
  currentPage = 1;

  // seed the globally-selected MIDI channel from the newly-loaded config's
  // own default (Config.midiChannel, itself defaulting to 1 - see
  // Config.h). From this point on, currentMidiChannel is what actually
  // gets read/changed (e.g. by cycling the MidiChannel parameter) - see
  // resolveMidiChannel() in Runtime.h.
  currentMidiChannel = (config.midiChannel >= 1 && config.midiChannel <= 16) ? config.midiChannel : 1;

  if (config.modes.size() > 0 && config.modes[0].banks.size() > 0) {
    currentBankIndex = 0;
    currentPatchIndex = config.modes[0].banks[0].resolveDefaultPatchIndex();
  }

  // if the landing patch has multiple pages, drop the selector onto the Page
  // parameter so left/right immediately flip pages (see
  // autoSelectPageParameterIfMultiPage() in Runtime.h and
  // docs/navigation.md)
  autoSelectPageParameterIfMultiPage();

  // now that Mode/Bank/Patch are settled, fire any button in that initial
  // selection whose defaultStatus is On - see applyDefaultButtonStatuses()
  // in Runtime.h. This is what makes a "start on" button take effect at
  // boot, and again each time a different config file is loaded at runtime
  // (SetConfig/CycleConfig both go through loadConfigFile() -> here). For a
  // multi-page patch this seeds every page, not just page 1.
  applyDefaultButtonStatuses();
}

// loads the named config file from CONFIG_DIR, replacing the running
// Config, resetting Mode/Bank/Patch selection, and persisting the choice
// so it's the one loaded automatically on the next boot. Returns true on
// success; the previous config is left in place on failure.
bool loadConfigFile(String filename) {
  String path = String(CONFIG_DIR) + "/" + filename;

  print("Loading config file: " + path, LogLevel::INFO);

  // ".c_str()" converts a String into a plain "const char *" (a pointer to
  // the raw characters) - many lower-level/library functions like SD.open()
  // expect this older-style representation rather than a full String
  // object. Comparable to needing to call something like .ToCharArray()
  // in C# when an API insists on a more primitive type; Python code
  // essentially never needs to think about this distinction.
  File dataFile = SD.open(path.c_str());
  if (!dataFile) {
    print("Could not open config file: " + path, LogLevel::WARNING);
    return false;
  }

  // clears out anything left over in jsonDoc from a previous load, so
  // stale data can't leak into the newly-loaded config
  jsonDoc.clear();
  DeserializationError error = deserializeJson(jsonDoc, dataFile, DeserializationOption::NestingLimit(50));
  dataFile.close();

  if (error != DeserializationError::Ok) {
    print("Error parsing config file: " + path, LogLevel::WARNING);
    print(error.c_str(), LogLevel::WARNING);
    return false;
  }

  jsonError = error;
  // replaces the global "config" object entirely with a freshly-built one
  // parsed from the newly-loaded JSON
  config = Config(jsonDoc);
  currentConfigFilename = filename;

  resetRuntimeSelection();
  saveLastConfigFilename(filename);

  print("Loaded config: " + filename + " (" + (String)config.modes.size() + " modes)", LogLevel::INFO);

  return true;
}

// loads the single MidiToolbox.json in the SD card root - a last-resort fallback
// for SD cards set up before multiple config files were supported
bool loadLegacyConfigFile() {
  print("Trying legacy config file: " + String(LEGACY_CONFIG_FILENAME), LogLevel::WARNING);

  File dataFile = SD.open(LEGACY_CONFIG_FILENAME);
  if (!dataFile) {
    print("No config file could be found", LogLevel::ERROR);
    return false;
  }

  jsonDoc.clear();
  DeserializationError error = deserializeJson(jsonDoc, dataFile, DeserializationOption::NestingLimit(50));
  dataFile.close();

  if (error != DeserializationError::Ok) {
    print("Error parsing legacy config file", LogLevel::ERROR);
    print(error.c_str(), LogLevel::ERROR);
    return false;
  }

  jsonError = error;
  config = Config(jsonDoc);
  currentConfigFilename = LEGACY_CONFIG_FILENAME;

  resetRuntimeSelection();

  return true;
}

// works out which config file to load at boot: the one that was last
// selected (if it still exists), otherwise the first config file found
// (alphabetically), otherwise the legacy MidiToolbox.json as a last resort.
// Called once from setup().
void loadStartupConfig() {
  std::vector<String> files = listConfigFiles();

  String lastFilename = getLastConfigFilename();
  if (lastFilename.length() > 0) {
    bool stillExists = false;
    for (String &name : files) {
      if (name == lastFilename) {
        stillExists = true;
        break;
      }
    }
    // "&&" short-circuits here too: if stillExists is false, loadConfigFile()
    // never gets called at all
    if (stillExists && loadConfigFile(lastFilename)) {
      return;
    }
    print("Previously-selected config not found: " + lastFilename, LogLevel::WARNING);
  }

  if (files.size() > 0 && loadConfigFile(files[0])) {
    return;
  }

  loadLegacyConfigFile();
}

// SetConfigAction / CycleConfigAction now go through Navigation.h -
// navSetConfig() -> applySelection() for the immediate jump, and
// pendingCycleConfig() for the deferred alphabetical step (which reuses
// listConfigFiles() above). See docs/navigation.md.

// matches #ifndef/#define above
#endif
