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
//    Describes ONE physical unit: how many footswitches it has and which     //
//    Teensy pin each is wired to, which three of them run the built-in       //
//    navigation role, which screen it uses and how its pins are wired, and   //
//    the RGB LED shift-register chain. Every other file in this project      //
//    reads these macros rather than hard-coding pins or a button count, so   //
//    building a differently-shaped unit is a matter of dropping in a         //
//    different copy of this one file - nothing else needs to change.         //
//                                                                            //
//    This copy matches the original 13-button MIDI Toolbox hardware -        //
//    button.h has a diagram of its layout. To build a different unit,        //
//    design it in the Designer app (../Designer/) and replace this file      //
//    with the MidiToolboxDevice.h it generates for your design; see          //
//    Designer/BUILD.md (downloaded alongside it) for the full process.       //
//    Hand-editing this file works too, following the shape below.            //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef MidiToolboxDevice_h
#define MidiToolboxDevice_h

#include <stdint.h>

#define DEVICE_NAME              "Live Rig Unit"
#define MICROCONTROLLER          "Teensy 4.1"

// Buttons: BUTTON_PINS[id] is the Teensy pin physical button `id` is wired
// to - the same id a performance config's Button.id refers to. NAV_SELECT_ID/
// NAV_DOWN_ID/NAV_UP_ID are the three ids that run the built-in navigation
// role (see docs/navigation.md) instead of an ordinary button; every
// other id is a plain performance button. The count is fixed at exactly
// three navigation buttons - only which ids they are is configurable.
#define BUTTON_COUNT             13
#define NAV_SELECT_ID            10
#define NAV_DOWN_ID              11
#define NAV_UP_ID                12
static const int8_t BUTTON_PINS[BUTTON_COUNT] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 15, 21, 22, 23 };

// Screen. Exactly one SCREEN_DRIVER_* macro is defined, below - Screen.h
// compiles against whichever one it finds and #errors at compile time if
// it doesn't recognise it (today that means anything other than
// ILI9341_T4 - see Screen.h's own top-of-file comment for why the others
// aren't implemented yet). SCREEN_LX/SCREEN_LY are already the panel's real
// pixel size for whichever orientation is selected; SCREEN_ORIENTATION_*
// only tells initScreen() which way to rotate the driver to match.
#define SCREEN_DRIVER_ILI9341_T4 1
#define SCREEN_LX                320
#define SCREEN_LY                240
#define SCREEN_ORIENTATION_LANDSCAPE 1
#define SCREEN_HAS_TOUCH         1
#define SCREEN_TOUCH_XPT2046     1
#define SCREEN_PIN_SCK           13
#define SCREEN_PIN_MOSI          11
#define SCREEN_PIN_MISO          12
#define SCREEN_PIN_CS            10
#define SCREEN_PIN_DC            9
#define SCREEN_PIN_RESET         6
#define SCREEN_PIN_T_CS          8
#define SCREEN_PIN_T_IRQ         2

// The on-screen button grid: one cell per performance button, arranged to
// match the unit's physical face. Columns are half a cell wide: every cell
// covers two half-columns, so a row offset by half a button spacing is a row
// shifted one half-column. SCREEN_GRID_BUTTON_IDS[row][halfCol] holds the id
// of the button whose cell starts in that half-column, or 255 where no cell
// starts. Row 0 is the top row of the grid.
#define SCREEN_GRID_HALF_COLS    10
#define SCREEN_GRID_ROWS         2
static const uint8_t SCREEN_GRID_BUTTON_IDS[SCREEN_GRID_ROWS][SCREEN_GRID_HALF_COLS] = {
  { 5, 255, 6, 255, 7, 255, 8, 255, 9, 255 },
  { 0, 255, 1, 255, 2, 255, 3, 255, 4, 255 },
};

// RGB LEDs: one behind every footswitch (LED index == button id), driven
// through a chain of 74HC595 shift registers over these three bit-banged
// pins (see the "Shift register" section of the .ino) - any three free pins
// work here, the chain doesn't need a hardware SPI bus.
#define LED_PIN_LATCH            28
#define LED_PIN_DATA             29
#define LED_PIN_CLOCK            30
#define SHIFT_REGISTERS          5

// Config storage. SD_BUILTIN selects the Teensy 4.1's own SD socket
// (BUILTIN_SDCARD); if it were 0, SD_PIN_CS would give the chip-select pin
// for an external SD module instead - v1 only ever ships SD_BUILTIN 1, since
// the Designer only offers Teensy models with a built-in slot.
#define SD_BUILTIN               1

#endif
