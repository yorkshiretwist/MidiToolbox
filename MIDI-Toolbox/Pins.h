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

// Unlike every other .h file in this project, this one has no include
// guard (#ifndef/#define/#endif - see the top of Note.h for what those do)
// and doesn't wrap anything in a class. That's fine here only because this
// file is #included exactly once, from the main .ino, and never from any
// other header - if two different files both included this one, every
// variable below would end up being defined twice, and the program
// wouldn't compile. This is a good example of C++ headers being a fairly
// blunt "paste this text in here" tool rather than a real module system
// like C#'s "using" or Python's "import" - it's on the programmer to avoid
// including the same non-guarded file more than once.

// pulls in `device` - the running description of THIS unit, including
// device.buttonCount and device.buttonPins[] (which Teensy pin each physical
// footswitch is wired to). See DeviceProfile.h for the full picture; this
// file only needs those two. DeviceProfile.h in turn includes
// MidiToolboxDevice.h, so the compile-time macros are still available to
// the files that haven't moved to `device` yet (the screen's layout).
#include "DeviceProfile.h"

// "const int" - a whole number that can't be changed after it's set, same
// idea as C#'s "const int" or, loosely, a Python module-level variable
// that convention says shouldn't be reassigned (Python has no true
// language-enforced constants).
//
// how many milliseconds a button's electrical signal is allowed to
// "bounce" (flicker rapidly as a mechanical switch settles) before its
// state is trusted - used below when constructing each Bounce object.
//
// Bounce samples the pin once per loop() pass, so this interval is only as
// reliable as loop() is fast. loop() now also drives LVGL (lv_timer_handler()
// / display()), whose redraws can stretch a pass well past the old 30 ms and
// let a still-settling footswitch through as a double press. 60 ms comfortably
// outlasts the mechanical bounce of the panel-mount momentary switches on
// their (long, unshielded) wiring, at a press latency that's imperceptible
// underfoot. If misfires persist, the next steps are Bounce2's "stable for
// interval" debouncer or a 100nF pin-to-GND cap - see the button-handling
// notes in the project history.
const int bounceTime = 60;

/*
Screen pins:
VCC         VIN         Power: 3.6 to 5.5 volts
GND         GND
CS          10          Alternate Pins: 9, 15, 20, 21
RESET       +3.3V
D/C	        9     	    Alternate Pins: 10, 15, 20, 21
SDI (MOSI)  11 (DOUT)
SCK	        13 (SCK)
LED	        VIN	        Use 100 ohm resistor
SDO (MISO)	12 (DIN)
T_CLK	      13 (SCK)
T_CS	      8	          Alternate: any digital pin
T_DIN	      11 (DOUT)
T_DO	      12 (DIN)
T_IRQ	      2         	Optional: can use any digital pin

2, 8, 9, 10, 11, 12, 13
*/

// external trigger pins - the piezo "kick" detector loop() reads via
// analogRead() (see the .ino). Not part of the footswitch/screen/LED wiring
// MidiToolboxDevice.h describes, and not currently wired to anything that
// fires an action - see the .ino's own comment on this.
int externalTrigger1Pin = 24;
int externalTrigger2Pin = 25;

// One Bounce object per footswitch, in physical button-id order - so
// footswitches[id] is button id's debounced state, for every id from 0 to
// device.buttonCount - 1. This used to be a fixed 13 separate named globals
// (button0, button1, ... buttonA, buttonB, buttonC), each a plain
// stack-allocated object - simple and clear for a fixed, always-13 unit,
// but that only works when the count is a number the source code itself
// hard-codes. device.buttonCount now comes from DeviceProfile.h, so the
// number of footswitches is a per-unit fact, not a compile-time given
// baked into a named-variable list. "Bounce" has no default constructor -
// see the setupPins() comment below - so a plain fixed-size array of them
// can't be declared without already knowing each one's pin; std::vector
// sidesteps that by never default-constructing an element at all, letting
// setupPins() build each one in place, in a loop, however many there are.
// The vector variable itself still just lives here as an ordinary global,
// exactly like the Button/Patch/ModeBank vectors elsewhere in this project
// (see Button.h) - only the Bounce objects it holds live on the heap now,
// not the vector itself.
std::vector<Bounce> footswitches;

// a free function (see Note.h) that configures every button pin's
// electrical mode and builds the footswitches vector - called once from
// setup() in the main .ino
void setupPins() {
  // reserving the final size up front means emplace_back() never needs to
  // grow (and so never needs to move already-constructed Bounce objects
  // around) partway through the loop - it just constructs each one exactly
  // once, directly in its final slot, passing (pin, bounceTime) straight to
  // Bounce's own constructor. This is the vector equivalent of the old
  // "Bounce button0 = Bounce(button0pin, bounceTime);" lines, just run
  // device.buttonCount times instead of being written out by hand once per button.
  footswitches.reserve(device.buttonCount);

  // pinMode/INPUT_PULLUP are Arduino built-ins, not standard C++ - they
  // configure a physical pin to read a digital signal, with the
  // microcontroller's internal pull-up resistor enabled (so the pin reads
  // HIGH when the button isn't pressed, and LOW when it is, without
  // needing an external resistor on the circuit board). There's no
  // C#/Python equivalent - this is hardware-level configuration specific
  // to embedded programming.
  for (int id = 0; id < device.buttonCount; id++) {
    pinMode(device.buttonPins[id], INPUT_PULLUP);
    footswitches.emplace_back(device.buttonPins[id], bounceTime);
  }

  pinMode(externalTrigger1Pin, INPUT_PULLUP);
  pinMode(externalTrigger2Pin, INPUT_PULLUP);
}
