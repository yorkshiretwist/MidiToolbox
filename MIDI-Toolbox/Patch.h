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

// include guard - see the top of Note.h for a full explanation
#ifndef Patch_h
#define Patch_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

// pulls in the Button class used by this file
#include "Button.h"
#include "Enums.h"

// represents a Patch
class Patch {
  public: 

    // the name of the Patch, shown on the screen
    String name;

    // the system parameters cyclable by SetParameter/CycleParameter while
    // this Patch is selected - if empty, the parent ModeBank's own
    // parameters are used instead (see getCurrentParameters() in
    // Runtime.h); this is the most specific level in the Patch > ModeBank >
    // Mode > Config precedence chain
    std::vector<Parameter> parameters;

    // "std::vector<Button>" is C++'s standard growable list/array type -
    // directly comparable to C#'s List<Button> or a Python list of Button
    // objects. Unlike a Python list, a C++ vector can only ever hold one
    // type (Button here) - similar to C#'s generic List<T>.
    //
    // buttons which apply to this Patche
    std::vector<Button> buttons;

    // The MIDI channel actions in this Patch should use when they don't
    // configure their own channel. -1 (the default) means "not set" - see
    // resolveMidiChannel() in Runtime.h for the full precedence order:
    // an action's own channel wins first, then this, then the parent
    // ModeBank's own midiChannel (if set), then whichever channel is
    // globally selected.
    int midiChannel = -1;

    // A group of 5 special lines controlling how Patch objects can be
    // copied/moved around. This needs a bit of background: C++ doesn't
    // have automatic garbage collection like C#/Python - when an object is
    // destroyed, anything it "owns" needs to be cleaned up deterministically
    // there and then. Button.h uses a smart pointer (std::unique_ptr) to
    // manage its list of actions, and a unique_ptr can only ever be owned
    // by ONE place at a time - trying to copy one is a compile error by
    // design (it exists specifically to prevent two places accidentally
    // thinking they both own, and both later try to delete, the same
    // object). Because Patch contains a vector of Buttons, and each Button
    // contains unique_ptrs, Patch inherits that same restriction. The 5
    // lines below spell out exactly what's and isn't allowed:
    //
    // constructs a new Patch object
    // Patch holds a vector<Button>, and Button is move-only (see Button.h),
    // so Patch is move-only too - the copy operations are deleted rather
    // than defaulted.
    //
    // the default constructor - "= default" asks the compiler to generate
    // the normal, do-nothing version for us (see Note.h for more on this)
    Patch() = default;
    // the copy constructor - what would run for code like
    // "Patch p2 = p1;" (making an independent duplicate of an existing
    // Patch). "= delete" makes this a compile error if anyone tries it -
    // there's no C#/Python equivalent to explicitly forbidding a copy like
    // this; the closest comparison is Python raising an exception from
    // __copy__, but C++ catches the mistake at compile time instead of
    // runtime.
    Patch(const Patch&) = delete;
    // the copy assignment operator - what would run for
    // "existingPatch = anotherPatch;" (overwriting one Patch's contents by
    // copying another's). Also forbidden, same reasoning as above.
    Patch& operator = (const Patch&) = delete;
    // the move constructor - what runs for code that transfers ownership
    // rather than copying, e.g. "buttons.push_back(Patch(json))" further
    // down this file: the temporary Patch object is "moved into" the
    // vector rather than copied, which is allowed even though copying
    // isn't. There's nothing quite like C++'s explicit move semantics in
    // C# or Python - both languages simply pass references to existing
    // objects around instead of needing this distinction.
    Patch(Patch&&) = default;
    // the move assignment operator - the "moving" equivalent of the copy
    // assignment operator above
    Patch& operator = (Patch&&) = default;

    // constructs a new Patch object from the given JsonVariant object
    Patch(JsonVariant json, JsonVariant presets = JsonVariant()) {
      print("", LogLevel::INFO);

      // set the name
      print("   Getting Patch.name... ", LogLevel::INFO, false);
      name = json["name"].as<String>();
      print(name, LogLevel::INFO);

      // set the MIDI channel, if this Patch configures one
      if (!json["midiChannel"].isNull()) {
        midiChannel = json["midiChannel"].as<int>();
      }

      // get the parameters applicable for this Patch - if absent, the
      // parent ModeBank's own parameters are used instead (see
      // getCurrentParameters() in Runtime.h)
      if (!json["parameters"].isNull()) {
        print("   Getting Patch.parameters...", LogLevel::DEBUG);
        JsonArray parametersArray = json["parameters"].as<JsonArray>();
        for (JsonVariant parameterItem : parametersArray) {
          parameters.push_back(getParameterEnum(parameterItem));
        }
      }

      // get the buttons for this Patch
      //
      // ".isNull()" checks whether this key was present in the JSON at
      // all - similar to checking `if "buttons" in someDict` in Python, or
      // `if (json.TryGetValue("buttons", out var buttons))` in C#. The "!"
      // in front means "not", so this reads as "if buttons is NOT null".
      if (!json["buttons"].isNull()) {
        print("   Getting Patch.buttons...", LogLevel::DEBUG);
        JsonArray buttonsArray = json["buttons"].as<JsonArray>();
        // a "range-based for loop" - directly equivalent to Python's
        // "for buttonItem in buttonsArray:" or C#'s
        // "foreach (var buttonItem in buttonsArray)". It walks through
        // every item in buttonsArray one at a time.
        for (JsonVariant buttonItem : buttonsArray) {
          // ".push_back(...)" adds an item to the end of a vector - the
          // same operation as C#'s List<T>.Add(...) or Python's
          // list.append(...). "Button(buttonItem)" constructs a brand new
          // Button object right here (calling Button's own JSON-reading
          // constructor - see Button.h) and that temporary object gets
          // moved into the vector by push_back.
          buttons.push_back(Button(buttonItem, presets));
        }
        print("   Patch.buttons.size(): " + (String)buttons.size(), LogLevel::INFO);
      }
    }
};

// matches #ifndef/#define above
#endif
