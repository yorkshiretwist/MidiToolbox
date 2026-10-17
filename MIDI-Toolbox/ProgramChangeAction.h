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
#ifndef ProgramChangeAction_h
#define ProgramChangeAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// implements the IAction interface - see NoteAction.h for a full
// explanation of inheritance ("class X : public IAction"), "private:" vs
// "public:", and "override". This class follows exactly the same shape as
// NoteAction, just simpler (a program change is a single fire-and-forget
// MIDI message, not a list of notes with timing/spread options).
//
// Represents an action to send a MIDI ProgramChange message
class ProgramChangeAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // The MIDI program number
    int programNumber = 0;

    // The MIDI channel this message is to be sent on. -1 means "use
    // whichever MIDI channel is currently selected" - see
    // resolveMidiChannel() in the main .ino file.
    int midiChannel = -1;

    // constructs a new ProgramChangeAction object from the given JsonVariant object for the given button
    ProgramChangeAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the programNumber
      if (!params["programNumber"].isNull()) {
        programNumber = params["programNumber"].as<int>();
      }

      // set the MIDI channel
      if (!params["midiChannel"].isNull()) {
        midiChannel = params["midiChannel"].as<int>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::ProgramChange;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      sendMidiProgramChange(programNumber, resolveMidiChannel(midiChannel));
    };

    // method to run when the button for this action is released
    void OnRelease() override {
      // program changes are one-shot triggers - there's nothing to undo on release
    };
};

// matches #ifndef/#define above
#endif
