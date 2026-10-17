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
#ifndef CycleActivateParameterAction_h
#define CycleActivateParameterAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents the combined "advance the selector, or activate a pending
// change" behaviour that the middle parameter footswitch (physical id 10,
// printed 11) has always had built in - see docs/navigation.md
// This is now an ordinary, nameable action ("Cycle/Activate
// Parameter") rather than bespoke firmware code, so it can be the default
// bound to id 10 (see builtInNavButtons() in Runtime.h) while still being
// an action a config could in principle bind anywhere else too.
//
// Named "Activate", not "Set", deliberately: when a change is pending this
// calls activateSelection() - the same commit function ActivateSelectionAction
// uses - not SetParameterAction, which is a completely different action
// (directly selecting which system parameter is current, by name).
//
// Deliberately has no parameters, unlike CycleParameterAction (which takes a
// "direction"): the built-in role this reproduces always advances forward
// when nothing is pending, and "activate" isn't directional.
//
// NOTE: this covers the button's TAP behaviour only. The long-press-revert
// gesture has no equivalent here - IAction has no notion of hold duration,
// so revert stays a hardware concept tied to physical id 10 while it's still
// at this built-in default (see navButton10Completed()/resolveButton10() in
// Navigation.h, and the .ino's buttonA handling). The on-screen parameter
// buttons (Screen.h) always keep the full built-in behaviour, including
// revert, regardless of what a config binds to the physical footswitch.
class CycleActivateParameterAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // constructs a new CycleActivateParameterAction object from the given JsonVariant object for the given button
    CycleActivateParameterAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;
      // no actionParameters - nothing to read
      (void)json;
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::CycleActivateParameter;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      if (navigationDirty()) {
        activateSelection();
      } else {
        cycleCurrentParameterSelection(CycleDirection::CycleUp);
      }
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // nothing to do on release - see the class comment above for why
      // long-press-revert isn't handled here
    };
};

// matches #ifndef/#define above
#endif
