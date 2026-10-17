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
//    Hierarchy navigation - the "pending cursor" that lets the player move   //
//    through Config / Mode / Bank / Patch (and MIDI channel) WITHOUT         //
//    anything taking effect until an explicit activation. See                //
//    docs/navigation.md for the full design and rationale; the        //
//    short version is that this is a live-performance instrument, so         //
//    cycling through candidate patches must never stop the notes the         //
//    current patch is holding - that only happens the moment the player      //
//    commits.                                                                //
//                                                                            //
//    This file (step 1 of the rollout in the design doc) provides only the   //
//    pending state and the pure cursor-movement helpers. Nothing calls       //
//    them yet - applySelection(), ActivateSelectionAction and the button     //
//    routing come in later steps. It lives in a .h file, like Runtime.h /    //
//    ConfigStore.h - see the comment at the top of Runtime.h for why - and   //
//    assumes it is included from MIDI-Toolbox.ino AFTER Runtime.h and        //
//    ConfigStore.h (it uses getCurrentMode()-style state, the find*ByName()  //
//    helpers, listConfigFiles() and the current* runtime globals, all        //
//    defined there).                                                         //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef Navigation_h
#define Navigation_h

#include <vector>

#include "Enums.h"

// ============================================================================
// pending cursor state
//
// These mirror the live selection globals (currentConfigFilename,
// currentModeIndex, currentBankIndex, currentPatchIndex, currentMidiChannel
// - declared in the .ino / the test harness). The live globals are what
// actually produces MIDI right now and change ONLY at activation; the
// pending values below are where the player is navigating to.

String pendingConfigFilename = "";
int pendingModeIndex = 0;
int pendingBankIndex = -1;
int pendingPatchIndex = -1;
int pendingMidiChannel = 1;

// ============================================================================
// candidate config peek
//
// The first Mode / Bank / Patch names of the pending config file, cached so
// the screen flash timer (which fires ~20x/second) never has to touch the
// SD card. Refreshed by navRefreshConfigPeek() whenever pendingConfigFilename
// changes. All "-" when the pending config is the running one.

String navPeekMode = "-";
String navPeekBank = "-";
String navPeekPatch = "-";

// reads the first Mode / Bank / Patch names out of a config file WITHOUT
// loading it as the running config. Parses into a local JsonDocument (never
// the global jsonDoc, which backs the live config). Returns false if the
// file can't be opened or parsed; the out-params are set to "-" in that
// case.
static bool navPeekConfigFirstNames(const String &filename, String &modeName,
                                    String &bankName, String &patchName) {
  modeName = "-";
  bankName = "-";
  patchName = "-";

  String path = String(CONFIG_DIR) + "/" + filename;
  File file = SD.open(path.c_str());
  if (!file) {
    return false;
  }

  JsonDocument doc;
  DeserializationError error =
      deserializeJson(doc, file, DeserializationOption::NestingLimit(50));
  file.close();
  if (error != DeserializationError::Ok) {
    return false;
  }

  JsonVariant mode = doc["modes"][0];
  if (!mode.isNull()) {
    if (!mode["name"].isNull()) {
      modeName = mode["name"].as<String>();
    }
    JsonVariant bank = mode["banks"][0];
    if (!bank.isNull()) {
      if (!bank["name"].isNull()) {
        bankName = bank["name"].as<String>();
      }
      JsonVariant patch = bank["patches"][0];
      if (!patch.isNull() && !patch["name"].isNull()) {
        patchName = patch["name"].as<String>();
      }
    }
  }
  return true;
}

// refreshes navPeek{Mode,Bank,Patch} for the current pendingConfigFilename
static void navRefreshConfigPeek() {
  if (pendingConfigFilename.length() == 0
      || pendingConfigFilename == currentConfigFilename) {
    navPeekMode = "-";
    navPeekBank = "-";
    navPeekPatch = "-";
    return;
  }
  navPeekConfigFirstNames(pendingConfigFilename, navPeekMode, navPeekBank,
                          navPeekPatch);
}

// ============================================================================
// syncing pending <-> live

// copies the live selection onto the pending cursor, so navigationDirty()
// becomes false. Called at boot (setup()), after every applySelection(), and
// after a config load.
void resetPendingToLive() {
  pendingConfigFilename = currentConfigFilename;
  pendingModeIndex = currentModeIndex;
  pendingBankIndex = currentBankIndex;
  pendingPatchIndex = currentPatchIndex;
  pendingMidiChannel = currentMidiChannel;
  navRefreshConfigPeek();
}

// full reset to the cold-boot defaults - only for the unit-test harness's
// resetTestState() (see test/test_main.cpp), mirroring how the live current*
// globals are reset there so no test leaks state into the next.
void resetNavigationState() {
  pendingConfigFilename = "";
  pendingModeIndex = 0;
  pendingBankIndex = -1;
  pendingPatchIndex = -1;
  pendingMidiChannel = 1;
  navPeekMode = "-";
  navPeekBank = "-";
  navPeekPatch = "-";
}

// ============================================================================
// dirty state
//
// Each level's flag also reports dirty when a HIGHER level differs: changing
// Config re-roots Mode/Bank/Patch, and changing Mode or Bank forces the
// levels beneath back to index 0, so a higher change means the lower
// readouts are showing a pending value too. Screen.h flashes a label iff its
// level is dirty (wired in a later step).

bool navigationConfigDirty() {
  return pendingConfigFilename != currentConfigFilename;
}
bool navigationModeDirty() {
  return navigationConfigDirty() || pendingModeIndex != currentModeIndex;
}
bool navigationBankDirty() {
  return navigationModeDirty() || pendingBankIndex != currentBankIndex;
}
bool navigationPatchDirty() {
  return navigationBankDirty() || pendingPatchIndex != currentPatchIndex;
}
bool navigationChannelDirty() {
  return pendingMidiChannel != currentMidiChannel;
}

// true if the pending cursor differs from the live selection anywhere - the
// single condition behind all preview flashing and the "Set?" label on
// button 10. navigationPatchDirty() already cascades up through Bank / Mode /
// Config, so this is just "the hierarchy or the channel differs".
bool navigationDirty() {
  return navigationPatchDirty() || navigationChannelDirty();
}

// ============================================================================
// internal helpers

// the Mode the pending cursor points at, clamped into range. Precondition:
// config.modes is non-empty - every public mutator below checks that first.
static Mode &navPendingMode() {
  int index = pendingModeIndex;
  if (index < 0 || index >= (int)config.modes.size()) {
    index = 0;
  }
  return config.modes[index];
}

// re-seeds pending Bank/Patch after the pending Mode moved. Normally that's
// "land on Bank 0 / Patch 0" (as selectMode() does for the live selection -
// see docs/navigation.md); but if the move landed back on
// the LIVE Mode, restore its live Bank/Patch instead, so navigating away
// and back to the current mode clears the pending state rather than leaving
// it dirty on a reset-to-zero Bank/Patch.
static void navResetBankPatchForPendingMode() {
  if (pendingModeIndex == currentModeIndex) {
    pendingBankIndex = currentBankIndex;
    pendingPatchIndex = currentPatchIndex;
    return;
  }
  Mode &mode = navPendingMode();
  pendingBankIndex = (mode.banks.size() > 0) ? 0 : -1;
  pendingPatchIndex =
      (pendingBankIndex >= 0) ? mode.banks[0].resolveDefaultPatchIndex() : -1;
}

// re-seeds pending Patch after the pending Bank moved: patch 0 of the new
// bank, or - if the move landed back on the live Mode's live Bank - the live
// patch, so navigating away and back clears the pending state.
static void navSeedPendingPatchForBank() {
  if (pendingModeIndex == currentModeIndex
      && pendingBankIndex == currentBankIndex) {
    pendingPatchIndex = currentPatchIndex;
    return;
  }
  Mode &mode = navPendingMode();
  bool hasBank = pendingBankIndex >= 0
      && pendingBankIndex < (int)mode.banks.size();
  pendingPatchIndex = hasBank
      ? mode.banks[pendingBankIndex].resolveDefaultPatchIndex() : -1;
}

// after the pending config filename changes, the pending Mode/Bank/Patch
// indices refer to a tree that isn't loaded. If we've landed back on the
// running config, restore its live position (so cycling a full loop clears
// dirty); otherwise drop to the top - applySelection() clamps against the
// real tree when the change is finally activated.
static void navSyncPendingTreeToConfig() {
  if (pendingConfigFilename == currentConfigFilename) {
    pendingModeIndex = currentModeIndex;
    pendingBankIndex = currentBankIndex;
    pendingPatchIndex = currentPatchIndex;
  } else {
    pendingModeIndex = 0;
    pendingBankIndex = 0;
    pendingPatchIndex = 0;
  }
  navRefreshConfigPeek();
}

// "((x % n) + n) % n" wraps an index both up and down - the same trick used
// throughout Runtime.h (a plain C++ "%" can return a negative result).
static int navWrap(int value, int count) {
  return ((value % count) + count) % count;
}

// ============================================================================
// pending cursor movement - NAVIGATE verbs
//
// Pure index maths over the config tree: no MIDI, no applySelection(), no
// side effect beyond moving the pending indices. A move that has nothing to
// step through (empty / single-entry level) is a silent no-op, exactly as
// the live cycle* helpers in Runtime.h behave.

void pendingCycleMode(CycleDirection direction) {
  int count = (int)config.modes.size();
  if (count == 0) {
    return;
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  pendingModeIndex = navWrap(pendingModeIndex + step, count);
  navResetBankPatchForPendingMode();
}

void pendingCycleBank(CycleDirection direction) {
  if (config.modes.empty()) {
    return;
  }
  Mode &mode = navPendingMode();
  int count = (int)mode.banks.size();
  if (count == 0) {
    return;
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  pendingBankIndex = navWrap(pendingBankIndex + step, count);
  navSeedPendingPatchForBank();
}

void pendingCyclePatch(CycleDirection direction) {
  if (config.modes.empty()) {
    return;
  }
  Mode &mode = navPendingMode();
  if (pendingBankIndex < 0 || pendingBankIndex >= (int)mode.banks.size()) {
    return;
  }
  int count = (int)mode.banks[pendingBankIndex].patches.size();
  if (count == 0) {
    return;
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  pendingPatchIndex = navWrap(pendingPatchIndex + step, count);
}

void pendingCycleMidiChannel(CycleDirection direction) {
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  int channel = pendingMidiChannel + step;
  if (channel > 16) channel = 1;
  if (channel < 1) channel = 16;
  pendingMidiChannel = channel;
}

// Config is stepped through the SD card's sorted file list. listConfigFiles()
// re-scans the card on each call - fine here because this only runs on a
// button press, never from the ~20Hz screen flash timer (which reads the
// cached navPeek* names instead).
void pendingCycleConfig(CycleDirection direction) {
  std::vector<String> files = listConfigFiles();
  int count = (int)files.size();
  if (count == 0) {
    return;
  }
  int index = 0;
  for (int i = 0; i < count; i++) {
    if (files[i] == pendingConfigFilename) {
      index = i;
      break;
    }
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  index = navWrap(index + step, count);
  pendingConfigFilename = files[index];
  navSyncPendingTreeToConfig();
}

// steps the pending value of whichever parameter the selector currently
// points at (currentParameterIndex into getCurrentParameters()) - the
// NAVIGATE counterpart to Runtime.h's cycleCurrentParameterValue(), used by
// CycleParameterValueAction. Config / Mode / Bank / Patch / MidiChannel each
// route to their pendingCycle* helper above.
void pendingCycleCurrentParameterValue(CycleDirection direction) {
  std::vector<Parameter> &parameters = getCurrentParameters();
  if (parameters.empty() || currentParameterIndex < 0
      || currentParameterIndex >= (int)parameters.size()) {
    return;
  }
  Parameter parameter = parameters[currentParameterIndex];
  if (parameter == Parameter::PatchParameter)            pendingCyclePatch(direction);
  else if (parameter == Parameter::BankParameter)        pendingCycleBank(direction);
  else if (parameter == Parameter::ModeParameter)        pendingCycleMode(direction);
  else if (parameter == Parameter::ConfigParameter)      pendingCycleConfig(direction);
  else if (parameter == Parameter::MIDIChannelParameter) pendingCycleMidiChannel(direction);
  // Page has no "pending" form - a page change tears nothing down, so it is
  // always immediate and is handled by navStepCurrentParameterValueResolved()
  // (and applyCurrentParameterValue()) before it ever reaches here. See
  // docs/navigation.md.
}

// ============================================================================
// pending cursor movement - by name
//
// The pending equivalents of the direct Set* actions, for the (unusual) case
// of a navigate-style action that jumps the pending cursor to a named
// target. The real Set* actions stay immediate and are not routed here.
// Each returns false (and logs) if the name doesn't resolve, leaving the
// pending cursor untouched.

bool pendingSetMode(const String &name) {
  int index = findModeIndexByName(name);
  if (index < 0) {
    print("No mode named: " + name, LogLevel::WARNING);
    return false;
  }
  pendingModeIndex = index;
  navResetBankPatchForPendingMode();
  return true;
}

bool pendingSetBank(const String &name) {
  if (config.modes.empty()) {
    return false;
  }
  Mode &mode = navPendingMode();
  int index = findBankIndexByName(mode, name);
  if (index < 0) {
    print("No bank named: " + name, LogLevel::WARNING);
    return false;
  }
  pendingBankIndex = index;
  navSeedPendingPatchForBank();
  return true;
}

bool pendingSetPatch(const String &name) {
  if (config.modes.empty()) {
    return false;
  }
  Mode &mode = navPendingMode();
  if (pendingBankIndex < 0 || pendingBankIndex >= (int)mode.banks.size()) {
    print("No pending bank to set a patch within", LogLevel::WARNING);
    return false;
  }
  int index = findPatchIndexByName(&mode.banks[pendingBankIndex], name);
  if (index < 0) {
    print("No patch named: " + name, LogLevel::WARNING);
    return false;
  }
  pendingPatchIndex = index;
  return true;
}

bool pendingSetConfig(const String &filename) {
  std::vector<String> files = listConfigFiles();
  for (int i = 0; i < (int)files.size(); i++) {
    if (files[i] == filename) {
      pendingConfigFilename = filename;
      navSyncPendingTreeToConfig();
      return true;
    }
  }
  print("No config file named: " + filename, LogLevel::WARNING);
  return false;
}

void pendingSetMidiChannel(int channel) {
  if (channel < 1 || channel > 16) {
    print("Invalid MIDI channel: " + (String)channel, LogLevel::WARNING);
    return;
  }
  pendingMidiChannel = channel;
}

// ============================================================================
// activation - committing a selection
//
// applySelection() is the ONE place the live selection changes and the ONLY
// moment the outgoing selection's held notes / CCs stop. Both the
// navigate-then-activate path (activateSelection() below) and the direct
// Set* actions (rewired in a later rollout step) funnel through it. The step
// order is load-bearing - see docs/navigation.md.
//
// Nothing calls applySelection() yet except activateSelection(), which is
// only reached via ActivateSelectionAction; the button routing that makes
// that action and the performance buttons commit comes in a later step.

// clamps an index into [0, count-1]; returns 0 for an empty range (callers
// that care about "empty" guard it separately)
static int navClamp(int value, int count) {
  if (count <= 0) return 0;
  if (value < 0) return 0;
  if (value >= count) return count - 1;
  return value;
}

void applySelection(const String &configFile, int modeIndex, int bankIndex,
                    int patchIndex, int midiChannel) {
  SelectionPassScope inSelectionPass; // see selectionPassDepth in Runtime.h
  bool configChanged =
      configFile.length() > 0 && configFile != currentConfigFilename;

  Patch *outgoingPatch = getCurrentPatch();

  // MaintainActiveButtonOnPatchChange support (see ModeBank.h): if this is a
  // patch change within the same bank (mode/bank/config all unchanged, only
  // the patch differs) and the outgoing bank asks for it, remember which
  // button ids are on now, before the teardown below turns them off - they
  // get re-triggered against the incoming Patch further down, once it's
  // been seeded. This is what lets a held chord (ChordPlayer's bottom row)
  // keep sounding across a sibling patch change that only swaps its voicing
  // (the middle row), instead of the change cutting it like an ordinary
  // patch change does.
  bool sameBankPatchChange = !configChanged
      && modeIndex == currentModeIndex
      && bankIndex == currentBankIndex
      && patchIndex != currentPatchIndex;
  std::vector<int> maintainedButtonIds;
  if (sameBankPatchChange && outgoingPatch != nullptr) {
    ModeBank *outgoingBank = getCurrentBank();
    if (outgoingBank != nullptr && outgoingBank->MaintainActiveButtonOnPatchChange) {
      maintainedButtonIds = collectOnButtonIds(*outgoingPatch);
    }
  }

  // 1. stop the outgoing selection's held notes / CCs - fires OnRelease()
  //    for every currently-on button. A multi-page patch can hold buttons on
  //    on pages other than the current one, so it also needs a whole-patch
  //    sweep; turnOffAllActiveButtons() still runs too, for the Bank/Mode/
  //    Config-level buttons turnOffAllButtonsInPatch() does not walk. Both
  //    are part of this one teardown step, so every note-off still precedes
  //    every incoming note-on. See docs/navigation.md.
  if (outgoingPatch != nullptr) {
    turnOffAllButtonsInPatch(*outgoingPatch);
  }
  turnOffAllActiveButtons();

  if (configChanged) {
    // loadConfigFile() rebuilds Config and, via resetRuntimeSelection(),
    // lands on the new config's Mode 0 / Bank 0 / Patch 0, seeds
    // currentMidiChannel from the new config, and fires its default-on
    // buttons. A config change always arrives here targeting that same
    // starting point (Mode/Bank/Patch are navigated only AFTER the new
    // config is live - see docs/navigation.md), so there is nothing
    // more to position and the passed mode/bank/patch/channel are ignored.
    if (!loadConfigFile(configFile)) {
      // load failed, config unchanged - we already tore the held buttons
      // down, so put this config's defaults back rather than leave silence
      currentPage = 1;
      applyDefaultButtonStatuses();
    }
  } else {
    if (config.modes.empty()) {
      resetPendingToLive();
      display();
      return;
    }
    // 2. point the live selection at the target, clamped to the tree
    currentModeIndex = navClamp(modeIndex, (int)config.modes.size());
    Mode &mode = config.modes[currentModeIndex];
    if (mode.banks.size() > 0) {
      currentBankIndex = navClamp(bankIndex, (int)mode.banks.size());
      ModeBank &bank = mode.banks[currentBankIndex];
      currentPatchIndex =
          (bank.patches.size() > 0) ? navClamp(patchIndex, (int)bank.patches.size()) : -1;
    } else {
      currentBankIndex = -1;
      currentPatchIndex = -1;
    }
    if (midiChannel >= 1 && midiChannel <= 16) {
      currentMidiChannel = midiChannel;
    }
    // a fresh patch always opens on page 1; if it has several pages, put the
    // selector on Page so the step buttons flip pages straight away (see
    // docs/navigation.md)
    currentPage = 1;
    autoSelectPageParameterIfMultiPage();
    // 3. fire the new selection's default-on buttons (every page of a
    //    multi-page patch, not just page 1). A same-bank patch change only
    //    re-seeds the incoming Patch's OWN buttons (patchLevelOnly=true) -
    //    the Bank/Mode/Config levels were already seeded when THIS bank was
    //    itself freshly entered, and re-checking them on every sibling patch
    //    change would re-fire a Bank-level defaultStatus:On button the
    //    instant something (the Exclusive-set cancel above, or this very
    //    teardown) leaves it momentarily off - dragging the selection back
    //    to that button's own target instead of landing where the player
    //    actually pressed. See applyDefaultButtonStatuses()'s own comment in
    //    Runtime.h and docs/navigation.md for the full incident.
    applyDefaultButtonStatuses(/* patchLevelOnly = */ sameBankPatchChange);

    // 3b. re-trigger any buttons carried over by MaintainActiveButtonOnPatchChange
    //     (collected above, before the teardown turned them off) against the
    //     incoming Patch, honouring its own button definitions - so a held
    //     chord picks up the new Patch's voicing rather than the old one.
    //     Skips a button already on (e.g. one of the new Patch's own
    //     defaultStatus-On buttons happens to share the same id).
    for (int buttonId : maintainedButtonIds) {
      Button *button = resolveButton(buttonId);
      if (button != nullptr && !button->isOn) {
        turnButtonOn(*button);
      }
    }
  }

  // 4. pending == live again; redraw
  resetPendingToLive();
  display();
}

// ============================================================================
// page navigation - always immediate
//
// The pending-cursor / "Set?" model (sections 2-4) exists only to protect
// held sound while the player browses Patch / Bank / Mode / Config
// candidates. A page change within a patch tears nothing down - every page's
// buttons were seeded together on entry and share one teardown when the
// patch is left - so there is no held sound to protect and nothing to
// confirm. Page navigation therefore applies straight away, on the press,
// with no flash and no "Set?". See docs/navigation.md.

// steps the LIVE page one place within the current Patch, wrapping in
// [1, patchPageCount]. A single-page patch is a silent no-op, matching the
// pendingCycle* helpers.
void applyPageStep(CycleDirection direction) {
  Patch *patch = getCurrentPatch();
  if (patch == nullptr) {
    return;
  }
  int count = patchPageCount(patch);
  if (count <= 1) {
    return;
  }
  int step = (direction == CycleDirection::CycleUp) ? 1 : -1;
  // currentPage is 1-based; wrap the 0-based (page-1) value and add 1 back
  currentPage = navWrap(currentPage - 1 + step, count) + 1;
  display();
}

// jumps the LIVE page straight to `page` (1-based) within the current Patch.
// Rejected (with a warning, page unchanged) if there is no current patch or
// the number is out of range.
void navSetPage(int page) {
  Patch *patch = getCurrentPatch();
  if (patch == nullptr) {
    print("No current patch to set a page within", LogLevel::WARNING);
    return;
  }
  int count = patchPageCount(patch);
  if (page < 1 || page > count) {
    print("Cannot select page " + (String)page, LogLevel::WARNING);
    return;
  }
  currentPage = page;
  display();
}

void navCyclePage(CycleDirection direction) { applyPageStep(direction); }

// the value-stepping counterpart: if the parameter selector is on Page, step
// the page immediately; otherwise fall back to the deferred pending-cursor
// helper. Used by both CycleParameterValueAction and the built-in 11/12
// buttons.
void navStepCurrentParameterValueResolved(CycleDirection direction) {
  std::vector<Parameter> &parameters = getCurrentParameters();
  if (!parameters.empty()
      && currentParameterIndex >= 0
      && currentParameterIndex < (int)parameters.size()
      && parameters[currentParameterIndex] == Parameter::PageParameter) {
    applyPageStep(direction);
    return;
  }
  pendingCycleCurrentParameterValue(direction);
}

// Commits the pending cursor (a no-op when nothing is pending). Button 10's
// "Set?" and a performance-button press will both route here in a later
// step.
void activateSelection() {
  if (!navigationDirty()) {
    return;
  }

  // a channel-only change doesn't tear the patch down - nothing is
  // stranded - so apply it directly, the same as an immediate
  // SetMidiChannel would (see docs/navigation.md).
  if (navigationChannelDirty() && !navigationPatchDirty()) {
    currentMidiChannel = pendingMidiChannel;
    resetPendingToLive();
    display();
    return;
  }

  applySelection(pendingConfigFilename, pendingModeIndex, pendingBankIndex,
                 pendingPatchIndex, pendingMidiChannel);
}

// immediately applies a new value to whichever parameter the selector points
// at, by name (Patch / Bank / Mode / Config) or number (MidiChannel) - the
// SET counterpart to pendingCycleCurrentParameterValue(), used by
// SetParameterValueAction. Unlike the pending helpers this takes effect at
// once, with a full teardown for a hierarchy change (a channel change
// strands nothing, so it just assigns).
void applyCurrentParameterValue(const String &value) {
  std::vector<Parameter> &parameters = getCurrentParameters();
  if (parameters.empty() || currentParameterIndex < 0
      || currentParameterIndex >= (int)parameters.size()) {
    print("No parameter is currently selected to set", LogLevel::WARNING);
    return;
  }
  Parameter parameter = parameters[currentParameterIndex];

  if (parameter == Parameter::PatchParameter) {
    int index = findPatchIndexByName(getCurrentBank(), value);
    if (index < 0) {
      print("No patch named: " + value, LogLevel::WARNING);
      return;
    }
    applySelection(currentConfigFilename, currentModeIndex, currentBankIndex,
                   index, currentMidiChannel);

  } else if (parameter == Parameter::BankParameter) {
    Mode &mode = getCurrentMode();
    int index = findBankIndexByName(mode, value);
    if (index < 0) {
      print("No bank named: " + value, LogLevel::WARNING);
      return;
    }
    applySelection(currentConfigFilename, currentModeIndex, index,
                   mode.banks[index].resolveDefaultPatchIndex(),
                   currentMidiChannel);

  } else if (parameter == Parameter::ModeParameter) {
    int index = findModeIndexByName(value);
    if (index < 0) {
      print("No mode named: " + value, LogLevel::WARNING);
      return;
    }
    Mode &mode = config.modes[index];
    int patchIndex = (mode.banks.size() > 0)
        ? mode.banks[0].resolveDefaultPatchIndex() : 0;
    applySelection(currentConfigFilename, index, 0, patchIndex,
                   currentMidiChannel);

  } else if (parameter == Parameter::MIDIChannelParameter) {
    int channel = value.toInt();
    if (channel < 1 || channel > 16) {
      print("Invalid MIDI channel: " + value, LogLevel::WARNING);
      return;
    }
    currentMidiChannel = channel;
    resetPendingToLive();
    display();

  } else if (parameter == Parameter::ConfigParameter) {
    if (value.length() == 0) {
      print("No config filename to set", LogLevel::WARNING);
      return;
    }
    applySelection(value, 0, 0, 0, currentMidiChannel);

  } else if (parameter == Parameter::PageParameter) {
    // Page takes a 1-based number and applies immediately - no teardown (see
    // docs/navigation.md). navSetPage() range-checks it.
    navSetPage(value.toInt());
  }
}

// ============================================================================
// action entry points
//
// The thin functions the navigation Action classes' OnPress() call. They
// live here, not in the action headers, because they need the pending
// cursor and the live-selection globals - neither of which the action
// headers (compiled far earlier in the build) can see. Forward-declared in
// MIDI-Toolbox.ino and the test harness, exactly as cycleMode() etc. were.
//
//   Cycle* verbs  -> move the pending cursor (flash, wait for activation)
//   Set*   verbs  -> applySelection() now, with a clean teardown
//   Page verbs    -> applied immediately (a page change tears nothing down);
//                    see the "page navigation" section above

void navCyclePatch(CycleDirection direction)  { pendingCyclePatch(direction); }
void navCycleBank(CycleDirection direction)   { pendingCycleBank(direction); }
void navCycleMode(CycleDirection direction)   { pendingCycleMode(direction); }
void navCycleConfig(CycleDirection direction) { pendingCycleConfig(direction); }
void navCycleParameterValue(CycleDirection direction) {
  navStepCurrentParameterValueResolved(direction);
}

// True when a Set* action should do nothing because it lands exactly where the
// live selection already is AND it is being fired from inside a selection pass
// (applySelection() / applyDefaultButtonStatuses() - selectionPassDepth in
// Runtime.h). Outside a pass, landing on the live selection is a deliberate
// RESET and goes ahead: the same teardown and re-seed as a change (see
// navSetPatch()). Inside one it is a defaultStatus:"On" button whose own action
// selects the very selection being seeded, which would tear it down and
// re-seed it again - firing that same button, without end. The whole landing
// (mode, bank AND patch) is compared.
bool navSetLandsOnLiveSelection(int modeIndex, int bankIndex, int patchIndex) {
  return selectionPassDepth > 0
      && modeIndex == currentModeIndex
      && bankIndex == currentBankIndex
      && patchIndex == currentPatchIndex;
}

// SetPatch. Targeting the patch that is already live RESETS it (see the guard
// below), except while a selection pass is already running. With only
// patchIndex, the patch is in the CURRENT bank. Adding
// bankIndex picks a patch in another bank of the current mode, and adding
// modeIndex as well picks one in another mode ("mode.bank.patch"). A bare
// modeIndex without a bankIndex is ignored. The patch lands exactly as named -
// bankIndex/modeIndex only say WHERE it is, never which patch.
void navSetPatch(int patchIndex, int bankIndex, int modeIndex) {
  bool hasBank = bankIndex >= 0;
  int targetMode = (hasBank && modeIndex >= 0) ? modeIndex : currentModeIndex;
  int targetBank = hasBank ? bankIndex : currentBankIndex;

  if (targetMode < 0 || targetMode >= (int)config.modes.size()) {
    print("Cannot select mode " + (String)targetMode, LogLevel::WARNING);
    return;
  }
  Mode &mode = config.modes[targetMode];
  if (targetBank < 0 || targetBank >= (int)mode.banks.size()) {
    print("Cannot select bank " + (String)targetBank, LogLevel::WARNING);
    return;
  }
  ModeBank &bank = mode.banks[targetBank];
  if (patchIndex < 0 || patchIndex >= (int)bank.patches.size()) {
    print("Cannot select patch " + (String)patchIndex, LogLevel::WARNING);
    return;
  }
  // The patch we are already on: when a player presses it, that is a RESET of
  // the current patch - the same teardown and re-seed as any patch change, so
  // every held note is released, the buttons go back to their defaults, the
  // page returns to 1 and any pending navigation is discarded. Not from inside
  // a selection pass, though - see navSetLandsOnLiveSelection().
  if (navSetLandsOnLiveSelection(targetMode, targetBank, patchIndex)) {
    return;
  }
  applySelection(currentConfigFilename, targetMode, targetBank,
                 patchIndex, currentMidiChannel);
}

// SetBank. With only bankIndex, the bank is in the CURRENT mode; adding
// modeIndex picks a bank in another mode ("mode.bank"). Either way it lands on
// that bank's default patch - so choosing the bank you are already in is never
// a no-op when pressed: from any other patch of the bank it takes you "home" to
// the default patch, and from the default patch itself it resets it (see
// navSetPatch() and navSetLandsOnLiveSelection()).
void navSetBank(int bankIndex, int modeIndex) {
  int targetMode = (modeIndex >= 0) ? modeIndex : currentModeIndex;
  if (targetMode < 0 || targetMode >= (int)config.modes.size()) {
    print("Cannot select mode " + (String)targetMode, LogLevel::WARNING);
    return;
  }
  Mode &mode = config.modes[targetMode];
  if (bankIndex < 0 || bankIndex >= (int)mode.banks.size()) {
    print("Cannot select bank " + (String)bankIndex, LogLevel::WARNING);
    return;
  }
  int landingPatch = mode.banks[bankIndex].resolveDefaultPatchIndex();
  if (navSetLandsOnLiveSelection(targetMode, bankIndex, landingPatch)) {
    return;
  }
  applySelection(currentConfigFilename, targetMode, bankIndex, landingPatch,
                 currentMidiChannel);
}

// SetMode. Lands on the mode's first bank and that bank's default patch, so -
// like SetBank - choosing the mode you are already in takes you back to that
// starting point, or resets it if you are already there.
void navSetMode(int modeIndex) {
  if (modeIndex < 0 || modeIndex >= (int)config.modes.size()) {
    print("Cannot select mode " + (String)modeIndex, LogLevel::WARNING);
    return;
  }
  Mode &mode = config.modes[modeIndex];
  // a mode with no banks has no landing bank or patch at all (-1, -1), which is
  // also what applySelection() leaves the live selection at for one
  int landingBank = mode.banks.empty() ? -1 : 0;
  int landingPatch = mode.banks.empty() ? -1 : mode.banks[0].resolveDefaultPatchIndex();
  if (navSetLandsOnLiveSelection(modeIndex, landingBank, landingPatch)) {
    return;
  }
  applySelection(currentConfigFilename, modeIndex, (landingBank < 0) ? 0 : landingBank,
                 (landingPatch < 0) ? 0 : landingPatch, currentMidiChannel);
}

void navSetConfig(String filename) {
  if (filename.length() == 0) {
    print("SetConfig action has no filename configured", LogLevel::WARNING);
    return;
  }
  applySelection(filename, 0, 0, 0, currentMidiChannel);
}

void navSetParameterValue(String value) {
  applyCurrentParameterValue(value);
}

// ============================================================================
// button input routing
//
// The shared logic behind a footswitch press (MIDI-Toolbox.ino's loop()) and
// a tap on the matching on-screen button (Screen.h). See
// docs/navigation.md.

// diagnostic: logs one button interaction to the serial monitor at DEBUG
// level - which id, what happened ("pressed" / "released" / "tapped" /
// "long-pressed"), and where it came from ("physical" / "on-screen").
void logButton(int id, const char *action, const char *source) {
  print("Button " + (String)id + " " + action + " (" + source + ")", LogLevel::DEBUG);
}

// how long (ms) button 10 must be held for a "long press" (the pending-change
// Revert gesture) rather than a tap. Measured on the footswitch and evaluated
// on release - see resolveButton10() and the .ino's buttonA handling.
#define NAV_LONG_PRESS_MS 500

// true iff EVERY one of the Button's actions is a plain performance action
// (PlayNotes / ProgramChange / ControlChange) - i.e. it touches nothing to do
// with the hierarchy, the pending cursor or the parameter selector. Only such
// a button pre-commits a pending navigation when pressed while a change is
// pending (performanceButtonPress()); any button that itself navigates - a
// `Set*` jump, a `Cycle*` step, or the selector - is left to resolve the
// pending state through its own actions (the jump supersedes it, a step
// extends it). An actionless button is not "pure performance".
bool buttonIsPurePerformance(Button &button) {
  if (button.actions.empty()) {
    return false;
  }
  for (std::unique_ptr<IAction> &action : button.actions) {
    ActionType type = action->getType();
    if (type != ActionType::PlayNotes
        && type != ActionType::ProgramChange
        && type != ActionType::ControlChange) {
      return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
// button-name overlay ("toast")
//
// Pressing a button that SENDS MIDI (any PlayNotes / ProgramChange /
// ControlChange action) shows the button's name over the device screen so the
// player can see what they just pressed. A Latched or Exclusive button shows
// "Name ON" / "Name OFF" for the state it has just moved to. Buttons that only
// navigate (Set* / Cycle*) show nothing - the screen changing is the feedback.
//
// The overlay stays up TOAST_MIN_MS from the press; a Momentary button's
// overlay additionally stays up for as long as the button is held. Screen.h
// draws it (screenSyncToast()) and hides it while a navigation is pending - the
// flashing "Set?" preview always takes priority.
//
// Every function takes the current time explicitly rather than calling
// millis() itself, so the timing can be unit-tested with a fake clock.
#define TOAST_MIN_MS 500

bool toastActive = false;
String toastText = "";
// the overlay's background: the button's own colour, else its bank's
ColourCode toastColour = ColourCode(44, 44, 44);
// the id of the Momentary button holding the overlay up, or -1
int toastHoldId = -1;
// when the minimum display time ends (compared wrap-safely, see toastVisible)
uint32_t toastExpiresAt = 0;

// true iff at least one of the Button's actions sends MIDI
bool buttonSendsMidi(Button &button) {
  for (std::unique_ptr<IAction> &action : button.actions) {
    ActionType type = action->getType();
    if (type == ActionType::PlayNotes
        || type == ActionType::ProgramChange
        || type == ActionType::ControlChange) {
      return true;
    }
  }
  return false;
}

// the overlay text for a Button that has just been pressed - call AFTER the
// press has run, so isOn reflects the state the button has moved to
String toastTextFor(Button &button) {
  if (button.buttonMode == ButtonMode::Latched || button.buttonMode == ButtonMode::Exclusive) {
    return button.name + (button.isOn ? " ON" : " OFF");
  }
  return button.name;
}

void toastShow(const String &text, const ColourCode &colour, int holdId, uint32_t nowMs) {
  toastActive = true;
  toastText = text;
  toastColour = colour;
  toastHoldId = holdId;
  toastExpiresAt = nowMs + TOAST_MIN_MS;
}

void toastHide() {
  toastActive = false;
  toastHoldId = -1;
}

// a button was released - a Momentary button's overlay no longer stays up on
// its account (it then ends at its minimum time, or straight away if that has
// already passed)
void toastOnRelease(int id) {
  if (toastActive && toastHoldId == id) {
    toastHoldId = -1;
  }
}

bool toastVisible(uint32_t nowMs) {
  if (!toastActive) {
    return false;
  }
  if (toastHoldId >= 0) {
    return true;
  }
  // signed difference so a millis() wrap-around doesn't matter
  return (int32_t)(toastExpiresAt - nowMs) > 0;
}

// A press of buttons 0-9 (and 10-12 when the config binds a Button to them):
// logs it, then - if a navigation is pending and this is a pure performance
// button - commits the pending change first, then runs the button's own
// actions. A performance button therefore confirms a pending navigation as a
// side effect; a button that does its own SetPatch/CycleMode/... resolves the
// pending state itself. After any such press, pending == live (unless the
// button's own action re-dirtied it, e.g. a Cycle* step). A button that sends
// MIDI also raises the button-name overlay (see above).
void performanceButtonPress(int id, const char *source) {
  logButton(id, "pressed", source);
  Button *button = resolveButton(id);
  if (button == nullptr) {
    return;
  }
  if (navigationDirty() && buttonIsPurePerformance(*button)) {
    activateSelection();
  }
  bool sendsMidi = buttonSendsMidi(*button);
  triggerButton(id);
  if (sendsMidi && button->name.length() > 0) {
    ModeBank *bank = getCurrentBank();
    ColourCode colour = button->hasColourCode ? button->colourCode
                        : (bank != nullptr) ? bank->subheaderColourCode
                                            : ColourCode(44, 44, 44);
    toastShow(toastTextFor(*button), colour,
              button->buttonMode == ButtonMode::Momentary ? id : -1, millis());
  }
}

// the matching release - runs releaseButton() and lets a held Momentary
// button's overlay start to expire. Use this rather than releaseButton()
// directly for any footswitch or on-screen release.
void performanceButtonRelease(int id) {
  releaseButton(id);
  toastOnRelease(id);
}

// what a completed press of the middle parameter button (id 10) does, in its
// built-in role. Pure - callers evaluate it and act. heldMs is how long the
// switch was held.
//   not dirty                      -> advance the parameter selector
//   dirty, quick tap               -> activate the pending change ("Set?")
//   dirty, held >= NAV_LONG_PRESS_MS -> revert the pending change to live
//
// A config may now override id 10 with its own Button (see
// buttonHasExplicitOverride() / builtInNavButtons() in Runtime.h and the
// .ino's buttonA handling) - in that case the physical footswitch runs the
// override's own action(s) on press/release instead of this, and hold
// duration means nothing (there is no long-press concept in the Action
// model - see CycleActivateParameterAction.h). This function itself is unchanged:
// it's simply no longer called for the physical footswitch while overridden.
// The on-screen parameter buttons (Screen.h) are a separate code path that
// always calls this regardless of any override, which is what guarantees
// revert (and the rest of the built-in role) is always reachable somewhere.
enum Button10Result { Button10Advance, Button10Activate, Button10Revert };

Button10Result resolveButton10(uint32_t heldMs, bool dirty) {
  if (!dirty) {
    return Button10Advance;
  }
  return (heldMs < NAV_LONG_PRESS_MS) ? Button10Activate : Button10Revert;
}

// runs the resolved action for a completed press of button 10's built-in
// parameter-selector role
void navButton10Completed(uint32_t heldMs) {
  switch (resolveButton10(heldMs, navigationDirty())) {
    case Button10Advance:
      cycleCurrentParameterSelection(CycleDirection::CycleUp);
      break;
    case Button10Activate:
      activateSelection();
      break;
    case Button10Revert:
      resetPendingToLive();
      display();
      break;
  }
}

// buttons 11 / 12's built-in role: step the value of whichever parameter the
// selector points at, down / up. Normally that means the pending value (flash,
// wait for "Set?"); when the selector is on Page it applies immediately - a
// page change tears nothing down (see navStepCurrentParameterValueResolved()
// and docs/navigation.md). A no-op when that level has
// nothing to step. Redraws so the flashing preview updates at once rather
// than on the next ~50ms flash-timer tick.
void navStepValue(CycleDirection direction) {
  navStepCurrentParameterValueResolved(direction);
  display();
}

// ============================================================================
// readouts for Screen.h
//
// The parameter row (middle-button label + arrows) and the flashing Header /
// SubHeader previews. Screen.h's screenSyncParamEdit() / screenUpdateParam-
// Arrows() call these. A label flashes iff its level's navigation*Dirty()
// is true; when navigationDirty() is false everything is solid.
//
// A Config-level difference cascades: navigation{Mode,Bank,Patch}Dirty() all
// report true when only the config differs, so all four Header/SubHeader
// labels flash - showing the candidate file's peeked first Mode/Bank/Patch.

// the parameter the selector currently points at (Patch / Bank / Mode /
// MidiChannel / Config)
static Parameter navSelectedParameter() {
  std::vector<Parameter> &parameters = getCurrentParameters();
  if (parameters.empty()) {
    return Parameter::PatchParameter;
  }
  int index = currentParameterIndex;
  if (index < 0 || index >= (int)parameters.size()) {
    index = 0;
  }
  return parameters[index];
}

// how many distinct values the selected parameter can take - Screen.h greys
// and disables the step arrows when this is 1 or less. Safe before a config
// has loaded.
int navigationValueCount() {
  if (config.modes.empty()) {
    return 0;
  }
  Parameter parameter = navSelectedParameter();
  if (parameter == Parameter::PatchParameter) {
    ModeBank *bank = getCurrentBank();
    return (bank != nullptr) ? (int)bank->patches.size() : 0;
  }
  if (parameter == Parameter::BankParameter) {
    return (int)getCurrentMode().banks.size();
  }
  if (parameter == Parameter::ModeParameter) {
    return (int)config.modes.size();
  }
  if (parameter == Parameter::MIDIChannelParameter) {
    return 16;
  }
  if (parameter == Parameter::ConfigParameter) {
    return (int)listConfigFiles().size();
  }
  if (parameter == Parameter::PageParameter) {
    // 1 for an ordinary single-page patch - the arrows grey out, exactly as
    // Patch does in a one-patch bank
    return patchPageCount(getCurrentPatch());
  }
  return 0;
}

// the middle button's label: "Set?" while a change is pending, otherwise the
// selected parameter's name
String navigationButton10Label() {
  if (config.modes.empty()) {
    return "-";
  }
  if (navigationDirty()) {
    return "Set?";
  }
  return getParameterString(navSelectedParameter());
}

// the config filename (with ".json" still on it - Screen.h strips it) the
// Config label should show: the pending candidate, which equals the running
// config's name when nothing is pending
String navigationPreviewConfigName() {
  return (pendingConfigFilename.length() > 0) ? pendingConfigFilename
                                              : currentConfigFilename;
}

// the Mode name the (possibly flashing) Header/right label should show
String navigationPreviewModeName() {
  if (navigationConfigDirty()) {
    return navPeekMode;
  }
  if (pendingModeIndex >= 0 && pendingModeIndex < (int)config.modes.size()) {
    return config.modes[pendingModeIndex].name;
  }
  return getCurrentMode().name;
}

// the Bank name the SubHeader/left label should show
String navigationPreviewBankName() {
  if (navigationConfigDirty()) {
    return navPeekBank;
  }
  if (pendingModeIndex >= 0 && pendingModeIndex < (int)config.modes.size()) {
    Mode &mode = config.modes[pendingModeIndex];
    if (pendingBankIndex >= 0 && pendingBankIndex < (int)mode.banks.size()) {
      return mode.banks[pendingBankIndex].name;
    }
  }
  return "-";
}

// the Patch being previewed (the pending candidate - the live patch while
// nothing is pending), or nullptr when nothing is selected or the Config level
// is dirty (only the names of a not-yet-loaded config are known)
Patch * navigationPreviewPatch() {
  if (navigationConfigDirty()) {
    return nullptr;
  }
  if (pendingModeIndex >= 0 && pendingModeIndex < (int)config.modes.size()) {
    Mode &mode = config.modes[pendingModeIndex];
    if (pendingBankIndex >= 0 && pendingBankIndex < (int)mode.banks.size()) {
      ModeBank &bank = mode.banks[pendingBankIndex];
      if (pendingPatchIndex >= 0 && pendingPatchIndex < (int)bank.patches.size()) {
        return &bank.patches[pendingPatchIndex];
      }
    }
  }
  return nullptr;
}

// the Patch name being previewed, WITHOUT any page suffix: the pending
// candidate's name (which equals the live patch's while nothing is pending),
// the peeked name while the Config level is dirty, or "-" when nothing is
// selected
String navigationPreviewPatchBaseName() {
  if (navigationConfigDirty()) {
    return navPeekPatch;
  }
  Patch *patch = navigationPreviewPatch();
  return (patch != nullptr) ? patch->name : String("-");
}

// the Patch name the SubHeader/right label should show. When the LIVE patch
// has more than one page, a " / N" page suffix is appended ("My Patch / 2"),
// so the player can see which page is up. The suffix is dropped while the
// Patch level is dirty - the label is then previewing a DIFFERENT candidate
// patch (whose page count isn't meaningful yet, and which always opens on
// page 1 once activated). See docs/navigation.md.
// (The Live screen layout shows the page as its own large readout instead and
// uses navigationPreviewPatchBaseName().)
String navigationPreviewPatchName() {
  String name = navigationPreviewPatchBaseName();
  if (navigationConfigDirty() || navigationPatchDirty() || name == "-") {
    return name;
  }
  Patch *live = getCurrentPatch();
  if (live != nullptr && patchPageCount(live) > 1) {
    name += " / " + String(currentPage);
  }
  return name;
}

// the MIDI channel the "Ch. N" readout should show
int navigationPreviewChannel() {
  return pendingMidiChannel;
}

// matches #ifndef/#define above
#endif
