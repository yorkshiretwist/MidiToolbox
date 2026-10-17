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
//    These functions are kept in a .h file, rather than living directly in  //
//    the .ino, on purpose: Arduino's build system automatically scans the   //
//    .ino file and generates forward declarations for every function it     //
//    finds there, inserting them all near the top of the file - before any  //
//    of the .ino's own #include lines have run. That breaks for any         //
//    function whose signature uses one of our own custom types (Mode,       //
//    ModeBank, Patch, Button...), because the type doesn't exist yet at the //
//    point the generated prototype is inserted. Arduino's prototype         //
//    generator only scans .ino files, not .h files, so moving these         //
//    functions here avoids the problem entirely.                            //
//                                                                            //
//    This file assumes it's included from MIDI-Toolbox.ino, after Config.h  //
//    and after the "Runtime variables" section (config, jsonDoc,            //
//    currentModeIndex etc.) and the "screen functions" section (display())  //
//    - it uses all of those directly rather than declaring its own copies.  //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef Runtime_h
#define Runtime_h

// ============================================================================
// Runtime state helpers - resolving the currently selected Mode/Bank/Patch
//
// This whole section leans heavily on C++ pointers, so it's worth pausing
// on them before diving in. A pointer (written with a "*", e.g. "Button *")
// is a variable that holds the memory address of some other object, rather
// than the object itself. It's C++'s more manual, explicit version of what
// C#/Python are always secretly doing under the hood for every object
// variable. Three things make pointers distinctly a "C++ thing" compared
// to C#/Python references:
//  - A pointer can be "null" (pointing at nothing) - written "nullptr" in
//    modern C++, similar to C#'s "null" or Python's "None" - and, unlike
//    C#/Python, the compiler does NOT stop you from accidentally using a
//    null pointer; doing so crashes the program instead of raising a clean
//    exception, so code that returns a pointer almost always needs an
//    explicit "is this null?" check before using it (see getCurrentBank()
//    and its callers below for the pattern).
//  - You get a pointer to an existing variable with "&" (the "address-of"
//    operator - confusingly, "&" means something different here than the
//    "reference parameter" use of "&" seen in NoteAction.h; C++ reuses a
//    few symbols for more than one purpose depending on context).
//  - You reach through a pointer to get to the object with either "*" (get
//    the object itself) or "->" (get one of the object's members directly -
//    "somePointer->field" is shorthand for "(*somePointer).field").

// gets the currently selected Mode
Mode & getCurrentMode() {
  // "config" and "currentModeIndex" are global variables declared earlier
  // in the main .ino file (this header is included after them - see the
  // note at the top of the file). Indexing into the "modes" vector with
  // "[currentModeIndex]" works exactly like C#/Python's list[index].
  return config.modes[currentModeIndex];
}

// gets the currently selected ModeBank, or nullptr if none is selected
// (either this Mode has no Banks, or one hasn't been chosen yet)
ModeBank * getCurrentBank() {
  Mode &mode = getCurrentMode();
  if (currentBankIndex < 0 || currentBankIndex >= (int)mode.banks.size()) {
    // returning nullptr here is this function's way of saying "there is no
    // current bank right now" - similar in spirit to a C# method returning
    // null, or a Python method returning None, to signal "nothing here".
    // Every caller of this function has to check for that before using the
    // result (see the pattern throughout this file).
    return nullptr;
  }
  // "&mode.banks[currentBankIndex]" takes the address of (a pointer to)
  // the specific ModeBank sitting at this index inside the vector - NOT a
  // copy of it. Returning a pointer here (rather than a ModeBank by value)
  // is also required because ModeBank can't be copied at all (see
  // Patch.h's explanation of why) - a pointer is one of the few ways to
  // hand back "this specific object" without copying it.
  return &mode.banks[currentBankIndex];
}

// gets the currently selected Patch, or nullptr if none is selected
Patch * getCurrentPatch() {
  ModeBank *bank = getCurrentBank();
  // "bank == nullptr" checks whether getCurrentBank() found nothing - the
  // "||" (or) short-circuits, so if bank IS null, the rest of the
  // condition (which would dereference bank) never runs, avoiding a crash.
  if (bank == nullptr || currentPatchIndex < 0 || currentPatchIndex >= (int)bank->patches.size()) {
    return nullptr;
  }
  // "bank->patches" - reaching through the "bank" pointer to get at its
  // "patches" field (see the note on "->" above)
  return &bank->patches[currentPatchIndex];
}

// how many pages the given Patch defines - the highest Button.page value
// among its buttons, or 1 if it has none above 1. A page of 0 ("show on
// every page") never raises the count. Used to decide whether the Page
// navigation parameter has anything to step, whether entering the patch
// auto-selects Page, and how many pages applyDefaultButtonStatuses() /
// turnOffAllButtonsInPatch() must sweep. See docs/navigation.md
// section 15.
int patchPageCount(Patch *patch) {
  if (patch == nullptr) {
    return 1;
  }
  int maxPage = 1;
  for (Button &button : patch->buttons) {
    if (button.page > maxPage) {
      maxPage = button.page;
    }
  }
  return maxPage;
}

// gets the system parameters that SetParameter/CycleParameter (and the
// deferred-navigation parameter selector - see Navigation.h) currently
// choose between: the current Patch's parameters if it has any, otherwise the
// current ModeBank's, otherwise the current Mode's, otherwise the root
// Config's - the same "most specific wins" precedence used for midiChannel
// (see resolveMidiChannel() below) and for Button resolution (see
// resolveButtonContainer() further down).
//
// If NO level of the hierarchy sets a "parameters" list, this falls back to
// the full set - Patch, Bank, Mode, MidiChannel, Config - so the parameter
// buttons always have something to cycle. "config.parameters" is defined in
// Config.h (and so out of scope in the unit-test forward declarations); the
// static default below lives here where every caller can reach it.
std::vector<Parameter> & getCurrentParameters() {
  Patch *patch = getCurrentPatch();
  if (patch != nullptr && patch->parameters.size() > 0) {
    return patch->parameters;
  }
  ModeBank *bank = getCurrentBank();
  if (bank != nullptr && bank->parameters.size() > 0) {
    return bank->parameters;
  }
  if (getCurrentMode().parameters.size() > 0) {
    return getCurrentMode().parameters;
  }
  if (config.parameters.size() > 0) {
    return config.parameters;
  }
  // "static" so it outlives this call (callers hold the reference) and is
  // built just once; never mutated anywhere, despite the non-const return
  // type this function has always had.
  static std::vector<Parameter> defaultParameters = {
    Parameter::PatchParameter,
    Parameter::BankParameter,
    Parameter::ModeParameter,
    Parameter::MIDIChannelParameter,
    Parameter::ConfigParameter,
    // Page sits last so the manual selector-advance order (Patch -> Bank ->
    // ... -> Page) is unchanged for existing muscle memory; a multi-page
    // patch auto-selects it anyway on entry (see
    // autoSelectPageParameterIfMultiPage() below). Like Patch/Bank/Mode it
    // is always listed even when there is only one page to step - the step
    // arrows just grey out, per navigationValueCount().
    Parameter::PageParameter
  };
  return defaultParameters;
}

// ============================================================================
// Runtime state helpers - resolving which Button (and so which actions)
// apply for a given physical button id
//
// A lower level always fully overrides a higher level: if the current Patch
// defines a Button with this id, that Button's actions run and any Bank or
// Mode Button with the same id is ignored completely. If the Patch doesn't
// define one (or there's no Patch selected), the Bank is checked, then the
// Mode.

// finds the Button with the given id in the given vector, or nullptr. Used
// for the Bank / Mode / Config levels, which are not page-filtered.
Button * findButtonById(std::vector<Button> &buttons, int id) {
  for (Button &button : buttons) {
    if (button.id == id) {
      // "&button" - the loop variable "button" is a reference to the real
      // Button living inside the vector (see NoteAction.h for what "&"
      // means as a parameter/loop-variable type), so taking its address
      // gives a pointer to that same real object, not a copy.
      return &button;
    }
  }
  return nullptr;
}

// finds the Patch-level Button matching BOTH the given id and the current
// page - a Button with page 0 matches every page. Within a Patch (id, page)
// is a composite key, so this is what keeps the page-2 "id 0" distinct from
// the page-1 "id 0". See docs/navigation.md.
Button * findPatchButtonById(std::vector<Button> &buttons, int id) {
  for (Button &button : buttons) {
    if (button.id == id && (button.page == currentPage || button.page == 0)) {
      return &button;
    }
  }
  return nullptr;
}

// finds the Button for the given id inside a resolved container, honouring
// the page key only when that container is the current Patch's own button
// vector (Bank / Mode / Config buttons are never page-filtered). The
// "&buttons == &patch->buttons" check is an address comparison - both point
// at the very same vector, since getCurrentPatch() returns a pointer into
// the live config tree.
Button * findButtonInContainer(std::vector<Button> &buttons, int id) {
  Patch *patch = getCurrentPatch();
  if (patch != nullptr && &buttons == &patch->buttons) {
    return findPatchButtonById(buttons, id);
  }
  return findButtonById(buttons, id);
}

// true iff a Patch/Bank/Mode/Config explicitly defines the given id itself,
// as opposed to it falling through to the built-in navigation fallback
// (see builtInNavButtons() below). Deliberately mirrors
// resolveButtonContainer()'s own checks one level short of the built-in
// fallback, rather than comparing its result against &builtInNavButtons()
// (which would be equally correct, but this reads more directly as "does
// the config tree itself have an opinion here?"). Only meaningful for
// device.navSelectId/device.navDownId/device.navUpId today - the .ino uses it to decide
// whether the device.navSelectId button still gets its special tap/long-press
// handling (only while it's still doing its built-in role - see
// navButton10Completed() in Navigation.h) or behaves like an ordinary button
// once a config has taken it over.
bool buttonHasExplicitOverride(int id) {
  Patch *patch = getCurrentPatch();
  if (patch != nullptr && findPatchButtonById(patch->buttons, id) != nullptr) {
    return true;
  }

  ModeBank *bank = getCurrentBank();
  if (bank != nullptr && findButtonById(bank->buttons, id) != nullptr) {
    return true;
  }

  if (findButtonById(getCurrentMode().buttons, id) != nullptr) {
    return true;
  }

  if (findButtonById(config.buttons, id) != nullptr) {
    return true;
  }

  return false;
}

// The built-in default Buttons for the three navigation footswitches -
// device.navSelectId/device.navDownId/device.navUpId from DeviceProfile.h (see
// docs/navigation.md). These are what apply when nothing
// in the Patch > Bank > Mode > Config hierarchy defines that id itself (see
// resolveButtonContainer() below) - so the built-in role is present on
// every config without needing to be written into any config's JSON, while
// still being an ordinary, overridable Button/Action like any other id.
// Built once, lazily, from a small in-memory JSON literal per button (reusing
// the normal Button(JsonVariant) constructor rather than a second,
// hand-written construction path) and never mutated afterwards. The three
// ids are only known at compile time as #define constants (not string
// literals), so each JSON document is assembled with ordinary String
// concatenation - "{\"id\":" + device.navSelectId + ",..." - rather than being
// written out as one fixed raw string the way a hard-coded id could be.
std::vector<Button> & builtInNavButtons() {
  static std::vector<Button> buttons;
  static bool initialised = false;
  if (!initialised) {
    JsonDocument doc;

    String selectJson = String("{\"id\":") + device.navSelectId
        + ",\"name\":\"Cycle/Activate Parameter\",\"buttonMode\":\"Momentary\",\"actions\":[{\"type\":\"CycleActivateParameter\"}]}";
    deserializeJson(doc, selectJson);
    buttons.push_back(Button(doc.as<JsonVariant>()));

    doc.clear();
    String downJson = String("{\"id\":") + device.navDownId
        + ",\"name\":\"Cycle Parameter Down\",\"buttonMode\":\"Momentary\",\"actions\":[{\"type\":\"CycleParameterValue\",\"actionParameters\":{\"direction\":\"Down\"}}]}";
    deserializeJson(doc, downJson);
    buttons.push_back(Button(doc.as<JsonVariant>()));

    doc.clear();
    String upJson = String("{\"id\":") + device.navUpId
        + ",\"name\":\"Cycle Parameter Up\",\"buttonMode\":\"Momentary\",\"actions\":[{\"type\":\"CycleParameterValue\",\"actionParameters\":{\"direction\":\"Up\"}}]}";
    deserializeJson(doc, upJson);
    buttons.push_back(Button(doc.as<JsonVariant>()));

    initialised = true;
  }
  return buttons;
}

// resolves which level's buttons vector supplies the Button for the given
// id, applying the Patch > Bank > Mode > Config override order - or nullptr
// if none of them define it. Exclusive button sets are resolved against
// this same vector (see triggerButton() below), which is what keeps a
// Bank's exclusive set separate from a Patch's, the Mode's, or the root
// Config's, even if they happen to reuse
// the same button ids. The Config (root) level is new in this version of
// the schema - it's the lowest-priority fallback, for buttons that should
// behave the same way everywhere, in every Mode. Below even that sits the
// built-in navigation fallback (builtInNavButtons() above), which only ever
// has anything to say about device.navSelectId/device.navDownId/device.navUpId.
std::vector<Button> * resolveButtonContainer(int id) {
  Patch *patch = getCurrentPatch();
  if (patch != nullptr && findPatchButtonById(patch->buttons, id) != nullptr) {
    return &patch->buttons;
  }

  ModeBank *bank = getCurrentBank();
  if (bank != nullptr && findButtonById(bank->buttons, id) != nullptr) {
    return &bank->buttons;
  }

  if (findButtonById(getCurrentMode().buttons, id) != nullptr) {
    return &getCurrentMode().buttons;
  }

  if (findButtonById(config.buttons, id) != nullptr) {
    return &config.buttons;
  }

  if (findButtonById(builtInNavButtons(), id) != nullptr) {
    return &builtInNavButtons();
  }

  return nullptr;
}

// resolves which Button (if any) should handle a press of the given
// physical button id, applying the Patch > Bank > Mode override order
Button * resolveButton(int id) {
  std::vector<Button> *container = resolveButtonContainer(id);
  if (container == nullptr) {
    return nullptr;
  }
  // "*container" - dereferences the pointer to get at the actual vector it
  // points to. findButtonInContainer() applies the page key iff that vector
  // is the current Patch's own.
  return findButtonInContainer(*container, id);
}

// ============================================================================
// Runtime API implementations - sending MIDI messages

// resolves an action's effective MIDI channel, checking each level of the
// config hierarchy from most to least specific:
//   1. the action's own configured channel (1-16), if it has one
//   2. the current Patch's midiChannel, if it's set
//   3. the current ModeBank's midiChannel, if it's set
//   4. the current Mode's midiChannel, if it's set
//   5. the globally-selected MIDI channel (currentMidiChannel) - this is
//      itself seeded from the root Config's midiChannel when a config file
//      loads (see resetRuntimeSelection() in ConfigStore.h), so an
//      unconfigured Config.midiChannel ultimately still applies here, just
//      indirectly, the same way an unset Mode.midiChannel falls through to
//      it rather than needing its own explicit check
// This mirrors the Patch > ModeBank > Mode precedence already used for
// button overrides (see resolveButtonContainer() above) - the closer to
// the button, the higher the priority.
int resolveMidiChannel(int actionMidiChannel) {
  if (actionMidiChannel >= 1 && actionMidiChannel <= 16) {
    return actionMidiChannel;
  }

  Patch *patch = getCurrentPatch();
  if (patch != nullptr && patch->midiChannel >= 1 && patch->midiChannel <= 16) {
    return patch->midiChannel;
  }

  ModeBank *bank = getCurrentBank();
  if (bank != nullptr && bank->midiChannel >= 1 && bank->midiChannel <= 16) {
    return bank->midiChannel;
  }

  Mode &mode = getCurrentMode();
  if (mode.midiChannel >= 1 && mode.midiChannel <= 16) {
    return mode.midiChannel;
  }

  return currentMidiChannel;
}

// "Note &note" - a reference parameter, not a pointer (no "*", no need for
// null-checking or "->") - see NoteAction.h for what "&" means here. This
// function only needs to read the Note's fields, but takes it by reference
// anyway to avoid the (small, but real) cost of copying a Note just to read
// it, which C#/Python don't need to think about since their objects are
// never copied implicitly like this.
void sendMidiNoteOn(Note &note, int midiChannel) {
  if (note.midiNoteNumber < 0) {
    print("Cannot send note on - invalid note: " + note.name, LogLevel::WARNING);
    return;
  }
  print("Sending note on: " + note.name + " (" + (String)note.midiNoteNumber + "), channel " + (String)midiChannel, LogLevel::INFO);
  // usbMIDI is a global object provided by the Teensy's MIDI library (not
  // written in this project) - sendNoteOn() is one of its methods, which
  // actually transmits the MIDI message over USB to whatever's connected.
  usbMIDI.sendNoteOn(note.midiNoteNumber, note.velocity, midiChannel);
}

void sendMidiNoteOff(Note &note, int midiChannel) {
  if (note.midiNoteNumber < 0) {
    return;
  }
  print("Sending note off: " + note.name + " (" + (String)note.midiNoteNumber + "), channel " + (String)midiChannel, LogLevel::INFO);
  usbMIDI.sendNoteOff(note.midiNoteNumber, 0, midiChannel);
}

void sendMidiProgramChange(int programNumber, int midiChannel) {
  print("Sending program change: " + (String)programNumber + ", channel " + (String)midiChannel, LogLevel::INFO);
  usbMIDI.sendProgramChange(programNumber, midiChannel);
}

void sendMidiControlChange(int controllerNumber, int controlValue, int midiChannel) {
  print("Sending control change: " + (String)controllerNumber + " = " + (String)controlValue + ", channel " + (String)midiChannel, LogLevel::INFO);
  usbMIDI.sendControlChange(controllerNumber, controlValue, midiChannel);
}

// ============================================================================
// Runtime API implementations - the system-parameter SELECTOR
//
// "Which system parameter is current" (Patch / Bank / Mode / MidiChannel /
// Config) is a UI cursor - currentParameterIndex into getCurrentParameters().
// SetParameterAction / CycleParameterAction move it, via setCurrentParameter()
// / cycleCurrentParameterSelection() below; it never touches the live
// selection.
//
// Actually applying a value (SetParameterValue / CycleParameterValue) and the
// direct Set*/Cycle* Patch/Bank/Mode/Config actions all go through
// Navigation.h now - pendingCycle*() for the deferred "navigate" verbs,
// applySelection() / applyCurrentParameterValue() for the immediate "set"
// verbs. See docs/navigation.md.

// finds the index of the Patch with the given name within the given Bank,
// or -1 if there's no current Bank or no Patch with that name - used by
// applyCurrentParameterValue() in Navigation.h to resolve a SetParameterValue
// action's "parameterValue" text into a Patch index
int findPatchIndexByName(ModeBank *bank, const String &name) {
  if (bank == nullptr) {
    return -1;
  }
  for (int i = 0; i < (int)bank->patches.size(); i++) {
    if (bank->patches[i].name == name) {
      return i;
    }
  }
  return -1;
}

// finds the index of the Bank with the given name within the given Mode,
// or -1 if none matches
int findBankIndexByName(Mode &mode, const String &name) {
  for (int i = 0; i < (int)mode.banks.size(); i++) {
    if (mode.banks[i].name == name) {
      return i;
    }
  }
  return -1;
}

// finds the index of the Mode with the given name, or -1 if none matches
int findModeIndexByName(const String &name) {
  for (int i = 0; i < (int)config.modes.size(); i++) {
    if (config.modes[i].name == name) {
      return i;
    }
  }
  return -1;
}

// directly sets which system parameter (Patch/Bank/Mode/MidiChannel/
// Config) SetParameterValue/CycleParameterValue actions currently affect,
// by name - see SetParameterAction. Only succeeds if "parameter" actually
// appears in the applicable parameters list (see getCurrentParameters()
// above); a Bank that only lists ["Patch"], for example, can't have its
// current parameter set to Bank.
void setCurrentParameter(Parameter parameter) {
  std::vector<Parameter> &parameters = getCurrentParameters();
  for (int i = 0; i < (int)parameters.size(); i++) {
    if (parameters[i] == parameter) {
      currentParameterIndex = i;
      print("Current parameter: " + getParameterString(parameters[currentParameterIndex]), LogLevel::INFO);
      return;
    }
  }
  print("Parameter not available in the current parameter list: " + getParameterString(parameter), LogLevel::WARNING);
}

// moves to the next/previous system parameter (e.g. Patch -> Bank -> Mode
// -> MidiChannel -> Config -> Patch...) that SetParameterValue/
// CycleParameterValue actions currently affect - see CycleParameterAction.
// This is what the previous version of this project's parameterless
// ChangeParameterAction did (always moving forward one step); now
// direction-aware, via CycleParameterAction's "direction" parameter.
void cycleCurrentParameterSelection(CycleDirection direction) {
  std::vector<Parameter> &parameters = getCurrentParameters();
  if (parameters.size() == 0) {
    print("No parameters are configured to cycle through", LogLevel::WARNING);
    return;
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  int count = (int)parameters.size();
  currentParameterIndex = ((currentParameterIndex + step) % count + count) % count;
  print("Current parameter: " + getParameterString(parameters[currentParameterIndex]), LogLevel::INFO);
  display();
}

// The VALUE side - setCurrentParameterValue() / cycleCurrentParameterValue()
// - moved to Navigation.h as applyCurrentParameterValue() (immediate, by
// name) and pendingCycleCurrentParameterValue() (deferred). See
// docs/navigation.md.

// ============================================================================
// Button functions
//
// Momentary buttons turn on when pressed and off when released.
//
// Latched buttons turn on when pressed, and turn off when pressed again -
// pressing a Latched button does nothing on release.
//
// Exclusive buttons work in a set (Button.buttonSet lists the other button
// ids in the set). Pressing an Exclusive button that's off turns off every
// other currently-on button in its set, then turns itself on. Pressing an
// Exclusive button that's already on turns it off, same as a Latched
// button. Exclusive buttons also do nothing on release. A button's set is
// only ever searched within the same level (Patch, Bank, or Mode) that the
// button itself came from - see resolveButtonContainer() above - so a set
// defined at Bank level can't reach into a Patch's or the Mode's buttons.

// the id of the physical button whose press is currently being handled by
// triggerButton() below (including everywhere further down the same call
// stack - a Set*/Cycle* action's OnPress() can re-enter applySelection()),
// or -1 when no press is in progress. turnOffAllActiveButtons() and
// turnOffAllButtonsInPatch() skip this id, so a button whose own action
// navigates elsewhere (e.g. SetPatch) doesn't undo its own isOn on the way
// through - see triggerButton() for where this is set, and
// docs/navigation.md for the failure this prevents. A single
// variable (not a stack) is enough: only one physical press is ever in
// progress at a time, even though handling it can re-enter applySelection()
// several layers deep.
int buttonBeingPressed = -1;

// turns a Button on: marks it on, then runs OnPress() on all its actions.
// isOn is set BEFORE the actions run, not after: a Set*/Cycle* action's
// OnPress() (e.g. SetPatchAction -> navSetPatch() -> applySelection()) can
// re-enter applyDefaultButtonStatuses() further down the same call stack,
// which resolves this exact Button again (directly, or via a shared Bank/
// Mode-level button) and checks isOn to decide whether to fire it. If isOn
// were still false at that point, the guard would not recognise this
// button as already "in the middle of" turning on, and would turn it on
// again - re-entering its OnPress() a second time, which re-enters
// applySelection() again, and so on without end, until the stack overflows
// and the device crashes/reboots. Marking it on first closes that hole.
void turnButtonOn(Button &button) {
  button.isOn = true;
  // "std::unique_ptr<IAction> &action" - a reference to each smart pointer
  // stored in button.actions (see Button.h for what unique_ptr means).
  // "action->OnPress()" reaches through the smart pointer to call OnPress()
  // on whatever concrete action object it owns (NoteAction, ProgramChangeAction,
  // etc.) - this is polymorphism in action: this code has no idea which
  // concrete type each action actually is, only that it implements
  // IAction's OnPress() method, and the correct version runs automatically
  // based on the object's real type. C# and Python both do this too
  // (it's a completely ordinary use of inheritance/interfaces there); the
  // difference here is purely the unique_ptr wrapper needed to manage
  // memory manually.
  for (std::unique_ptr<IAction> &action : button.actions) {
    action->OnPress();
  }
}

// turns a Button off: runs OnRelease() on all its actions and marks it off
void turnButtonOff(Button &button) {
  for (std::unique_ptr<IAction> &action : button.actions) {
    action->OnRelease();
  }
  button.isOn = false;
}

// How many selection passes are in progress right now: applySelection()
// (Navigation.h) and applyDefaultButtonStatuses() each hold one for as long as
// they run. A Set* navigation action that targets the selection we are already
// on is a deliberate "reset" when a player presses it, but it must do nothing
// when it is fired from inside one of those passes - that is a defaultStatus:On
// button whose own action selects its own patch, and honouring it would tear the
// selection down and re-seed it, firing that same button again, without end
// (the stack-overflow crash described in docs/navigation.md). See
// navSetPatch() in Navigation.h.
int selectionPassDepth = 0;

// holds one level of selectionPassDepth for the scope it is declared in, and
// releases it on every way out (including early returns)
struct SelectionPassScope {
  SelectionPassScope() { selectionPassDepth++; }
  ~SelectionPassScope() { selectionPassDepth--; }
};

// Runs the actions for - and marks on - every Button in the current
// selection whose defaultStatus is On (see Button.defaultStatus and the
// ButtonStatus enum in Enums.h). Called by applySelection() in Navigation.h
// whenever the live Mode/Bank/Patch changes, and once when a config file
// finishes loading (resetRuntimeSelection() in ConfigStore.h), so a button
// configured to start on takes effect the instant you land on the patch that
// defines it, exactly as if it had been pressed. A button that's already on
// is left alone, so re-activating the same selection never re-fires it.
//
// It's forward-declared in the main .ino (and in the test harness) rather
// than here, because resetRuntimeSelection() in ConfigStore.h (included
// before this file) calls it, but it needs turnButtonOn()/resolveButton(),
// which are defined lower down this file.
//
// patchLevelOnly (default false) restricts the sweep to ids that resolve to
// a Patch-level Button - i.e. skips any id that falls through to a Bank /
// Mode / Config-level Button entirely, rather than checking its isOn at
// all. applySelection() (Navigation.h) passes true for a same-bank patch
// change: a Bank/Mode/Config-level Button was already seeded when that
// level was itself freshly entered, and re-checking it here on every
// sibling patch change would re-fire it whenever something else (e.g. the
// ordinary Exclusive-set cancel, or this same sweep turning off the
// previous patch's buttons) happens to have left it momentarily off - which
// is exactly how a Bank-level defaultStatus:On button whose action is
// SetPatch ends up re-triggering itself and dragging the selection back to
// its own target on every sibling press, instead of just seeding once on
// entry to the bank. See docs/navigation.md for the full incident.
void applyDefaultButtonStatuses(bool patchLevelOnly) {
  SelectionPassScope inSelectionPass; // see selectionPassDepth above
  // A multi-page patch is entered as a whole (see docs/navigation.md
  // section 15): every page's default-on buttons must fire, not just the
  // page that happens to be current. We do that by sweeping each page in
  // turn - resolveButton() reads the global currentPage, so temporarily
  // point it at each page, then restore it. On every non-page path (bank /
  // mode level, or an ordinary single-page patch) this loop runs exactly
  // once with pages == 1, identical to the old behaviour. The !isOn guard in
  // the body keeps a page-0 "every page" button from being fired more than
  // once.
  Patch *patch = getCurrentPatch();
  int pages = patchPageCount(patch);
  int savedPage = currentPage;
  for (int p = 1; p <= pages; p++) {
    currentPage = p;
    // physical button ids run 0..device.buttonCount-1 - see DeviceProfile.h
    for (int id = 0; id < device.buttonCount; id++) {
      if (patchLevelOnly
          && (patch == nullptr || findPatchButtonById(patch->buttons, id) == nullptr)) {
        continue; // this id isn't a Patch-level Button - already seeded when its own level was entered
      }
      Button *button = resolveButton(id);
      if (button != nullptr
          && button->defaultStatus == ButtonStatus::ButtonStatusOn
          && !button->isOn) {
        turnButtonOn(*button);
      }
    }
  }
  currentPage = savedPage;
}

// Turns OFF every currently-on Button in the current selection, running each
// one's OnRelease() actions (note-offs, CC releases, ...). The mirror image
// of applyDefaultButtonStatuses(). This is how a pending Config/Mode/Bank/
// Patch change stops the outgoing selection's held sound at the instant of
// activation - and nowhere earlier: see applySelection() in Navigation.h,
// and the rationale in docs/navigation.md.
//
// Skips buttonBeingPressed: a button whose own action navigates (e.g.
// SetPatch) is already marked isOn before that action runs (see
// turnButtonOn()), and without this guard this sweep - reached via that
// same action's applySelection() call - would immediately turn it back off
// again, so the button it never visibly ends up lit even though the press
// "took". The button it turns off instead is whichever OTHER button was
// previously held; the ordinary Exclusive-set cancel in triggerButton()
// already turned off this press's own siblings before we ever got here.
void turnOffAllActiveButtons() {
  for (int id = 0; id < device.buttonCount; id++) {
    if (id == buttonBeingPressed) {
      continue;
    }
    Button *button = resolveButton(id);
    if (button != nullptr && button->isOn) {
      turnButtonOff(*button);
    }
  }
}

// True if any Button in the current selection is on - held down, or latched
// on - on ANY page of the current Patch (a multi-page Patch leaves latched
// buttons on in pages other than the one showing; see
// turnOffAllButtonsInPatch() below). Changes nothing: it steps through
// the pages the same way applyDefaultButtonStatuses() does, putting
// currentPage back afterwards. The serial protocol uses it to refuse to
// restart the unit while something may still be sounding, since a restart
// would never send the note-offs and leave notes stuck on the synth.
bool anyButtonActive() {
  if (config.modes.empty()) {
    return false; // no config loaded yet (e.g. safe mode), so nothing can be on
  }
  Patch *patch = getCurrentPatch();
  int pages = patchPageCount(patch);
  int savedPage = currentPage;
  bool active = false;
  for (int p = 1; p <= pages && !active; p++) {
    currentPage = p;
    for (int id = 0; id < device.buttonCount && !active; id++) {
      Button *button = resolveButton(id);
      active = (button != nullptr && button->isOn);
    }
  }
  currentPage = savedPage;
  return active;
}

// Turns OFF every currently-on Button on EVERY page of the given Patch,
// firing each one's OnRelease() actions (note-offs, CC releases).
// turnOffAllActiveButtons() above only reaches buttons that resolve for the
// current page; a multi-page patch deliberately leaves buttons latched on
// on pages other than the one on screen (a page change tears nothing down -
// see docs/navigation.md), so the moment the patch is left
// that held sound would be stranded with nothing able to release it unless
// every page is swept. Called from applySelection() in Navigation.h on any
// Patch / Bank / Mode / Config change, alongside (not instead of)
// turnOffAllActiveButtons(), which still handles the Bank / Mode / Config-
// level buttons this does not walk. Both run in the same teardown step, so
// every outgoing note-off still precedes every incoming note-on.
//
// Skips buttonBeingPressed too - see the comment on turnOffAllActiveButtons()
// above for why: a Patch-level button whose own action navigates away from
// this very Patch would otherwise be turned back off by this same sweep, on
// its own way out. Matched by (id, page) exactly as findPatchButtonById()
// does - patch.buttons spans every page, and (id, page) is the composite key
// within a Patch, so a different page's Button sharing the same id is a
// distinct object that still needs tearing down here, not skipped.
void turnOffAllButtonsInPatch(Patch &patch) {
  for (Button &button : patch.buttons) {
    bool isBeingPressed = button.id == buttonBeingPressed
        && (button.page == currentPage || button.page == 0);
    if (isBeingPressed) {
      continue;
    }
    if (button.isOn) {
      turnButtonOff(button);
    }
  }
}

// Turns OFF every Button that is on anywhere in the current selection - the
// Patch (all its pages), Bank, Mode and Config levels - firing each one's
// OnRelease() actions, exactly as a Patch/Bank/Mode change does when it tears
// the outgoing selection down (see applySelection() in Navigation.h). The
// serial protocol uses it before a forced restart, so the synth gets its
// note-offs instead of being left with stuck notes.
void releaseAllActiveButtons() {
  if (config.modes.empty()) {
    return;
  }
  Patch *patch = getCurrentPatch();
  if (patch != nullptr) {
    turnOffAllButtonsInPatch(*patch);
  }
  turnOffAllActiveButtons();
}

// Collects the ids of every currently-on Button in the given Patch. Used by
// applySelection() in Navigation.h to remember, before it tears the outgoing
// Patch down, which buttons a ModeBank with MaintainActiveButtonOnPatchChange
// set (see ModeBank.h) needs to re-trigger against the incoming Patch once
// it's seeded.
std::vector<int> collectOnButtonIds(Patch &patch) {
  std::vector<int> ids;
  for (Button &button : patch.buttons) {
    if (button.isOn) {
      ids.push_back(button.id);
    }
  }
  return ids;
}

// If the current Patch has more than one page, point the parameter selector
// at the Page parameter, so the left/right step buttons immediately cycle
// pages the moment the player lands on a multi-page patch (see
// docs/navigation.md). Single-page patches leave the
// selector exactly where it was. A no-op if the applicable parameter list
// (see getCurrentParameters()) doesn't include Page - e.g. a mode that
// declares a restricted "parameters" list without it.
void autoSelectPageParameterIfMultiPage() {
  if (patchPageCount(getCurrentPatch()) <= 1) {
    return;
  }
  std::vector<Parameter> &parameters = getCurrentParameters();
  for (int i = 0; i < (int)parameters.size(); i++) {
    if (parameters[i] == Parameter::PageParameter) {
      currentParameterIndex = i;
      return;
    }
  }
}

// when a button is pressed
void triggerButton(int buttonIndex) {
  std::vector<Button> *container = resolveButtonContainer(buttonIndex);
  if (container == nullptr) {
    print("No button configured for id " + (String)buttonIndex, LogLevel::DEBUG);
    return;
  }

  Button *button = findButtonInContainer(*container, buttonIndex);
  if (button == nullptr) {
    return;
  }

  // marks this id as "the button whose press is in progress" for the rest
  // of this call, including everything its own action(s) do further down
  // the same call stack (e.g. a SetPatch's applySelection()) - see
  // turnOffAllActiveButtons() / turnOffAllButtonsInPatch() above for what
  // this protects against, and buttonBeingPressed's own comment for why a
  // single save/restore (not a stack) is enough. Restored on every path out
  // of this function, not just reset to -1, so a re-entrant call further up
  // an already-in-progress press (there isn't one today, but nothing stops
  // a future action from calling triggerButton() itself) leaves the outer
  // press's id correctly exempted again once the inner one returns.
  int previousButtonBeingPressed = buttonBeingPressed;
  buttonBeingPressed = buttonIndex;

  if (button->buttonMode == ButtonMode::Momentary) {
    // "*button" dereferences the pointer to pass the actual Button object
    // (by reference - turnButtonOn takes "Button &button") to turnButtonOn
    turnButtonOn(*button);

  } else if (button->buttonMode == ButtonMode::Latched) {
    if (button->isOn) {
      turnButtonOff(*button);
    } else {
      turnButtonOn(*button);
    }

  } else if (button->buttonMode == ButtonMode::Exclusive) {
    if (button->isOn) {
      // pressing an already-on Exclusive button turns it off, like Latched
      turnButtonOff(*button);
    } else {
      // turn off every other currently-on button in this Button's set
      for (int siblingId : button->buttonSet) {
        // "continue" skips straight to the next loop iteration, same
        // meaning as C#'s "continue" or Python's "continue"
        if (siblingId == buttonIndex) {
          continue;
        }
        // page-aware, so an Exclusive set only ever cancels siblings on the
        // same page (see docs/navigation.md)
        Button *sibling = findButtonInContainer(*container, siblingId);
        if (sibling != nullptr && sibling->isOn) {
          turnButtonOff(*sibling);
        }
      }
      turnButtonOn(*button);
    }
  }

  buttonBeingPressed = previousButtonBeingPressed;
}

// when a button is released
void releaseButton(int buttonIndex) {
  std::vector<Button> *container = resolveButtonContainer(buttonIndex);
  if (container == nullptr) {
    return;
  }

  Button *button = findButtonInContainer(*container, buttonIndex);
  if (button == nullptr) {
    return;
  }

  // only Momentary buttons act on release - Latched and Exclusive buttons
  // are turned off by being pressed again (see triggerButton() above)
  if (button->buttonMode != ButtonMode::Momentary) {
    return;
  }

  turnButtonOff(*button);
}

// matches #ifndef/#define above
#endif
