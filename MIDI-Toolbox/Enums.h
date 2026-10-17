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
#ifndef Enums_h
#define Enums_h

// part of the C++ standard library - gives us std::string_view, a
// lightweight, read-only "view" into a string's characters (used once,
// below, for getButtonModeString)
#include <string_view>

// "enum" in C++ works much like C#'s enum: a fixed set of named integer
// constants. Python has no true built-in equivalent (the "enum" module
// added similar functionality later, but it's a library, not core
// language). Unlike some other languages, plain C++ enum names (Momentary,
// Latched, Exclusive below) are visible directly, not just as
// ButtonMode.Momentary - though this project consistently writes
// ButtonMode::Momentary anyway (the "::" is explained below) for clarity.
//
// the modes for a Button
enum ButtonMode {
  // the button is only on while it is pressed
  Momentary,
  // when pressed the button stays on until it is pressed again
  Latched,
  // the button is one of a set, where only one button in the set can be on at once
  // when pressed all other buttons in the set are turned off, and the pressed button
  // acts in Latched mode
  Exclusive
};

// a free function (see Note.h for what that means) that converts a JSON
// string value (like "Latched") into the matching ButtonMode enum value.
// This is a common pattern throughout this file: JSON files can only
// contain text, numbers, true/false, etc, not C++ enum values directly, so
// every enum in this file gets a matching "read this from JSON" function
// and a "turn this back into readable text" function.
//
// get a ButtonMode enum value from the given JsonVariant value
ButtonMode getButtonModeEnum(JsonVariant value) {
  String valueString = value.as<String>();
  // "==" compares String contents (like C#'s == on strings, or Python's
  // ==) - this is NOT comparing memory addresses the way == on raw C-style
  // strings would; Arduino's String class overloads == to do the sensible
  // text comparison, matching how you'd expect strings to compare in
  // C#/Python.
  if (valueString == "Latched") return ButtonMode::Latched;
  if (valueString == "Exclusive") return ButtonMode::Exclusive;
  // "ButtonMode::Momentary" - the "::" is the scope resolution operator,
  // roughly like C#'s "." when accessing something through a type name
  // (e.g. ButtonMode.Momentary) rather than through an object instance.
  // It says "look inside ButtonMode for something called Momentary".
  return ButtonMode::Momentary;
}

// get the given ButtonMode name as a string
std::string_view getButtonModeString(ButtonMode buttonMode) {
  if (buttonMode == ButtonMode::Latched) return "Latched";
  if (buttonMode == ButtonMode::Exclusive) return "Exclusive";
  return "Momentary";
}

// ----------------------------------------------------------------------------
// The status a Button starts in when the Patch (or Bank/Mode/Config) that
// defines it first becomes the current selection. Defaults to Off - the
// button does nothing until it's physically pressed, exactly as it did
// before this property existed. Set to On, the button's actions are run
// once and the button is marked on the moment its container is loaded, as
// though it had been pressed - see applyDefaultButtonStatuses() in
// Runtime.h. This is for a footswitch that should already be engaged
// whenever you land on a given patch: an always-on clean boost, a default
// amp channel, an effect loop that's normally active, and so on.
//
// The enumerator names are prefixed (ButtonStatusOff/ButtonStatusOn) rather
// than a bare Off/On, the same defensive choice made for CycleDirection's
// CycleUp/CycleDown - "On" and "Off" are common enough words to risk
// clashing with a name from Arduino or a library.
enum ButtonStatus {
  // the button starts off (the default)
  ButtonStatusOff,
  // the button starts on, firing its actions as soon as its container loads
  ButtonStatusOn
};

// get a ButtonStatus enum value from the given JsonVariant value - anything
// that isn't exactly "On" (a missing value included) means Off, the same
// "fail safe to the ordinary behaviour" approach used by getButtonModeEnum
// above
ButtonStatus getButtonStatusEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "On") return ButtonStatus::ButtonStatusOn;
  return ButtonStatus::ButtonStatusOff;
}

// get the given ButtonStatus name as a string
std::string_view getButtonStatusString(ButtonStatus buttonStatus) {
  if (buttonStatus == ButtonStatus::ButtonStatusOn) return "On";
  return "Off";
}

// ----------------------------------------------------------------------------
// How a ModeBank governs the relationship between its Patches' buttons - set
// per bank via the "bankType" JSON key (see ModeBank.h), defaulting to
// Default. This is expected to grow more values over time, each describing a
// different cross-patch button model.
//
//   Default          - a patch change tears the outgoing patch down (every
//                      on button's OnRelease() runs) and re-seeds the
//                      incoming patch from each button's defaultStatus. The
//                      behaviour this project has always had.
//   PersistentButtons - HISTORICAL. This value is still parsed and stored,
//                      but currently has NO runtime effect: a bank behaves
//                      exactly like BankTypeDefault regardless of it. The
//                      "button state carries between sibling containers"
//                      behaviour it used to describe now lives at the Patch
//                      level as pages (see Button.page and
//                      docs/navigation.md). The enum value
//                      is kept because a future bank model may want it.
//
// The enumerator names are prefixed (BankTypeDefault / BankTypePersistent-
// Buttons) rather than a bare Default / PersistentButtons, the same
// defensive choice made for ButtonStatus above - "Default" in particular is
// far too common a word to leave unqualified.
enum BankType {
  // the default cross-patch button handling (reset on every patch change)
  BankTypeDefault,
  // buttons persist across the bank's patches (see the note above)
  BankTypePersistentButtons
};

// get a BankType enum value from the given JsonVariant value - anything that
// isn't a recognised name (a missing value included) means Default, the same
// "fail safe to the ordinary behaviour" approach used by getButtonModeEnum
// and getButtonStatusEnum above
BankType getBankTypeEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "PersistentButtons") return BankType::BankTypePersistentButtons;
  return BankType::BankTypeDefault;
}

// get the given BankType name as a string
std::string_view getBankTypeString(BankType bankType) {
  if (bankType == BankType::BankTypePersistentButtons) return "PersistentButtons";
  return "Default";
}

// ============================================================================
// ScreenLayout - how the device screen is laid out while a ModeBank is
// selected (the ModeBank's "screenLayout" JSON key):
//   Grid - the ordinary layout: header, SubHeader, parameter row and the
//          on-screen button grid.
//   Live - the large-type performance view: Bank / Patch / Page in a big font
//          with the navigation buttons and MIDI channel along the bottom, and
//          no button grid.
// Prefixed enumerators for the same reason as BankType above.
enum ScreenLayout {
  ScreenLayoutGrid,
  ScreenLayoutLive
};

// get a ScreenLayout from the given JsonVariant - anything that isn't "Live"
// (a missing value included) means Grid, so an unknown value fails safe to the
// layout every existing config already has
ScreenLayout getScreenLayoutEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "Live") return ScreenLayout::ScreenLayoutLive;
  return ScreenLayout::ScreenLayoutGrid;
}

// get the given ScreenLayout name as a string
std::string_view getScreenLayoutString(ScreenLayout screenLayout) {
  if (screenLayout == ScreenLayout::ScreenLayoutLive) return "Live";
  return "Grid";
}

// ----------------------------------------------------------------------------
// The types of actions a Button can perform. This is the biggest change in
// the updated config schema, so it's worth explaining the shape of it
// before the plain enum list below:
//
//   - PlayNotes / ProgramChange / ControlChange send MIDI messages, same as
//     before.
//
//   - Every other kind of action now comes in a "Set*"/"Cycle*" pair:
//     "Set*" jumps straight to a specific value (by index or by name),
//     while "Cycle*" moves one step up or down (via a "direction" action
//     parameter) from wherever things currently are. This pairing is
//     consistent across Patch, Bank, Mode, and Config - previously only
//     Patch/Bank/Config had this (as SelectPatch/SelectBank/SelectConfig,
//     with no Cycle* equivalent for Patch/Bank, and no Mode support at
//     all), so CyclePatch, CycleBank, SetMode and CycleMode are all newly
//     added. All of them (and the Set*/Cycle* Config pair) now run through
//     Navigation.h's deferred "pending cursor" - see
//     docs/navigation.md.
//
//   - SetParameter/CycleParameter choose WHICH system parameter (Patch,
//     Bank, Mode, MidiChannel, or Config) the buttons that manipulate "the
//     current parameter" affect - see getCurrentParameters() in Runtime.h.
//     SetParameterValue/CycleParameterValue then set or step that chosen
//     parameter's VALUE. This replaces the old ChangeParameter (which only
//     ever moved forward through the parameter list, with no way to jump
//     to one directly) and the old CycleParameter (which cycled the
//     current parameter's value, now renamed CycleParameterValue to make
//     room for the new, clearer name split).
enum ActionType {
  // default/unrecognised - never produced by a real config file
  NoAction,
  // send MIDI notes
  PlayNotes,
  // send MIDI program change
  ProgramChange,
  // send MIDI control change
  ControlChange,
  // directly set which system parameter (Patch/Bank/Mode/MidiChannel/Config)
  // the CycleParameterValue/SetParameterValue actions affect, by name
  SetParameter,
  // cycle (up or down) through the list of available system parameters,
  // changing which one is current
  CycleParameter,
  // the built-in role of the middle parameter footswitch (physical id 10,
  // printed 11) - see CycleActivateParameterAction.h and
  // docs/navigation.md. No parameters: while a change is
  // pending it activates (commits) that change; otherwise it advances the
  // parameter selector. This is the default action for id 10 on every config
  // (see builtInNavButtons() in Runtime.h) unless a config overrides it.
  CycleActivateParameter,
  // directly set the value of the current system parameter, by name/number
  SetParameterValue,
  // cycle (up or down) the value of the current system parameter
  CycleParameterValue,
  // directly select a Patch by index, within the current Bank
  SetPatch,
  // cycle (up or down) the Patch within the current Bank
  CyclePatch,
  // directly select a Bank by index, within the current Mode
  SetBank,
  // cycle (up or down) the Bank within the current Mode
  CycleBank,
  // directly select a Mode by index
  SetMode,
  // cycle (up or down) the Mode
  CycleMode,
  // select a config file to load, by name
  SetConfig,
  // cycle to the next/previous config file, alphabetically
  CycleConfig,
  // directly select a page by number, within the current Patch
  SetPage,
  // cycle (up or down) the page within the current Patch
  CyclePage,
  // activate (commit) whatever Config/Mode/Bank/Patch/MIDI-channel change
  // has been navigated to but not yet applied - see Navigation.h and
  // docs/navigation.md. A no-op when nothing is pending.
  ActivateSelection,
  // run a preset - a named, reusable list of actions defined once in the
  // config's top-level "presets" array. Unlike every other ActionType this
  // never becomes an IAction object: Button.h expands it, at load time,
  // into the preset's own actions at this position in the button's list.
  ExecutePreset
};

// same "convert from JSON text" pattern as getButtonModeEnum above - a long
// chain of if statements checking the text and returning the matching enum
// value. The JSON string on the left and the C++ enum name on the right
// don't have to match (and in one case below, deliberately don't - see the
// "Note" comment).
//
// get an ActionType enum value from the given JsonVariant value
ActionType getActionTypeEnum(JsonVariant value) {
  String valueString = value.as<String>();
  // "PlayNotes" is the current schema's spelling; "Note" is accepted too,
  // purely so config files written for the previous version of this
  // project keep working without needing to be edited by hand.
  if (valueString == "PlayNotes" || valueString == "Note") return ActionType::PlayNotes;
  if (valueString == "ProgramChange") return ActionType::ProgramChange;
  if (valueString == "ControlChange") return ActionType::ControlChange;
  if (valueString == "SetParameter") return ActionType::SetParameter;
  if (valueString == "CycleParameter") return ActionType::CycleParameter;
  if (valueString == "CycleActivateParameter") return ActionType::CycleActivateParameter;
  if (valueString == "SetParameterValue") return ActionType::SetParameterValue;
  if (valueString == "CycleParameterValue") return ActionType::CycleParameterValue;
  if (valueString == "SetPatch") return ActionType::SetPatch;
  if (valueString == "CyclePatch") return ActionType::CyclePatch;
  if (valueString == "SetBank") return ActionType::SetBank;
  if (valueString == "CycleBank") return ActionType::CycleBank;
  if (valueString == "SetMode") return ActionType::SetMode;
  if (valueString == "CycleMode") return ActionType::CycleMode;
  if (valueString == "SetConfig") return ActionType::SetConfig;
  if (valueString == "CycleConfig") return ActionType::CycleConfig;
  if (valueString == "SetPage") return ActionType::SetPage;
  if (valueString == "CyclePage") return ActionType::CyclePage;
  if (valueString == "ActivateSelection") return ActionType::ActivateSelection;
  if (valueString == "ExecutePreset") return ActionType::ExecutePreset;
  return ActionType::NoAction;
}

// get the given ActionType name as a string
String getActionTypeString(ActionType actionType) {
  if (actionType == ActionType::PlayNotes) return "PlayNotes";
  if (actionType == ActionType::ProgramChange) return "ProgramChange";
  if (actionType == ActionType::ControlChange) return "ControlChange";
  if (actionType == ActionType::SetParameter) return "SetParameter";
  if (actionType == ActionType::CycleParameter) return "CycleParameter";
  if (actionType == ActionType::CycleActivateParameter) return "CycleActivateParameter";
  if (actionType == ActionType::SetParameterValue) return "SetParameterValue";
  if (actionType == ActionType::CycleParameterValue) return "CycleParameterValue";
  if (actionType == ActionType::SetPatch) return "SetPatch";
  if (actionType == ActionType::CyclePatch) return "CyclePatch";
  if (actionType == ActionType::SetBank) return "SetBank";
  if (actionType == ActionType::CycleBank) return "CycleBank";
  if (actionType == ActionType::SetMode) return "SetMode";
  if (actionType == ActionType::CycleMode) return "CycleMode";
  if (actionType == ActionType::SetConfig) return "SetConfig";
  if (actionType == ActionType::CycleConfig) return "CycleConfig";
  if (actionType == ActionType::SetPage) return "SetPage";
  if (actionType == ActionType::CyclePage) return "CyclePage";
  if (actionType == ActionType::ActivateSelection) return "ActivateSelection";
  if (actionType == ActionType::ExecutePreset) return "ExecutePreset";
  return "NoAction";
}

// the directions for cycling parameter values
//
// notice this enum has no "= something" after CycleDown/CycleUp - like C#,
// C++ enum values just count up from 0 by default (CycleDown is 0, CycleUp
// is 1) unless you explicitly assign different numbers
enum CycleDirection {
  CycleDown,
  CycleUp
};

// get an CycleDirection enum value from the given JsonVariant value
CycleDirection getCycleDirectionEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "Up") return CycleDirection::CycleUp;
  return CycleDirection::CycleDown;
}

// get the given CycleDirection name as a string
String getCycleDirectionString(CycleDirection cycleDirection) {
  if (cycleDirection == CycleDirection::CycleUp) return "Up";
  return "Down";
}

// list of system parameters the selector can point at (and that any
// SetParameterValue/CycleParameterValue action affects) - see
// getCurrentParameters() in Runtime.h for how the applicable list is worked
// out (Patch > Bank > Mode > Config precedence), and, in Navigation.h,
// applyCurrentParameterValue() / pendingCycleCurrentParameterValue() for
// what each of these does when it's the current one.
enum Parameter {
  // change the patch
  PatchParameter,
  // change the bank
  BankParameter,
  // change the mode
  ModeParameter,
  // change the MIDI channel
  MIDIChannelParameter,
  // change the loaded config file
  ConfigParameter,
  // change the page within the current Patch (see Button.page and
  // docs/navigation.md). Unlike the others this is applied
  // immediately - a page change tears nothing down - and its value count is
  // the current patch's page count (1 for an ordinary single-page patch, in
  // which case the step arrows simply grey out, exactly as Patch does in a
  // one-patch bank).
  PageParameter
};

// get an Parameter enum value from the given JsonVariant value
Parameter getParameterEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "Patch") return Parameter::PatchParameter;
  if (valueString == "Mode") return Parameter::ModeParameter;
  // "MidiChannel" is the current schema's spelling; "MIDIChannel" (the
  // previous version's all-caps spelling) is still accepted too, so
  // existing config files don't need editing by hand.
  if (valueString == "MidiChannel" || valueString == "MIDIChannel") return Parameter::MIDIChannelParameter;
  if (valueString == "Config") return Parameter::ConfigParameter;
  if (valueString == "Page") return Parameter::PageParameter;
  return Parameter::BankParameter;
}

// get the given Parameter name as a string
String getParameterString(Parameter parameter) {
  if (parameter == Parameter::PatchParameter) return "Patch";
  if (parameter == Parameter::ModeParameter) return "Mode";
  if (parameter == Parameter::MIDIChannelParameter) return "MidiChannel";
  if (parameter == Parameter::ConfigParameter) return "Config";
  if (parameter == Parameter::PageParameter) return "Page";
  return "Bank";
}

// plain global variables, sitting outside of any class or function - these
// are "constants" in spirit (their names are written in a way that
// suggests it) but aren't declared with C++'s "const" keyword, so nothing
// stops other code elsewhere from changing them at runtime. This is
// similar to a module-level variable in Python, or a public static field
// in C#.
//
// number of milliseconds between SpreadUp or SpreadDown notes
int NoteSpreadUpDownMs = 65;
// number of milliseconds between SpreadRandom notes
int NoteSpreadRandomMs = 30;

// list of ways to spread MIDI notes
enum NoteSpread {
  // no note spread
  NoSpread,
  // spread up, with NoteSpreadUpDownMs milliseconds between notes
  SpreadUp,
  // spread down, with NoteSpreadUpDownMs milliseconds between notes
  SpreadDown,
  // spread random, making a chord sound slightly more human, with NoteSpreadRandomMs milliseconds between notes
  SpreadRandom
};

// get an NoteSpread enum value from the given JsonVariant value
NoteSpread getNoteSpreadEnum(JsonVariant value) {
  String valueString = value.as<String>();
  if (valueString == "SpreadUp") return NoteSpread::SpreadUp;
  if (valueString == "SpreadDown") return NoteSpread::SpreadDown;
  if (valueString == "SpreadRandom") return NoteSpread::SpreadRandom;
  return NoteSpread::NoSpread;
}

// get the given NoteSpread name as a string
String getNoteSpreadString(NoteSpread noteSpread) {
  if (noteSpread == NoteSpread::SpreadUp) return "SpreadUp";
  if (noteSpread == NoteSpread::SpreadDown) return "SpreadDown";
  if (noteSpread == NoteSpread::SpreadRandom) return "SpreadRandom";
  return "NoSpread";
}

// matches #ifndef/#define above
#endif
