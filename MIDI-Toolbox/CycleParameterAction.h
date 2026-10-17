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
#ifndef CycleParameterAction_h
#define CycleParameterAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to cycle (up or down) through the list of available
// system parameters (Patch, Bank, Mode, MidiChannel, Config - see
// getCurrentParameters() in Runtime.h), changing which one is current -
// i.e. which one SetParameterValue/CycleParameterValue actions affect.
//
// NOTE: this is a change in behaviour from the previous version of this
// project. Previously, "CycleParameter" cycled the VALUE of the current
// parameter (e.g. moved to the next Patch) - that job now belongs to
// CycleParameterValueAction (in this same project), renamed to make room
// for this clearer split. This class instead does what the previous
// version's parameterless ChangeParameterAction did (move which parameter
// is current), but now bidirectionally, via the "direction" action
// parameter below, rather than only ever moving forward.
class CycleParameterAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the direction in which to cycle which parameter is current
    CycleDirection direction = CycleDirection::CycleUp;

    // constructs a new CycleParameterAction object from the given JsonVariant object for the given button
    CycleParameterAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the direction
      if (!params["direction"].isNull()) {
        direction = getCycleDirectionEnum(params["direction"]);
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::CycleParameter;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      cycleCurrentParameterSelection(direction);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
