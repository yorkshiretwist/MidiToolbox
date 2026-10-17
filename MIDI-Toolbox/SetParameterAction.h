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
#ifndef SetParameterAction_h
#define SetParameterAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly set which system parameter (Patch,
// Bank, Mode, MidiChannel, or Config) the SetParameterValue/
// CycleParameterValue actions currently affect, by name. This replaces the
// previous version's ChangeParameterAction, which had no parameters and
// could only ever move forward through the parameter list one step at a
// time - see CycleParameterAction (in this same project) for the
// direction-aware "move one step" equivalent, now that SetParameterAction
// covers "jump straight there".
class SetParameterAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the system parameter to make current
    Parameter parameter = Parameter::BankParameter;

    // constructs a new SetParameterAction object from the given JsonVariant object for the given button
    SetParameterAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the parameter
      if (!params["parameterName"].isNull()) {
        parameter = getParameterEnum(params["parameterName"]);
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetParameter;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      setCurrentParameter(parameter);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
