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
#ifndef SendNotesAction_h
#define SendNotesAction_h

#include <string>
#include <vector>
#include <utility>
#include <ArduinoJson.h>

#include "Enums.h"
#include "Note.h"
#include "IAction.h"

// Represents an action to send MIDI note messages with fine-grained control.
//
// Unlike NoteAction (PlayNotes), which sends Note On when the button turns on
// and Note Off automatically when it turns off, this sends exactly ONE kind of
// message - Note On or Note Off, as set by "noteMessage" - each time the
// button turns on, and nothing when it turns off. To start a note and later
// stop it, a config pairs a SendNotes (NoteOn) with a SendNotes (NoteOff).
class SendNotesAction : public IAction {
  public:

    // the notes to send
    std::vector<Note> notes;

    // The MIDI channel on which to send the notes. -1 means "use whichever
    // MIDI channel is currently selected" - see resolveMidiChannel().
    int midiChannel = -1;

    // how to spread MIDI notes
    NoteSpread noteSpread = NoteSpread::NoSpread;

    // true to send Note On messages, false to send Note Off messages
    bool sendNoteOn = true;

    SendNotesAction(ButtonMode buttonModeObj, JsonVariant json) {
      JsonVariant params = json["actionParameters"];

      if (!params["noteNames"].isNull()) {
        JsonArray noteNamesArray = params["noteNames"].as<JsonArray>();
        for (JsonVariant noteNameItem : noteNamesArray) {
          notes.push_back(Note(noteNameItem));
        }
      }

      if (!params["midiChannel"].isNull()) {
        midiChannel = params["midiChannel"].as<int>();
      }

      if (!params["noteSpread"].isNull()) {
        noteSpread = getNoteSpreadEnum(params["noteSpread"]);
      }

      // "noteMessage" is "NoteOn" (the default) or "NoteOff"
      if (!params["noteMessage"].isNull()) {
        String message = params["noteMessage"].as<String>();
        sendNoteOn = !(message == "NoteOff");
      }

      if (!params["velocity"].isNull()) {
        int velocity = params["velocity"].as<int>();
        for (Note &note : notes) {
          note.velocity = velocity;
        }
      }
    }

    ActionType getType() const override {
      return ActionType::SendNotes;
    }

    // sends one note in the configured message type
    void sendOne(Note &note, int resolvedChannel) {
      note.midiChannel = resolvedChannel;
      if (sendNoteOn) {
        sendMidiNoteOn(note, resolvedChannel);
      } else {
        sendMidiNoteOff(note, resolvedChannel);
      }
    }

    void OnPress() override {
      int resolvedChannel = resolveMidiChannel(midiChannel);

      if (noteSpread == NoteSpread::SpreadUp) {
        for (Note &note : notes) {
          sendOne(note, resolvedChannel);
          delay(NoteSpreadUpDownMs);
        }

      } else if (noteSpread == NoteSpread::SpreadDown) {
        for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
          sendOne(*it, resolvedChannel);
          delay(NoteSpreadUpDownMs);
        }

      } else if (noteSpread == NoteSpread::SpreadRandom) {
        std::vector<int> order;
        for (int i = 0; i < (int)notes.size(); i++) {
          order.push_back(i);
        }
        // Fisher-Yates shuffle, as in NoteAction
        for (int i = (int)order.size() - 1; i > 0; i--) {
          int j = random(i + 1);
          std::swap(order[i], order[j]);
        }
        for (int index : order) {
          sendOne(notes[index], resolvedChannel);
          delay(NoteSpreadRandomMs);
        }

      } else {
        for (Note &note : notes) {
          sendOne(note, resolvedChannel);
        }
      }
    };

    // nothing is sent on release - that is the point of this action
    void OnRelease() override {
    };
};

#endif
