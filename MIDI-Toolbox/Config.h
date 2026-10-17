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
//                                                                            //
//    This class represents the entire config stored in the JSON config       //
//    file. The config is a hierarchy roughly represented thus:               //
//    Config: {                                                               //
//      Modes: [{                                                             //
//        // The highest organisation level, used to modify the entire        //
//        // working of the unit, for example one mode might be to send       //
//        // MIDI notes like a simple 'keyboard', another mode might be       //
//        // to send program and control changes to amend FX settings.        //
//                                                                            //
//        Banks: [{                                                           //
//          // Contains a collection of patches, a bank might be used for a   //
//          // song, where patches are the set of FX settings used in that    //
//          // song.                                                          //
//                                                                            //
//          Patches: [{                                                       //
//            // Contains configuration for what the buttons do, for example  //
//            // modify FX settings like overdrive, delay etc. Or, buttons    //
//            // in a patch might send collections of MIDI notes as chords.   //
//                                                                            //
//            Buttons: [{                                                     //
//              // Contains the actions a single button invokes for this      //
//              // patch.                                                     //
//                                                                            //
//              Actions: [{                                                   //
//                  // Contains a discrete action for a button. One or more   //
//                  // actions can be invoked by a single button press, for   //
//                  // example sending a MIDI program change and a note       //
//                  // message.                                               //
//              }]                                                            //
//            }]                                                              //
//          }]                                                                //
//        }]                                                                  //
//      }]                                                                    //
//    }                                                                       //
//                                                                            //
//    "buttons" and "parameters" can also be defined directly on Config      //
//    (this level), as well as on Mode, ModeBank, or Patch. This is so you    //
//    can define Button actions, or a parameters list, once at whichever      //
//    level they should apply from - anything more specific (closer to the    //
//    Patch) overrides anything less specific (closer to this Config), the    //
//    same "most specific wins" rule used for midiChannel throughout this     //
//    project - see resolveButtonContainer()/getCurrentParameters() in        //
//    Runtime.h.                                                              //
//                                                                            //
//    The config can also have a top-level "presets" array: named, reusable   //
//    lists of actions. A button runs one with an "ExecutePreset" action      //
//    whose "presetId" is the preset's "id". A button runs its actions        //
//    strictly in the order listed, and an ExecutePreset action runs the      //
//    preset's actions at that point in the list. Presets are expanded into   //
//    each button's own actions as the config loads (see Button.h), so        //
//    nothing about them exists at runtime. A preset can't contain another    //
//    ExecutePreset.                                                          //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef Config_h
#define Config_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

// pulls in the ColourCode, Mode, Button and Parameter/enum types used by
// this file
#include "Enums.h"
#include "ColourCode.h"
#include "Mode.h"
#include "Button.h"

// represents the entire config
//
// note that unlike Patch/Mode/ModeBank/Button, this class doesn't need to
// explicitly delete its copy constructor/assignment operator - it still
// contains a std::vector<Mode> (and Mode is move-only, per Mode.h), so C++
// automatically figures out on its own that Config can't be copied either,
// without needing to be told so directly. It only needed spelling out
// explicitly further down the hierarchy because those classes explicitly
// (if mistakenly) asked the compiler to generate copy behaviour with
// "= default" - simply not mentioning copying at all, as Config does here,
// lets the compiler work out the right answer by itself.
class Config {
  public:
    // the colour of the header bar on the screen - defaults to [22,22,22]
    // per the schema if the config file doesn't set one
    ColourCode headerColourCode = ColourCode(22, 22, 22);

    // whether the config file actually set headerColourCode (via either
    // "headerColourCode" or the legacy "defaultColourCode") rather than the
    // [22,22,22] default above standing in. The header bar itself doesn't
    // care - it draws headerColourCode either way - but the A/B/C footswitch
    // LEDs use this to fall back to their own dim colour when no header
    // colour was configured (see screenSyncButtonLeds() in Screen.h).
    bool hasHeaderColourCode = false;

    // The MIDI channel used by actions that don't configure their own, and
    // don't sit under a Mode/ModeBank/Patch that configures one either -
    // defaults to 1. This is only consulted once, at load time, to seed the
    // runtime's currentMidiChannel variable (see resetRuntimeSelection() in
    // ConfigStore.h) - from that point on, currentMidiChannel is what
    // actually gets used/changed (e.g. by cycling the MidiChannel
    // parameter), same as before this field existed.
    int midiChannel = 1;

    // the system parameters cyclable by SetParameter/CycleParameter, used
    // when no Mode/ModeBank/Patch defines its own list - see
    // getCurrentParameters() in Runtime.h
    std::vector<Parameter> parameters;

    // buttons which apply everywhere, used only when no Mode/ModeBank/Patch
    // defines a Button with the same id - the lowest-priority level in the
    // Patch > ModeBank > Mode > Config override chain (see
    // resolveButtonContainer() in Runtime.h)
    std::vector<Button> buttons;

    // the Modes in the config file
    std::vector<Mode> modes;

    // constructs a new Config object - an empty-bodied constructor, same
    // idea as ColourCode's (see ColourCode.h)
    Config(){}

    // constructs a new Config object from the given JSON document.
    // Takes jsondoc by reference (JsonDocument&), not by value - a by-value
    // copy duplicates the *entire* parsed document (its whole internal
    // memory pool) for the lifetime of this constructor, on top of the
    // caller's original. On a large config file that peak briefly needs
    // roughly double the JSON memory right as the (usually biggest) "modes"
    // array is being walked, which on a memory-constrained board can be
    // enough to exhaust the heap and crash. This constructor only reads
    // from jsondoc and doesn't need its own copy, so a reference is both
    // cheaper and safe.
    Config(JsonDocument &jsondoc) {

      // set the header colour code - "headerColourCode" is the current
      // schema's property name; "defaultColourCode" (the previous
      // version's name for essentially the same thing) is still accepted
      // too, so existing config files don't need editing by hand. If
      // neither is present, the default set above ([22,22,22]) stands.
      print("Getting Config.headerColourCode...", LogLevel::DEBUG);
      if (!jsondoc["headerColourCode"].isNull()) {
        headerColourCode = ColourCode(jsondoc["headerColourCode"].as<JsonArray>());
        hasHeaderColourCode = true;
      } else if (!jsondoc["defaultColourCode"].isNull()) {
        headerColourCode = ColourCode(jsondoc["defaultColourCode"].as<JsonArray>());
        hasHeaderColourCode = true;
      }
      print(" Config.headerColourCode: " + headerColourCode.getString(), LogLevel::DEBUG);

      // set the MIDI channel, if this config configures one
      if (!jsondoc["midiChannel"].isNull()) {
        midiChannel = jsondoc["midiChannel"].as<int>();
      }

      // get the root-level parameters, if any
      if (!jsondoc["parameters"].isNull()) {
        print("Getting Config.parameters...", LogLevel::DEBUG);
        JsonArray parametersArray = jsondoc["parameters"].as<JsonArray>();
        for (JsonVariant parameterItem : parametersArray) {
          parameters.push_back(getParameterEnum(parameterItem));
        }
      }

      // the top-level "presets" array - named, reusable lists of actions
      // that any button can run with an ExecutePreset action. Nothing is
      // stored here: each Button expands the presets it uses into its own
      // action objects while it is being constructed (see Button.h), so
      // this only has to stay valid until the constructor finishes - which
      // it does, because jsondoc outlives it. It is passed down through
      // Mode / ModeBank / Patch to every Button, so it must be read before
      // the buttons and modes below. Absent means "no presets".
      JsonVariant presets = jsondoc["presets"];

      // get the root-level buttons, if any
      if (!jsondoc["buttons"].isNull()) {
        print("Getting Config.buttons...", LogLevel::DEBUG);
        JsonArray buttonsArray = jsondoc["buttons"].as<JsonArray>();
        for (JsonVariant buttonItem : buttonsArray) {
          buttons.push_back(Button(buttonItem, presets));
        }
      }

      // get the Modes
      print("Getting Config.modes...", LogLevel::DEBUG);
      JsonArray modesArray = jsondoc["modes"].as<JsonArray>();
      // walks through each item in the JSON "modes" array, constructing a
      // real Mode object for each one and adding it to the "modes" vector
      // above (see Patch.h for more on this range-based for loop and
      // push_back)
      for (JsonVariant modeItem : modesArray) {
        modes.push_back(Mode(modeItem, presets));
      }

      print("", LogLevel::INFO);
    }

    // get a comma-separated list of mode names
    String getModeNames() {
      String modeNames = "";
      int modeCount = 0;
      // "const Mode &mode" - a reference (see NoteAction.h for what "&"
      // means) to each Mode in turn, and "const" here promises this loop
      // won't modify any of them - it only reads mode.name. Using a
      // reference rather than a plain "Mode mode" avoids trying to copy
      // each Mode, which - as the class comment above explains - isn't
      // even possible for this type.
      for (const Mode &mode : modes) {
        if (modeCount > 0) {
          modeNames += ", ";  
        }
        modeNames += mode.name;
        modeCount++;
      }
      return modeNames;
    }
};

// matches #ifndef/#define above
#endif
