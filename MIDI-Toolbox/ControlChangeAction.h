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
#ifndef ControlChangeAction_h
#define ControlChangeAction_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

#include "Enums.h"
#include "IAction.h"

// see NoteAction.h for a full explanation of the inheritance/override
// pattern used by every action class in this project
//
// Represents an action to send a MIDI ControlChange message
class ControlChangeAction : public IAction {
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // The MIDI controller number
    int controllerNumber = 0;

    // The MIDI control value to send when the button is pressed
    int controlValue = 127;

    // The MIDI control value to send when a Momentary button is released
    int releaseValue = 0;

    // The MIDI channel this message is to be sent on. -1 means "use
    // whichever MIDI channel is currently selected" - see
    // resolveMidiChannel() in the main .ino file.
    int midiChannel = -1;

    // constructs a new ControlChangeAction object from the given JsonVariant object for the given button
    ControlChangeAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the controllerNumber
      if (!params["controllerNumber"].isNull()) {
        controllerNumber = params["controllerNumber"].as<int>();
      }

      // set the controlValue
      if (!params["controlValue"].isNull()) {
        controlValue = params["controlValue"].as<int>();
      }

      // set the releaseValue
      if (!params["releaseValue"].isNull()) {
        releaseValue = params["releaseValue"].as<int>();
      }

      // set the MIDI channel
      if (!params["midiChannel"].isNull()) {
        midiChannel = params["midiChannel"].as<int>();
      }
    }

    // the type of this action
    ActionType getType() const override {
      return ActionType::ControlChange;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      sendMidiControlChange(controllerNumber, controlValue, resolveMidiChannel(midiChannel));
    };

    // method to run when the button for this action is released
    //
    // The button dispatch logic (Runtime.h) only calls this when the button
    // should actually turn off: on physical release for Momentary buttons,
    // or when a Latched/Exclusive button is deliberately turned off (pressed
    // again, or displaced by a sibling in its Exclusive set). So this always
    // sends the release value - there's no need to check buttonMode here.
    void OnRelease() override {
      sendMidiControlChange(controllerNumber, releaseValue, resolveMidiChannel(midiChannel));
    };
};

// matches #ifndef/#define above
#endif