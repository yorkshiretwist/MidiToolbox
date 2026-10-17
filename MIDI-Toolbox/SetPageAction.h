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
#ifndef SetPageAction_h
#define SetPageAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to directly select a page (1-based) within the
// current Patch. Pages are a software expansion of the fixed 13-button
// hardware: one Patch can define several pages of buttons and the player
// flips between them live. A page change tears nothing down, so - unlike
// SetPatch/SetBank/SetMode/SetConfig, which funnel through the deferred
// pending cursor - this applies immediately, with no "Set?" confirmation.
// See navSetPage() in Navigation.h and docs/navigation.md.
class SetPageAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the 1-based page number to select, within the current Patch
    int page = 1;

    // constructs a new SetPageAction object from the given JsonVariant object for the given button
    SetPageAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the page
      if (!params["page"].isNull()) {
        page = params["page"].as<int>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::SetPage;
    }

    // method to run when the button for this action is pressed - a direct
    // Set: applies immediately (see navSetPage() in Navigation.h)
    void OnPress() override {
      navSetPage(page);
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
