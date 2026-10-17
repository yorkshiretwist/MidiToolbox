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
//    The reference unit's button ID configuration is:                       //
//                                                                            //
//    [TFT] 10 11 12                                                          //
//                                                                            //
//    5  6  7  8  9                                                           //
//                                                                            //
//    0  1  2  3  4                                                           //
//                                                                            //
//    Nothing in this file enforces that layout, or even a fixed button      //
//    count - MidiToolboxDevice.h (see Pins.h) is what actually defines how  //
//    many footswitches a given unit has, which three ids run the built-in   //
//    navigation role, and where each one is wired. A Button's own "id"      //
//    field (below) is just whatever physical id that unit gave it.          //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef Button_h
#define Button_h

// <memory> gives us std::unique_ptr and std::make_unique, used below - see
// the comment on the "actions" field for what these are and why they're
// needed here
#include <memory>
#include <string>
#include <vector>
#include <ArduinoJson.h>

// pulls in the enums this file uses, the IAction interface, and every
// concrete action class (NoteAction, ProgramChangeAction, etc.) that
// implements it - Button needs to know about all of them so it can create
// the right one for each entry in a Button's "actions" JSON array (see
// setAction() below)
#include "Enums.h"
#include "ColourCode.h"
#include "IAction.h"
#include "NoteAction.h"
#include "ProgramChangeAction.h"
#include "ControlChangeAction.h"
#include "SetParameterAction.h"
#include "CycleParameterAction.h"
#include "CycleActivateParameterAction.h"
#include "SetParameterValueAction.h"
#include "CycleParameterValueAction.h"
#include "SetPatchAction.h"
#include "CyclePatchAction.h"
#include "SetBankAction.h"
#include "CycleBankAction.h"
#include "SetModeAction.h"
#include "CycleModeAction.h"
#include "SetConfigAction.h"
#include "CycleConfigAction.h"
#include "SetPageAction.h"
#include "CyclePageAction.h"
#include "ActivateSelectionAction.h"

// represents a Button, with the properties and actions it invokes for the given mode
class Button {
  public: 

    // the id of this Button, which defines where it is physically on the box
    int id;

    // the name of the Button, shown on the screen
    String name;

    // the Button ids in a set when the Button mode is Exclusive
    std::vector<int> buttonSet;

    // the mode of the Button
    ButtonMode buttonMode;

    // Which page within the Patch this Button belongs to. Pages let one Patch
    // define more buttons than the 13-button hardware has, with the player
    // flipping between pages live (see the Page navigation parameter and the
    // SetPage / CyclePage actions). Default 1. A page of 0 is special: the
    // Button shows on EVERY page of the patch (for a control that should be
    // reachable no matter which page is up). Within a Patch, (id, page) act
    // as a composite key - the same id may appear once per page. Only
    // Patch-level buttons are page-filtered; Bank / Mode / Config buttons
    // always resolve regardless of the current page. See resolveButton() in
    // Runtime.h and docs/navigation.md.
    int page = 1;

    // the status this Button starts in when its Patch/Bank/Mode/Config first
    // becomes the current selection - Off (the default) means "do nothing
    // until pressed"; On means the button's actions are run once and isOn is
    // set the moment that container loads, with no physical press. Applied
    // by applyDefaultButtonStatuses() in Runtime.h, not by this constructor.
    ButtonStatus defaultStatus = ButtonStatus::ButtonStatusOff;

    // Optional per-button background colour for the on-screen button grid
    // (see Screen.h). When hasColourCode is false the grid falls back to the
    // currently-selected bank's subheaderColourCode. The button is drawn in
    // this colour at full opacity while it's on, and at 20% opacity while
    // it's off. Set from the JSON "colourCode" array (e.g. [24, 54, 250]);
    // left unset by an absent "colourCode".
    bool hasColourCode = false;
    ColourCode colourCode;

    // whether this Button is currently "on" - meaningful for Latched and
    // Exclusive buttons, which stay on between presses; always false for
    // Momentary buttons between presses. Managed by the button dispatch
    // logic in Runtime.h, not by the JSON constructor.
    bool isOn = false;

    // This is the trickiest line in the whole project for someone coming
    // from C#/Python, so it's worth unpacking piece by piece:
    //
    // - IAction is an "interface" (see IAction.h) - Button doesn't know or
    //   care exactly which kind of action each one is (NoteAction,
    //   ProgramChangeAction, ...), only that each one can OnPress()/
    //   OnRelease().
    // - "std::unique_ptr<IAction>" is a *pointer* to an IAction - "pointer"
    //   is a C++ concept with no exact match in C#/Python. In Python and
    //   C#, EVERY object variable is really already something like a
    //   pointer/reference under the hood - you never think about it. In
    //   C++, you have a choice: hold the actual object directly, or hold a
    //   pointer that points at an object living somewhere else (typically
    //   on the "heap", the same place C#/Python objects always live). We
    //   need a pointer here specifically because IAction is an interface -
    //   you can't have a "plain IAction" sitting in memory (only concrete
    //   things like NoteAction can actually exist), and the vector needs
    //   a single, consistent type to hold, hence "pointer to an IAction"
    //   rather than "a NoteAction, or a ProgramChangeAction, or...".
    // - A plain/"raw" pointer in C++ (spelled "IAction*") would be exactly
    //   like a reference in C#, but with a big catch: NOTHING automatically
    //   deletes the object it points to when you're done with it (no
    //   garbage collector). Forget to delete it and you leak memory
    //   forever; delete it twice, or use it after deleting it, and the
    //   program can crash unpredictably. "std::unique_ptr" fixes this: it's
    //   a small wrapper around a raw pointer that automatically deletes the
    //   object it owns the moment the unique_ptr itself is destroyed (e.g.
    //   when the vector holding it is destroyed). This pattern - a small
    //   object automatically cleaning something up when it goes out of
    //   scope - is called RAII, and it's how C++ gets predictable cleanup
    //   without a garbage collector. The "unique" part means exactly one
    //   unique_ptr can ever own a given object at a time - trying to copy
    //   one is a compile error (see Patch.h's comments on Patch's
    //   copy/move constructors for the knock-on effect this has).
    // - "std::vector<std::unique_ptr<IAction>>" then, is: a growable list,
    //   where each item is a smart, self-cleaning pointer to some object
    //   that implements the IAction interface. In C# terms, the closest
    //   equivalent is simply "List<IAction>" - C#'s garbage collector makes
    //   all of this pointer/ownership bookkeeping automatic and invisible,
    //   which is exactly what unique_ptr is manually recreating here.
    //
    // the actions performed when the button is pressed
    std::vector<std::unique_ptr<IAction>> actions;

    // See Patch.h for a full explanation of this copy/move-forbidding
    // pattern - required here specifically because "actions" (above) holds
    // unique_ptrs, which can never be copied.
    //
    // constructs a new Button object
    // Button is move-only: it owns its actions via std::unique_ptr<IAction>,
    // which cannot be copied, so the copy operations are deleted rather than
    // defaulted (defaulting them here would be a hard compile error, since a
    // vector<Button> would then try to instantiate a copy constructor that
    // can't exist).
    Button() = default;
    Button(const Button&) = delete;
    Button& operator = (const Button&) = delete;
    Button(Button&& rhs) = default;
    Button& operator = (Button&& other) = default;

    // constructs a new Button object from the given JsonVariant object.
    //
    // "presets" is the config's top-level "presets" array (or a null
    // variant if it has none) - only consulted to expand any ExecutePreset
    // actions. The default value ("= JsonVariant()", a null variant)
    // keeps every existing Button(json) call compiling unchanged.
    Button(JsonVariant json, JsonVariant presets = JsonVariant()) {
      print("", LogLevel::INFO);

      // set the id
      print("    Getting Button.id... ", LogLevel::DEBUG, false);
      id = json["id"].as<int>();
      print((String)id, LogLevel::DEBUG);
      
      // set the name
      print("    Getting Button.name... ", LogLevel::DEBUG, false);
      name = json["name"].as<String>();
      print(name, LogLevel::DEBUG);
      
      // set the button mode
      print("    Getting Button.buttonMode... ", LogLevel::DEBUG, false);
      buttonMode = getButtonModeEnum(json["buttonMode"].as<JsonVariant>());
      print((String)buttonMode, LogLevel::DEBUG);

      // set the page - optional, defaulting to 1. A negative value is
      // nonsense, so fall back to 1 (page 0 is the deliberate "every page"
      // marker and is kept as-is)
      print("    Getting Button.page... ", LogLevel::DEBUG, false);
      page = json["page"].isNull() ? 1 : json["page"].as<int>();
      if (page < 0) {
        page = 1;
      }
      print((String)page, LogLevel::DEBUG);

      // set the default status - optional, so an absent "defaultStatus" (a
      // null JsonVariant) falls through getButtonStatusEnum() to Off
      print("    Getting Button.defaultStatus... ", LogLevel::DEBUG, false);
      defaultStatus = getButtonStatusEnum(json["defaultStatus"].as<JsonVariant>());
      print((String)defaultStatus, LogLevel::DEBUG);

      // set the optional colour code override - an absent "colourCode" (a
      // null JsonVariant) leaves hasColourCode false, and the on-screen grid
      // falls back to the selected bank's subheaderColourCode
      if (!json["colourCode"].isNull()) {
        print("    Getting Button.colourCode... ", LogLevel::DEBUG, false);
        colourCode = ColourCode(json["colourCode"].as<JsonArray>());
        hasColourCode = true;
        print(colourCode.getString(), LogLevel::DEBUG);
      }

      // set the button set
      if (!json["buttonSet"].isNull()) {
        print("    Getting Button.buttonSet...", LogLevel::DEBUG);
        JsonArray buttonSetArray = json["buttonSet"].as<JsonArray>();
        for (JsonVariant buttonSetItem : buttonSetArray) {
          buttonSet.push_back(buttonSetItem.as<int>());
        }
        print("    Button.buttonSet: " + getButtonSetIds(), LogLevel::INFO);
      }

      // get the actions
      if (!json["actions"].isNull()) {
        print("    Getting Button.actions...", LogLevel::DEBUG);
        JsonArray actionsArray = json["actions"].as<JsonArray>();
        for (JsonVariant actionItem : actionsArray) {          
          print(" Getting Action.type... ", LogLevel::DEBUG, false);
          ActionType actionType = getActionTypeEnum(actionItem["type"]);
          print(getActionTypeString(actionType), LogLevel::DEBUG);

          // calls the method defined further down in this same class -
          // just like calling a "self." method in Python or a "this."
          // method in C# (both of which are usually left off, same as
          // here)
          //
          // an ExecutePreset action is replaced, right here, by the
          // preset's own actions - so a button runs its actions strictly
          // top to bottom, and the preset's actions run at the point the
          // ExecutePreset action sits in that list
          if (actionType == ActionType::ExecutePreset) {
            addPresetActions(actionItem, presets);
          } else {
            setAction(actionType, actionItem);
          }
        }
        print("    Button.actions: " + (String)actions.size(), LogLevel::INFO);
      }

      print("", LogLevel::INFO);
    }

    // expands one ExecutePreset action into the actions of the preset it
    // names. The preset's actions are built exactly as if they had been
    // written out in this button's own "actions" list - each one gets its
    // own action object (never shared with another button, or another use
    // of the same preset, since actions hold per-press state such as which
    // notes are currently sounding) and takes this button's buttonMode.
    //
    // A preset that can't be found is skipped with a warning rather than
    // failing the whole config load - the button still loads with its
    // other actions. Presets can't contain ExecutePreset themselves (the
    // editor never produces it, and expanding it here would risk a preset
    // that runs itself), so any such action inside a preset is skipped too.
    void addPresetActions(JsonVariant actionItem, JsonVariant presets) {
      JsonVariant presetId = actionItem["actionParameters"]["presetId"];
      if (presetId.isNull()) {
        print("    ExecutePreset action has no presetId - skipped", LogLevel::WARNING);
        return;
      }
      int wantedId = presetId.as<int>();

      for (JsonVariant preset : presets.as<JsonArray>()) {
        if (preset["id"].as<int>() != wantedId || preset["id"].isNull()) {
          continue;
        }

        print("    Expanding preset " + (String)wantedId + "...", LogLevel::DEBUG);
        for (JsonVariant presetAction : preset["actions"].as<JsonArray>()) {
          ActionType presetActionType = getActionTypeEnum(presetAction["type"]);
          if (presetActionType == ActionType::ExecutePreset) {
            print("    Preset " + (String)wantedId + " contains ExecutePreset - skipped", LogLevel::WARNING);
            continue;
          }
          setAction(presetActionType, presetAction);
        }
        return;
      }

      print("    ExecutePreset: no preset with id " + (String)wantedId + " - skipped", LogLevel::WARNING);
    }

    // get a comma-separated list of button set ids
    String getButtonSetIds() {
      String ids = "";
      int idCount = 0;
      // "int id" here creates a new local variable also called "id" -
      // C++ allows this even though the class also has a field called
      // "id" (see the top of this class); inside this loop, "id" refers to
      // the loop variable, not the field, because the local one "shadows"
      // it. This is legal in C#/Python too, though generally discouraged
      // there for the same reason it can be confusing here.
      for (int id : buttonSet) {
        if (idCount > 0) {
          ids += ", ";  
        }
        ids += (String)id;
        idCount++;
      }
      return ids;
    }

    // add a concrete instance of an action which implements the IAction interface to the actions array
    void setAction(ActionType actionType, JsonVariant actionItem) {

      // each of these blocks does the same thing for a different action
      // type: if the JSON said this action is of this type, construct the
      // real, concrete C++ object for it (NoteAction, ProgramChangeAction,
      // etc.) and add it to "actions" above.
      //
      // "std::make_unique<NoteAction>(buttonMode, actionItem)" creates a
      // new NoteAction object (passing buttonMode and actionItem to its
      // constructor) and immediately wraps it in a unique_ptr - this is
      // the normal, safe way to create a unique_ptr-owned object, broadly
      // comparable to just writing "new NoteAction(buttonMode, actionItem)"
      // in C#, except it also guarantees the object gets an owner
      // immediately with no chance of a mistake in between.
      if (actionType == ActionType::PlayNotes) {
        actions.push_back(std::make_unique<NoteAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::ProgramChange) {
        actions.push_back(std::make_unique<ProgramChangeAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::ControlChange) {
        actions.push_back(std::make_unique<ControlChangeAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetParameter)  {
        actions.push_back(std::make_unique<SetParameterAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleParameter) {
        actions.push_back(std::make_unique<CycleParameterAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleActivateParameter) {
        actions.push_back(std::make_unique<CycleActivateParameterAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetParameterValue) {
        actions.push_back(std::make_unique<SetParameterValueAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleParameterValue) {
        actions.push_back(std::make_unique<CycleParameterValueAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetPatch) {
        actions.push_back(std::make_unique<SetPatchAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CyclePatch) {
        actions.push_back(std::make_unique<CyclePatchAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetBank) {
        actions.push_back(std::make_unique<SetBankAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleBank) {
        actions.push_back(std::make_unique<CycleBankAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetMode) {
        actions.push_back(std::make_unique<SetModeAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleMode) {
        actions.push_back(std::make_unique<CycleModeAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetConfig) {
        actions.push_back(std::make_unique<SetConfigAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CycleConfig) {
        actions.push_back(std::make_unique<CycleConfigAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::SetPage) {
        actions.push_back(std::make_unique<SetPageAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::CyclePage) {
        actions.push_back(std::make_unique<CyclePageAction>(buttonMode, actionItem));
      }

      if (actionType == ActionType::ActivateSelection) {
        actions.push_back(std::make_unique<ActivateSelectionAction>(buttonMode, actionItem));
      }
    };
};

// matches #ifndef/#define above
#endif
