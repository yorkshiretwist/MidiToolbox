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

// include guard - see the top of Note.h for a full explanation of why this
// pattern appears in every header file in this project
#ifndef IAction_h
#define IAction_h

#include <string>
#include <vector>
// a third-party library (not part of C++ or Arduino itself) for reading and
// writing JSON - this project uses it to load the config.json-style files
#include <ArduinoJson.h>

// pulls in the ActionType enum used below
#include "Enums.h"

// This class is what C# calls an "interface" and Python doesn't really have
// a direct equivalent of (Python normally relies on "duck typing" instead -
// any object with the right methods works, no formal interface needed).
// C++ doesn't have a separate "interface" keyword like C# does - instead,
// this is an ordinary class where every method is "pure virtual" (see
// below), which has the same effect: it defines a contract that other
// classes (NoteAction, ProgramChangeAction, etc. - see Button.h) promise to
// fulfil, without providing any implementation itself. You can never create
// an IAction object directly, only objects of classes that implement it.
//
// represents the interface for an action
class IAction {
  public: 
    // A destructor - the opposite of a constructor, called when an object
    // is destroyed. C# and Python don't require you to think about this
    // (their garbage collectors clean up automatically); C++ objects are
    // destroyed deterministically (often when they go out of scope), so
    // classes can define a destructor to run cleanup code at that exact
    // moment. "virtual" here matters a lot: without it, deleting an object
    // through an IAction* pointer would only run IAction's own destructor,
    // not the real subclass's (e.g. NoteAction's) - "virtual" makes the
    // correct one run based on the object's actual type, not the type of
    // pointer used to reach it. This one has an empty body ({}) because
    // there's nothing to clean up here.
    //
    // constructs a new IAction from the given JsonVariant object for the given button mode
    virtual ~IAction() {}

    // "virtual ... = 0;" makes this a "pure virtual" function - it has NO
    // implementation here at all, and any class that inherits from IAction
    // (see Button.h's NoteAction, ProgramChangeAction, etc.) MUST provide
    // one, or it will fail to compile. This is exactly how C# interface
    // methods work (a C# "interface" method has no body either); Python
    // has no direct equivalent, though raising
    // NotImplementedError in a base class method is the closest common
    // pattern. "const" after the parameter list means this method promises
    // not to modify the object it's called on.
    //
    // the type of this action - implemented by each concrete action so it is
    // available polymorphically through an IAction* / unique_ptr<IAction>,
    // e.g. for showing the action type on the screen
    virtual ActionType getType() const = 0;

    // method to run when the button for this action is pressed
    virtual void OnPress() = 0;

    // method to run when the button for this action is released
    virtual void OnRelease() = 0;
};

// matches #ifndef/#define above
#endif
