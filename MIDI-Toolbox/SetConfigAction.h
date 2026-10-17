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
#ifndef SetConfigAction_h
#define SetConfigAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly select and load a named config file
// (see ConfigStore.h) - the choice is remembered and reloaded
// automatically on the next boot. This class was called SelectConfigAction
// in the previous version of this project; renamed to SetConfigAction to
// sit alongside the new/renamed Set*/Cycle* action pairs (see Enums.h's
// ActionType comment).
class SetConfigAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the name of the config file to load, e.g. "MyRig.json"
    String configFilename;

    // constructs a new SetConfigAction object from the given JsonVariant object for the given button
    SetConfigAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the configFilename - "configFilename" is the current schema's
      // property name; "filename" is accepted too, purely for
      // compatibility with config files written against an earlier draft
      // of this schema
      if (!params["configFilename"].isNull()) {
        configFilename = params["configFilename"].as<String>();
      } else if (!params["filename"].isNull()) {
        configFilename = params["filename"].as<String>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetConfig;
    }

    // method to run when the button for this action is pressed - a direct
    // Set: loads immediately, after a clean teardown of the outgoing
    // selection's held sound (see applySelection() in Navigation.h)
    void OnPress() override {
      navSetConfig(configFilename);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
