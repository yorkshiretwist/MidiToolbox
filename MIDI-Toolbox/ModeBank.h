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
//    Exceptions were thrown when I tried to use the class name 'Bank'        //
//    I assume this is a reserved word                                        //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation. Note the
// guard is named "Bank_h", not "ModeBank_h" - it just needs to be unique
// across the whole project, it doesn't have to match the filename.
#ifndef Bank_h
#define Bank_h

#include <string>
#include <vector>
#include <ArduinoJson.h>

// pulls in the types used by this file: the Parameter enum, the Patch
// class, and the Button class
#include "Enums.h"
#include "ColourCode.h"
#include "Patch.h"
#include "Button.h"

// represents a ModeBank
class ModeBank {
  public: 

    // the name of the ModeBank, shown on the screen
    String name;

    // Patches in this ModeBank
    std::vector<Patch> patches;

    // buttons which apply to all Patches in this ModeBank
    std::vector<Button> buttons;

    // the colour of the subheader bar on the screen while this ModeBank is
    // selected - defaults to [44,44,44] per the schema if the config file
    // doesn't set one
    ColourCode subheaderColourCode = ColourCode(44, 44, 44);

    // How this bank governs the relationship between its Patches' buttons -
    // see the BankType enum in Enums.h. BankTypeDefault (the default) is the
    // ordinary "reset on every patch change" behaviour.
    // BankTypePersistentButtons is still parsed from the "bankType" JSON key
    // but currently has NO effect - the "carry button state between sibling
    // containers" idea now lives at the Patch level as pages (see Button.page
    // and docs/navigation.md). The field is kept so a
    // future bank model can use it.
    BankType bankType = BankType::BankTypeDefault;

    // how the screen is laid out while this ModeBank is selected - see the
    // ScreenLayout enum in Enums.h. Grid (the default) is the ordinary
    // button-grid screen; Live is the large-type performance view.
    ScreenLayout screenLayout = ScreenLayout::ScreenLayoutGrid;

    // the system parameters cyclable by button 10, if null use the parameters property on the Mode
    std::vector<Parameter> parameters;

    // When true, a Button that's "on" in the outgoing Patch survives a patch
    // change within this same bank: applySelection() (Navigation.h) still
    // turns it off first (firing its normal OnRelease actions, e.g. note-
    // offs) and tears the outgoing patch down as usual, but once the
    // incoming Patch has been seeded it re-triggers the same button id
    // there (firing that Patch's OnPress actions for it), rather than
    // leaving it off. This is what lets a held chord (ChordPlayer's bottom
    // row of buttons) keep sounding while a sibling patch change (the
    // middle row) swaps in a different voicing, instead of cutting the
    // chord every time the voicing changes. Defaults to false, matching the
    // ordinary "reset on every patch change" behaviour.
    bool MaintainActiveButtonOnPatchChange = false;

    // The Patch that becomes active whenever this ModeBank is freshly
    // selected - by a SetBank/CycleBank action, by arriving at it via a
    // Mode or Config change, or as the initial selection at boot. Defaults
    // to 0 (the first Patch). Read through resolveDefaultPatchIndex()
    // below, which clamps it into range, so a config that sets this past
    // the end of "patches" still lands somewhere valid rather than crashing.
    int defaultPatchIndex = 0;

    // The MIDI channel actions in this ModeBank (or its Patches) should use
    // when they don't configure their own channel. -1 (the default) means
    // "not set" - see resolveMidiChannel() in Runtime.h for the full
    // precedence order: an action's own channel wins first, then the
    // current Patch's midiChannel (if set), then this, then whichever
    // channel is globally selected.
    int midiChannel = -1;

    // See Patch.h for a full explanation of this copy/move-forbidding
    // pattern, needed because ModeBank contains Patches and Buttons, which
    // in turn own std::unique_ptr objects that can't be copied.
    //
    // constructs a new ModeBank object
    // ModeBank holds vectors of Patch and Button, both move-only (see
    // Button.h), so ModeBank is move-only too.
    ModeBank() = default;
    ModeBank(const ModeBank&) = delete;
    ModeBank& operator = (const ModeBank&) = delete;
    ModeBank(ModeBank&&) = default;
    ModeBank& operator = (ModeBank&&) = default;

    // constructs a new ModeBank object from the given JsonVariant object
    ModeBank(JsonVariant json, JsonVariant presets = JsonVariant()) {
      print("", LogLevel::INFO);

      // set the name
      print("  Getting ModeBank.name... ", LogLevel::INFO, false);
      name = json["name"].as<String>();
      print(name, LogLevel::INFO);

      // set the MIDI channel, if this ModeBank configures one
      if (!json["midiChannel"].isNull()) {
        midiChannel = json["midiChannel"].as<int>();
      }

      // set the subheader colour code, if this ModeBank configures one
      if (!json["subheaderColourCode"].isNull()) {
        subheaderColourCode = ColourCode(json["subheaderColourCode"].as<JsonArray>());
      }

      // set the screen layout, if this ModeBank configures one - an absent or
      // unrecognised "screenLayout" falls through to Grid
      if (!json["screenLayout"].isNull()) {
        screenLayout = getScreenLayoutEnum(json["screenLayout"].as<JsonVariant>());
        print("  ModeBank.screenLayout: " + String(std::string(getScreenLayoutString(screenLayout)).c_str()), LogLevel::INFO);
      }

      // set the bank type, if this ModeBank configures one - an absent or
      // unrecognised "bankType" falls through getBankTypeEnum() to Default
      if (!json["bankType"].isNull()) {
        bankType = getBankTypeEnum(json["bankType"].as<JsonVariant>());
        print("  ModeBank.bankType: " + String(std::string(getBankTypeString(bankType)).c_str()), LogLevel::INFO);
      }

      // set MaintainActiveButtonOnPatchChange, if this ModeBank configures it
      if (!json["maintainActiveButtonOnPatchChange"].isNull()) {
        MaintainActiveButtonOnPatchChange = json["maintainActiveButtonOnPatchChange"].as<bool>();
        print("  ModeBank.MaintainActiveButtonOnPatchChange: " + String(MaintainActiveButtonOnPatchChange), LogLevel::INFO);
      }

      // set defaultPatchIndex, if this ModeBank configures one
      if (!json["defaultPatchIndex"].isNull()) {
        defaultPatchIndex = json["defaultPatchIndex"].as<int>();
        print("  ModeBank.defaultPatchIndex: " + (String)defaultPatchIndex, LogLevel::INFO);
      }

      // get the Patches for this ModeBank
      if (!json["patches"].isNull()) {
        print("  Getting ModeBank.patches...", LogLevel::DEBUG);
        JsonArray patchesArray = json["patches"].as<JsonArray>();
        for (JsonVariant patchItem : patchesArray) {
          patches.push_back(Patch(patchItem, presets));
        }
      }

      // get the parameters applicable for this ModeBank - if absent, the
      // Mode's own parameters are used instead (see getCurrentParameters()
      // in the main .ino file)
      if (!json["parameters"].isNull()) {
        print("  Getting ModeBank.parameters...", LogLevel::DEBUG);
        JsonArray parametersArray = json["parameters"].as<JsonArray>();
        for (JsonVariant parameterItem : parametersArray) {
          parameters.push_back(getParameterEnum(parameterItem));
        }
      }

      // get the buttons for this ModeBank
      if (!json["buttons"].isNull()) {
        print("  Getting ModeBank.buttons...", LogLevel::DEBUG);
        JsonArray buttonsArray = json["buttons"].as<JsonArray>();
        for (JsonVariant buttonItem : buttonsArray) {
          buttons.push_back(Button(buttonItem, presets));
        }
        print("  ModeBank.buttons.size(): " + (String)buttons.size(), LogLevel::INFO);
      }
    }

    // Resolves defaultPatchIndex into a valid index for this ModeBank's
    // Patches, clamping it into range so a config that sets it past the end
    // of "patches" (or leaves it negative) still lands somewhere valid
    // rather than crashing. Returns -1 if this ModeBank has no Patches.
    // This is the ONE place "which Patch does landing on this bank select"
    // is decided - Navigation.h and ConfigStore.h both call through it
    // rather than assuming Patch 0.
    int resolveDefaultPatchIndex() const {
      if (patches.empty()) {
        return -1;
      }
      if (defaultPatchIndex < 0) {
        return 0;
      }
      if (defaultPatchIndex >= (int)patches.size()) {
        return (int)patches.size() - 1;
      }
      return defaultPatchIndex;
    }
};

// matches #ifndef/#define above
#endif
