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
#ifndef CycleModeAction_h
#define CycleModeAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to cycle (up or down) the currently-selected Mode.
// New in this version of the schema - see SetModeAction for the "jump
// straight to this Mode" equivalent.
class CycleModeAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the direction in which to cycle the Mode
    CycleDirection direction = CycleDirection::CycleUp;

    // constructs a new CycleModeAction object from the given JsonVariant object for the given button
    CycleModeAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the direction
      if (!params["direction"].isNull()) {
        direction = getCycleDirectionEnum(params["direction"]);
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::CycleMode;
    }

    // method to run when the button for this action is pressed - moves the
    // pending navigation cursor (see Navigation.h); nothing changes until an
    // activation
    void OnPress() override {
      navCycleMode(direction);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
