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
//    The running description of THIS unit: how many footswitches it has,     //
//    which Teensy pin each is wired to, which three run the navigation       //
//    role, how many shift registers drive the LEDs, and the on-screen        //
//    button grid.                                                            //
//                                                                            //
//    Until now these facts were macros in MidiToolboxDevice.h, fixed when    //
//    the sketch was compiled. They are now plain values in the variable      //
//    `device` below, so the rest of the firmware reads `device.buttonCount`  //
//    etc. instead of BUTTON_COUNT. For the moment `device` is still filled   //
//    from MidiToolboxDevice.h, so the unit behaves exactly as before; the    //
//    point of the indirection is that a later step can fill it from a        //
//    device.json file on the SD card instead (see docs/runtime-device-       //
//    plan.md).                                                               //
//                                                                            //
//    Not here yet: the screen's resolution and pins, and the LED chain's     //
//    pins. The screen sizes its buffers and layout with compile-time         //
//    macros (SCREEN_LX etc., still from MidiToolboxDevice.h) and moves to    //
//    runtime values in a later step; the LED pins are fixed in the firmware  //
//    because the ShiftPWM library cannot take them from a file.              //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef DeviceProfile_h
#define DeviceProfile_h

#include <stdint.h>
#include <string.h>

// the compile-time description of the unit, used to fill `device` below
#include "MidiToolboxDevice.h"

// Upper limits for the fixed-size arrays in DeviceProfile (and for the
// arrays elsewhere that need one slot per button). A unit may have fewer
// buttons than this, never more - they match what config/device-schema.json
// allows. 255 is reserved in the grid to mean "no cell here", so ids stay
// well below that.
const int MAX_BUTTONS = 32;
const int MAX_GRID_ROWS = 8;
const int MAX_GRID_HALF_COLS = 32;
const int MAX_DEVICE_NAME = 40;

// the id the grid uses for "no button cell starts in this half-column"
const uint8_t GRID_NO_CELL = 255;

// "struct" is C++'s plain bundle of named values with no behaviour of its
// own - roughly a C# struct or a Python dataclass. The arrays are fixed
// size (rather than std::vector) so that this can be filled from either a
// compile-time header or a file without any allocation.
struct DeviceProfile {
  char name[MAX_DEVICE_NAME + 1];

  // how many footswitches the unit has; physical button ids run
  // 0..buttonCount-1 and buttonPins[id] is the Teensy pin button `id` is
  // wired to - the same id a performance config's Button.id refers to
  int buttonCount;
  int8_t buttonPins[MAX_BUTTONS];

  // the three button ids that run the built-in navigation role (see
  // docs/navigation.md) rather than an ordinary performance button
  int navSelectId;
  int navDownId;
  int navUpId;

  // how many 74HC595 shift registers make up the LED chain
  int shiftRegisters;

  // The on-screen button grid. Columns are half a cell wide, so a row offset
  // by half a button spacing is a row shifted one half-column.
  // gridButtonIds[row][halfCol] is the id of the button whose cell starts in
  // that half-column, or GRID_NO_CELL where none does. Row 0 is the top.
  int gridRows;
  int gridHalfCols;
  uint8_t gridButtonIds[MAX_GRID_ROWS][MAX_GRID_HALF_COLS];
};

// A hand-edited MidiToolboxDevice.h that asks for more than the arrays above
// can hold would otherwise overflow them silently; refuse to compile instead.
static_assert(BUTTON_COUNT <= MAX_BUTTONS, "MidiToolboxDevice.h has more buttons than MAX_BUTTONS in DeviceProfile.h");
static_assert(SCREEN_GRID_ROWS <= MAX_GRID_ROWS, "MidiToolboxDevice.h has more grid rows than MAX_GRID_ROWS in DeviceProfile.h");
static_assert(SCREEN_GRID_HALF_COLS <= MAX_GRID_HALF_COLS, "MidiToolboxDevice.h has more grid columns than MAX_GRID_HALF_COLS in DeviceProfile.h");

// Builds the profile for the unit this sketch was compiled for, straight
// from the macros in MidiToolboxDevice.h.
DeviceProfile deviceProfileFromHeader() {
  DeviceProfile profile;
  memset(&profile, 0, sizeof(profile));
  strncpy(profile.name, DEVICE_NAME, MAX_DEVICE_NAME);

  profile.buttonCount = BUTTON_COUNT;
  for (int id = 0; id < BUTTON_COUNT; id++) {
    profile.buttonPins[id] = BUTTON_PINS[id];
  }
  profile.navSelectId = NAV_SELECT_ID;
  profile.navDownId = NAV_DOWN_ID;
  profile.navUpId = NAV_UP_ID;

  profile.shiftRegisters = SHIFT_REGISTERS;

  profile.gridRows = SCREEN_GRID_ROWS;
  profile.gridHalfCols = SCREEN_GRID_HALF_COLS;
  for (int row = 0; row < MAX_GRID_ROWS; row++) {
    for (int col = 0; col < MAX_GRID_HALF_COLS; col++) {
      profile.gridButtonIds[row][col] =
          (row < SCREEN_GRID_ROWS && col < SCREEN_GRID_HALF_COLS)
              ? SCREEN_GRID_BUTTON_IDS[row][col]
              : GRID_NO_CELL;
    }
  }
  return profile;
}

// The profile in use. This is a global variable initialised by calling the
// function above; because the whole firmware is a single translation unit
// (see the note in test/test_main.cpp), globals are initialised in the
// order they are defined, so this is ready before any later global that
// reads it.
DeviceProfile device = deviceProfileFromHeader();

#endif
