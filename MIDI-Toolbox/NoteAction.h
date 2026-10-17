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
#ifndef NoteAction_h
#define NoteAction_h

#include <string>
#include <vector>
// <utility> gives us std::swap, used below in the SpreadRandom shuffle
#include <utility>
#include <ArduinoJson.h>

#include "Enums.h"
#include "Note.h"
#include "IAction.h"

// "class NoteAction : public IAction" - the ": public IAction" part means
// NoteAction *inherits from* (or "implements", in interface terms) IAction,
// directly equivalent to C#'s "class NoteAction : IAction". Python spells
// this "class NoteAction(IAction):". Because IAction's methods are all
// "pure virtual" (see IAction.h), NoteAction is required to provide real
// bodies for getType()/OnPress()/OnRelease() below, or this won't compile.
//
// Represents an action to play one or more MIDI notes
class NoteAction : public IAction {
  // "private:" is the opposite of "public:" (see Note.h) - everything
  // below it can only be used from inside this class's own methods, not
  // from outside code. Directly equivalent to C#'s "private". Python has
  // no enforced equivalent (only the convention of a leading underscore,
  // e.g. self._buttonMode, which nothing actually stops you ignoring).
  private:
    // the button mode for which this is an action
    ButtonMode buttonMode;

  public:

    // the notes to send
    std::vector<Note> notes;

    // The MIDI channel on which to play the notes. -1 means "use whichever
    // MIDI channel is currently selected", i.e. currentMidiChannel - see
    // resolveMidiChannel() in the main .ino file.
    int midiChannel = -1;

    // how to spread MIDI notes
    NoteSpread noteSpread = NoteSpread::NoSpread;

    // constructs a new NoteAction object from the given JsonVariant object for the given button
    NoteAction(ButtonMode buttonModeObj, JsonVariant json) {
      buttonMode = buttonModeObj;

      JsonVariant params = json["actionParameters"];

      // set the notes
      if (!params["noteNames"].isNull()) {
        print("    Getting NoteAction.notes...", LogLevel::DEBUG);
        JsonArray noteNamesArray = params["noteNames"].as<JsonArray>();
        for (JsonVariant noteNameItem : noteNamesArray) {
          notes.push_back(Note(noteNameItem));
        }
        print("    NoteAction.notes.size(): " + (String)notes.size(), LogLevel::INFO);
      }

      // set the MIDI channel
      if (!params["midiChannel"].isNull()) {
        midiChannel = params["midiChannel"].as<int>();
      }

      // set the spread of MIDI notes
      if (!params["noteSpread"].isNull()) {
        noteSpread = getNoteSpreadEnum(params["noteSpread"]);
      }

      // set the velocity for every note in this set, overriding each
      // Note's own default (99, see Note.h) - "velocity" is optional, and
      // applies uniformly to the whole "notes" vector rather than being
      // configurable per-note, since the JSON "noteNames" array is just a
      // flat list of names with no per-note structure
      if (!params["velocity"].isNull()) {
        print("    Getting NoteAction.velocity...", LogLevel::DEBUG);
        int velocity = params["velocity"].as<int>();
        // "Note &note" - a reference, not a copy, so this actually
        // modifies the real Note objects living inside "notes" (see the
        // OnPress() method below for a fuller explanation of "&" here)
        for (Note &note : notes) {
          note.velocity = velocity;
        }
      }
    }

    // "override" tells the compiler "I mean for this to replace the
    // matching virtual method from the base class (IAction)" - the same
    // purpose as C#'s "override" keyword. If the signature here doesn't
    // actually match anything virtual in IAction (e.g. a typo in the
    // method name), the compiler raises an error instead of silently
    // creating an unrelated new method - a safety net C# also gives you,
    // that Python simply doesn't have (a misspelled method name in Python
    // just quietly does nothing).
    //
    // the type of this action
    ActionType getType() const override {
      return ActionType::PlayNotes;
    }

    // method to run when the button for this action is pressed
    void OnPress() override {
      int resolvedChannel = resolveMidiChannel(midiChannel);

      if (noteSpread == NoteSpread::SpreadUp) {
        // "Note &note" - the "&" makes this a *reference* to each Note
        // already living inside "notes", not a copy of it. This matters
        // here because the loop body below modifies note.midiChannel - if
        // this were "Note note" (no &), each modification would only
        // affect a throwaway copy, and the real Note in the vector would
        // be untouched. C#/Python don't need this distinction for objects
        // (looping over a list already hands you references to the real
        // objects); this "&" is C++ specifically opting into that same
        // reference behaviour rather than its normal copy-by-default one.
        for (Note &note : notes) {
          note.midiChannel = resolvedChannel;
          sendMidiNoteOn(note, resolvedChannel);
          delay(NoteSpreadUpDownMs);
        }

      } else if (noteSpread == NoteSpread::SpreadDown) {
        // "auto" asks the compiler to work out the variable's type for
        // itself from what it's being set to - similar to C#'s "var".
        // Python doesn't need this at all (variables are never declared
        // with a type up front). ".rbegin()"/".rend()" give a "reverse
        // iterator" - walking the vector back-to-front - there's no
        // single-keyword equivalent in C#/Python; the closest comparison
        // is Python's reversed(notes) or C#'s notes.Reverse().
        for (auto it = notes.rbegin(); it != notes.rend(); ++it) {
          // "it->midiChannel" and "*it" - "it" here is an iterator, which
          // behaves much like a pointer (see Button.h's comment on
          // pointers for more). "*it" means "the Note this iterator is
          // currently pointing at", and "it->midiChannel" is shorthand for
          // "(*it).midiChannel" - reaching through the iterator to a field
          // on the Note it points to. The "->" arrow operator comes up
          // constantly in C++ for exactly this "reach through a
          // pointer/iterator to get a member" operation.
          it->midiChannel = resolvedChannel;
          sendMidiNoteOn(*it, resolvedChannel);
          delay(NoteSpreadUpDownMs);
        }

      } else if (noteSpread == NoteSpread::SpreadRandom) {
        // shuffle a list of indexes into notes, then play them in that order
        std::vector<int> order;
        for (int i = 0; i < (int)notes.size(); i++) {
          order.push_back(i);
        }
        // a classic Fisher-Yates shuffle, walking backwards through the
        // list and swapping each element with a random earlier one
        for (int i = (int)order.size() - 1; i > 0; i--) {
          int j = random(i + 1);
          // std::swap(a, b) exchanges the values of a and b - similar to
          // C#'s (a, b) = (b, a) tuple-swap trick, or Python's
          // a, b = b, a
          std::swap(order[i], order[j]);
        }
        for (int index : order) {
          notes[index].midiChannel = resolvedChannel;
          sendMidiNoteOn(notes[index], resolvedChannel);
          delay(NoteSpreadRandomMs);
        }

      } else {
        for (Note &note : notes) {
          note.midiChannel = resolvedChannel;
          sendMidiNoteOn(note, resolvedChannel);
        }
      }
    };

    // method to run when the button for this action is released
    //
    // The button dispatch logic (Runtime.h) only calls this when the button
    // should actually turn off: on physical release for Momentary buttons,
    // or when a Latched/Exclusive button is deliberately turned off (pressed
    // again, or displaced by a sibling in its Exclusive set). So this always
    // turns the notes off - there's no need to check buttonMode here.
    void OnRelease() override {
      for (Note &note : notes) {
        sendMidiNoteOff(note, note.midiChannel);
      }
    };
};

// matches #ifndef/#define above
#endif
