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
#ifndef SetBankAction_h
#define SetBankAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly select a Bank, by index, within the
// current Mode. This class was called SelectBankAction in the previous
// version of this project; renamed to SetBankAction to sit alongside the
// new CycleBankAction (see Enums.h's ActionType comment for the full
// Set*/Cycle* naming pattern).
class SetBankAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the zero-based index of the Bank to select, within the current Mode
    int bankIndex = 0;

    // the Mode that Bank is in, when it is not the current Mode; -1 (the
    // default) means "the current Mode". See navSetBank().
    int modeIndex = -1;

    // constructs a new SetBankAction object from the given JsonVariant object for the given button
    SetBankAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the bankIndex
      if (!params["bankIndex"].isNull()) {
        bankIndex = params["bankIndex"].as<int>();
      }
      if (!params["modeIndex"].isNull()) {
        modeIndex = params["modeIndex"].as<int>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetBank;
    }

    // method to run when the button for this action is pressed - a direct
    // Set: applies immediately, with a clean teardown of the outgoing
    // selection (see applySelection() in Navigation.h)
    void OnPress() override {
      navSetBank(bankIndex, modeIndex);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
