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
#ifndef SetModeAction_h
#define SetModeAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly select a Mode, by index. This is new in
// this version of the schema - the previous version had no way at all to
// switch Mode via a button press (only by physically wiring up buttons to
// specific Mode indices at build time was even possible, and even then not
// through the config file). See CycleModeAction for the direction-aware
// "step one Mode at a time" equivalent.
class SetModeAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the zero-based index of the Mode to select
    int modeIndex = 0;

    // constructs a new SetModeAction object from the given JsonVariant object for the given button
    SetModeAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the modeIndex
      if (!params["modeIndex"].isNull()) {
        modeIndex = params["modeIndex"].as<int>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetMode;
    }

    // method to run when the button for this action is pressed - a direct
    // Set: applies immediately, with a clean teardown of the outgoing
    // selection (see applySelection() in Navigation.h)
    void OnPress() override {
      navSetMode(modeIndex);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
