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
#ifndef ActivateSelectionAction_h
#define ActivateSelectionAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to activate (commit) whatever Config / Mode / Bank /
// Patch / MIDI-channel change the player has navigated to but not yet
// applied. A no-op when nothing is pending. This is the ONLY moment the
// outgoing selection's held notes stop - see applySelection() in
// Navigation.h and the rationale in docs/navigation.md.
//
// activateSelection() is defined in Navigation.h (included later in the
// build); it's forward-declared in MIDI-Toolbox.ino and the test harness,
// before this header, exactly as cycleMode() etc. are for the other action
// classes.
class ActivateSelectionAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // constructs a new ActivateSelectionAction object from the given
    // JsonVariant object for the given button. This action takes no
    // "actionParameters".
    ActivateSelectionAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;
      (void)json;
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::ActivateSelection;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      activateSelection();
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release
    };
};

// matches #ifndef/#define above
#endif
