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

// C++ header files don't have real "modules" like Python or C# namespaces/
// assemblies - instead, every .h file gets copy-pasted (by the preprocessor)
// into every .cpp/.ino file that #includes it. If two different files both
// included Note.h, the "class Note" definition below would get pasted in
// twice, and the compiler would complain about it being defined twice. The
// #ifndef/#define/#endif trio below (an "include guard") prevents that: the
// first time this file is included, Note_h isn't defined yet, so we define
// it and process the rest of the file; any later #include of this same file
// sees Note_h is already defined and skips straight to #endif, so nothing
// gets pasted in twice. Every .h file in this project starts this way.
#ifndef Note_h
#define Note_h

// #include is like Python's "import" or C#'s "using" - it pulls in
// declarations from another file so we can use them here. <string> and
// <vector> are part of the C++ standard library (not Arduino-specific).
#include <string>
#include <vector>

// A "free function" - not attached to any class, similar to a Python
// module-level function or a C# static method. String here is Arduino's
// own string class (not the C++ standard library's std::string) - it works
// much like a C# string or Python str for everyday use (concatenation with
// +, .length(), etc.). Note that C++ requires you to declare the type of
// every parameter and the return value up front (here: takes a String,
// returns an int) - there's no type inference like Python, and no implicit
// "var" like you might reach for in C#.
//
// convert a note name like "C4", "C#4", or "Db4" into a MIDI note number,
// using the common convention where C4 = MIDI note 60 (i.e. C-1 = 0).
// returns -1 if the name can't be parsed.
int noteNameToMidiNumber(String noteName) {
  if (noteName.length() < 2) {
    // print() is a custom function defined elsewhere in this project (in
    // the main .ino file) - it's not a C++ or Arduino built-in. It writes a
    // message to the Serial Monitor, roughly like Console.WriteLine in C#
    // or print() in Python, but gated by a log-level so noisy DEBUG
    // messages can be turned off without deleting the calls.
    print("  Invalid note name: " + noteName, LogLevel::WARNING);
    return -1;
  }

  // the letter name, A-G, gives the semitone offset within the octave
  //
  // "int semitone;" declares a variable without giving it a starting value
  // - unlike C# or Python, C++ does NOT automatically initialise local
  // variables to 0/None/null. If none of the switch cases below ran, this
  // would contain garbage. Here that's fine because every possible branch
  // (including "default") either sets it or returns before it's used.
  int semitone;
  // switch/case works much like C#'s switch or a Python if/elif chain
  // matching on noteName[0] - the first character of the string (accessing
  // a string by index like this, [0], gives you a single character, like
  // Python's noteName[0] or C#'s noteName[0]).
  switch (noteName[0]) {
    // each "case" here ends with "break;", which stops execution falling
    // through into the next case - unlike Python (no switch statement to
    // fall through in) and more explicit than C#, which also requires a
    // break (or similar) in each case.
    case 'C': semitone = 0; break;
    case 'D': semitone = 2; break;
    case 'E': semitone = 4; break;
    case 'F': semitone = 5; break;
    case 'G': semitone = 7; break;
    case 'A': semitone = 9; break;
    case 'B': semitone = 11; break;
    // "default" is what runs if none of the cases above matched - like
    // Python's final "else" in an if/elif chain, or C#'s "default" case.
    default:
      print("  Invalid note letter: " + noteName, LogLevel::WARNING);
      return -1;
  }

  // an optional sharp (#) or flat (b) modifier follows the letter
  //
  // "unsigned int" is an integer that can never be negative (it "wraps
  // around" to a huge positive number instead of going below 0). It's used
  // here because string lengths/indexes can never sensibly be negative -
  // there's no direct equivalent in C# or Python, where a plain int is
  // normally used for this instead.
  unsigned int index = 1;
  if (index < noteName.length() && noteName[index] == '#') {
    semitone += 1;
    index++;
  } else if (index < noteName.length() && noteName[index] == 'b') {
    semitone -= 1;
    index++;
  }

  // whatever remains is the octave number, e.g. "4", "-1"
  //
  // .substring(index) returns everything from position "index" to the end
  // of the string (like Python's noteName[index:] or C#'s
  // noteName.Substring(index)). .toInt() parses that text as a number
  // (like Python's int(...) or C#'s int.Parse(...)).
  int octave = noteName.substring(index).toInt();

  // the function's result - like "return" in C#/Python, this ends the
  // function and hands this value back to whoever called it
  return ((octave + 1) * 12) + semitone;
}

// "class" works much like a class in C# or Python: a blueprint bundling
// together data (fields) and behaviour (methods/constructors). Represents a
// single MIDI note.
class Note {
  // "public:" is an access specifier - it means everything below it (until
  // the next access specifier, or the end of the class) can be read/called
  // from outside this class, similar to C#'s "public" keyword. Python
  // doesn't have this concept built into the language (everything is
  // accessible by convention); C++ classes actually enforce it, and unlike
  // C#, class members are private by default unless you say otherwise -
  // that's why this "public:" line is needed here even though it might
  // look redundant coming from C#.
  public:
    // The name of this note, including the octave e.g.C3
    //
    // A field/member variable, just like in C# or Python (self.name in a
    // Python __init__). No starting value is given here, so it'll be
    // whatever String's own default constructor produces (an empty
    // string) - Arduino's String class always initialises safely, unlike
    // the plain "int semitone" further up.
    String name;

    // The MIDI note number
    //
    // "= -1" here is a default member initialiser: it's the field's value
    // the moment a Note object is created, before any constructor body
    // runs - similar to giving a field an initial value directly in a C#
    // class, or setting self.midiNoteNumber = -1 in Python's __init__.
    int midiNoteNumber = -1;

    // The MIDI channel this note is playing on, used when stopping notes after changing channels
    int midiChannel = 1;

    // the velocity at which this note should be played, default 99
    int velocity = 99;

    // constructs a new Note
    //
    // This is a constructor - like C#'s "public Note()" or Python's
    // "__init__(self)" - it's what runs when you create a new Note object.
    // "= default" tells the compiler "just generate the normal, do-nothing
    // constructor for me" (set every field to whatever its default
    // initialiser above says, and nothing else) - it's a shorthand for
    // writing an empty "Note() {}" body yourself.
    Note() = default;

    // constructs a new Note object from the given JsonVariant value, which
    // is the note name itself, e.g. "C3" (see the "noteNames" array in
    // config.json)
    //
    // This is a second constructor with different parameters - C++ allows
    // multiple constructors (or any function) with the same name as long
    // as their parameter lists differ, called "overloading". C# supports
    // this too; Python doesn't (a Python class only ever has one
    // __init__). JsonVariant is a type from the ArduinoJson library (a
    // third-party library this project depends on) representing one piece
    // of parsed JSON data - roughly like a JToken in C#'s Json.NET, or a
    // value pulled out of a dict after json.loads() in Python.
    Note(JsonVariant json) {

      // set the name
      print("  Getting Note.name... ", LogLevel::DEBUG, false);
      // ".as<String>()" converts/extracts the JSON value as a String -
      // the <String> part is a template argument telling ArduinoJson what
      // type to convert to, similar in spirit to a generic method call
      // like C#'s json.As<string>() or Python's just doing str(value).
      name = json.as<String>();
      print(name, LogLevel::DEBUG);

      // work out the MIDI note number from the name - calling the free
      // function defined above, in this same file
      midiNoteNumber = noteNameToMidiNumber(name);
    }
};

// matches the #ifndef/#define at the top of the file - marks the end of
// the "only process this file once" include guard
#endif
