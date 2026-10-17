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
#ifndef Mode_h
#define Mode_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

// pulls in the types used by this file: the Parameter enum, the ModeBank
// class, and the Button class
#include "Enums.h"
#include "ModeBank.h"
#include "Button.h"

// represents a Mode
class Mode {
  public: 

    // the name of the Mode, shown on the screen
    String name;

    // The MIDI channel actions in this Mode (or its Banks/Patches) should
    // use when they don't configure their own channel. -1 (the default)
    // means "not set" - see resolveMidiChannel() in Runtime.h for the full
    // precedence order: an action's own channel wins first, then the
    // current Patch's midiChannel (if set), then the current ModeBank's,
    // then this, then whichever channel is globally selected (itself
    // seeded from Config.midiChannel - see ConfigStore.h).
    int midiChannel = -1;

    // Buttons which apply to all Banks and Patches in this mode
    std::vector<Button> buttons;

    // std::vector<Parameter> - a growable list of Parameter enum values
    // (see Enums.h). Comparable to C#'s List<Parameter> or a Python list
    // of enum members.
    //
    // the system parameters cyclable by button 10
    std::vector<Parameter> parameters;

    // Banks in this mode
    std::vector<ModeBank> banks;

    // See Patch.h for a full explanation of why a class that (indirectly,
    // through Button) owns std::unique_ptr objects needs to spell out its
    // copy/move behaviour like this rather than leaving it to the
    // compiler's normal defaults.
    //
    // constructs a new Mode object
    // Mode holds vectors of ModeBank and Button, both move-only (see
    // Button.h), so Mode is move-only too.
    Mode() = default;               // the "do nothing" constructor
    Mode(const Mode&) = delete;     // copying a Mode is forbidden
    Mode& operator = (const Mode&) = delete;  // ...as is copy-assigning one
    Mode(Mode&& rhs) = default;     // but moving one is fine
    Mode& operator = (Mode&& other) = default;  // ...as is move-assigning one

    // constructs a new Mode object from the given JsonVariant object
    Mode(JsonVariant json, JsonVariant presets = JsonVariant()) {
      print("", LogLevel::INFO);

      // set the name
      print(" Getting Mode.name... ", LogLevel::INFO, false);
      name = json["name"].as<String>();
      print(name, LogLevel::INFO);

      // set the MIDI channel, if this Mode configures one
      if (!json["midiChannel"].isNull()) {
        midiChannel = json["midiChannel"].as<int>();
      }

      // get the parameters applicable for this Mode
      if (!json["parameters"].isNull()) {
        print(" Getting Mode.parameters...", LogLevel::DEBUG);
        JsonArray parametersArray = json["parameters"].as<JsonArray>();
        // walks through each item in the JSON array (see Patch.h for more
        // on this range-based for loop syntax), converting each JSON
        // string (e.g. "Patch") into the matching Parameter enum value
        // (see getParameterEnum in Enums.h) before adding it to the list
        for (JsonVariant parameterItem : parametersArray) {
          parameters.push_back(getParameterEnum(parameterItem));
        }
      }

      // get the buttons for this mode
      if (!json["buttons"].isNull()) {
        print(" Getting Mode.buttons...", LogLevel::DEBUG);
        JsonArray buttonsArray = json["buttons"].as<JsonArray>();
        for (JsonVariant buttonItem : buttonsArray) {
          buttons.push_back(Button(buttonItem, presets));
        }
        print(" Mode.buttons.size(): " + (String)buttons.size(), LogLevel::INFO);
      }

      // get the banks for this mode
      if (!json["banks"].isNull()) {
        print(" Getting Mode.banks...", LogLevel::DEBUG);
        JsonArray banksArray = json["banks"].as<JsonArray>();
        for (JsonVariant bankItem : banksArray) {
          banks.push_back(ModeBank(bankItem, presets));
        }
      }
    }
};

// matches #ifndef/#define above
#endif
