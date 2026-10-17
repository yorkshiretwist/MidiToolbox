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
#ifndef SetParameterValueAction_h
#define SetParameterValueAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly set the VALUE of whichever system
// parameter is currently selected (see SetParameterAction/
// CycleParameterAction for how that's chosen) - e.g. if Bank is the
// current parameter, this jumps straight to the bank named
// "parameterValue" below, rather than stepping through them one at a time
// the way CycleParameterValueAction does.
//
// "parameterValue" is always a plain string in JSON, but what it means
// depends entirely on which parameter is current when this action runs:
//   - Patch:       the name of a Patch within the current Bank
//   - Bank:        the name of a Bank within the current Mode
//   - Mode:        the name of a Mode
//   - MidiChannel: a number, 1-16, as text (e.g. "4")
//   - Config:      the filename of a config file (e.g. "Live Rig.json")
// See applyCurrentParameterValue() in Navigation.h for exactly how each of
// these is resolved.
class SetParameterValueAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the value to set the current parameter to
    String parameterValue;

    // constructs a new SetParameterValueAction object from the given JsonVariant object for the given button
    SetParameterValueAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the parameterValue
      if (!params["parameterValue"].isNull()) {
        parameterValue = params["parameterValue"].as<String>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetParameterValue;
    }

    // method to run when the button for this action is pressed - a direct
    // Set of the current parameter's value: applies immediately, with a
    // clean teardown for a hierarchy change (see applyCurrentParameterValue()
    // in Navigation.h)
    void OnPress() override {
      navSetParameterValue(parameterValue);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
