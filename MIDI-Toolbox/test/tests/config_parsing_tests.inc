//----------------------------------------------------------------------------//
//    Tests for the JSON-parsing constructors: Button, Patch, ModeBank,      //
//    Mode, and Config. These check that config.json-style data gets        //
//    turned into the right in-memory objects.                              //
//----------------------------------------------------------------------------//

class ConfigParsingTest : public ::testing::Test {
  protected:
    void SetUp() override {
      resetTestState();
    }
};

TEST_F(ConfigParsingTest, Button_ParsesIdNameModeAndButtonSet) {
  JsonDocument doc = parseJson(R"({
    "id": 3,
    "name": "Overdrive",
    "buttonMode": "Exclusive",
    "buttonSet": [2, 3, 4],
    "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_EQ(button.id, 3);
  EXPECT_EQ(button.name, "Overdrive");
  EXPECT_EQ(button.buttonMode, ButtonMode::Exclusive);
  ASSERT_EQ(button.buttonSet.size(), 3u);
  EXPECT_EQ(button.buttonSet[0], 2);
  EXPECT_EQ(button.buttonSet[1], 3);
  EXPECT_EQ(button.buttonSet[2], 4);
}

TEST_F(ConfigParsingTest, Button_WithNoButtonSet_LeavesItEmpty) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Solo", "buttonMode": "Momentary", "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_TRUE(button.buttonSet.empty());
}

TEST_F(ConfigParsingTest, Button_ParsesDefaultStatus) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Boost", "buttonMode": "Latched", "defaultStatus": "On", "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_EQ(button.defaultStatus, ButtonStatus::ButtonStatusOn);
}

TEST_F(ConfigParsingTest, Button_WithNoDefaultStatus_DefaultsToOff) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Solo", "buttonMode": "Momentary", "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_EQ(button.defaultStatus, ButtonStatus::ButtonStatusOff);
}

TEST_F(ConfigParsingTest, Button_ParsesColourCode) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "ProgramChange", "buttonMode": "Momentary",
    "colourCode": [24, 54, 250], "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_TRUE(button.hasColourCode);
  EXPECT_EQ(button.colourCode.red, 24);
  EXPECT_EQ(button.colourCode.green, 54);
  EXPECT_EQ(button.colourCode.blue, 250);
}

TEST_F(ConfigParsingTest, Button_WithNoColourCode_HasColourCodeFalse) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Solo", "buttonMode": "Momentary", "actions": []
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_FALSE(button.hasColourCode);
}

TEST_F(ConfigParsingTest, Button_MultipleActions_CreatesOneObjectPerAction) {
  // a single button press can trigger several actions at once - e.g. a MIDI
  // note AND a program change AND a control change together (see the
  // "buttonSet"/"actions" design discussed when this feature was built)
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Combo", "buttonMode": "Momentary",
    "actions": [
      { "type": "Note", "actionParameters": { "noteNames": ["C3"] } },
      { "type": "ProgramChange", "actionParameters": { "programNumber": 5 } },
      { "type": "ControlChange", "actionParameters": { "controllerNumber": 1, "controlValue": 100 } }
    ]
  })");

  Button button(doc.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 3u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::PlayNotes);
  EXPECT_EQ(button.actions[1]->getType(), ActionType::ProgramChange);
  EXPECT_EQ(button.actions[2]->getType(), ActionType::ControlChange);
}

TEST_F(ConfigParsingTest, Patch_ParsesNameAndButtons) {
  JsonDocument doc = parseJson(R"({
    "name": "Lead Tone",
    "buttons": [
      { "id": 0, "name": "A", "buttonMode": "Momentary", "actions": [] },
      { "id": 1, "name": "B", "buttonMode": "Momentary", "actions": [] }
    ]
  })");

  Patch patch(doc.as<JsonVariant>());

  EXPECT_EQ(patch.name, "Lead Tone");
  ASSERT_EQ(patch.buttons.size(), 2u);
  EXPECT_EQ(patch.buttons[0].id, 0);
  EXPECT_EQ(patch.buttons[1].id, 1);
}

TEST_F(ConfigParsingTest, Patch_WithNoButtons_LeavesButtonsEmpty) {
  JsonDocument doc = parseJson(R"({ "name": "Empty Patch" })");

  Patch patch(doc.as<JsonVariant>());

  EXPECT_TRUE(patch.buttons.empty());
}

TEST_F(ConfigParsingTest, Patch_WithMidiChannel_ParsesIt) {
  JsonDocument doc = parseJson(R"({ "name": "Lead Tone", "midiChannel": 7 })");

  Patch patch(doc.as<JsonVariant>());

  EXPECT_EQ(patch.midiChannel, 7);
}

TEST_F(ConfigParsingTest, Patch_WithNoMidiChannel_DefaultsToUnset) {
  JsonDocument doc = parseJson(R"({ "name": "Lead Tone" })");

  Patch patch(doc.as<JsonVariant>());

  EXPECT_EQ(patch.midiChannel, -1);
}

TEST_F(ConfigParsingTest, ModeBank_ParsesNamePatchesButtonsAndParameters) {
  JsonDocument doc = parseJson(R"({
    "name": "Song Bank",
    "parameters": ["Patch", "MIDIChannel"],
    "patches": [ { "name": "Verse", "buttons": [] } ],
    "buttons": [ { "id": 5, "name": "Tap", "buttonMode": "Momentary", "actions": [] } ]
  })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.name, "Song Bank");
  ASSERT_EQ(bank.parameters.size(), 2u);
  EXPECT_EQ(bank.parameters[0], Parameter::PatchParameter);
  EXPECT_EQ(bank.parameters[1], Parameter::MIDIChannelParameter);
  ASSERT_EQ(bank.patches.size(), 1u);
  EXPECT_EQ(bank.patches[0].name, "Verse");
  ASSERT_EQ(bank.buttons.size(), 1u);
  EXPECT_EQ(bank.buttons[0].id, 5);
}

TEST_F(ConfigParsingTest, ModeBank_WithMidiChannel_ParsesIt) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "midiChannel": 3 })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.midiChannel, 3);
}

TEST_F(ConfigParsingTest, ModeBank_WithNoMidiChannel_DefaultsToUnset) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.midiChannel, -1);
}

TEST_F(ConfigParsingTest, Mode_ParsesNameParametersButtonsAndBanks) {
  JsonDocument doc = parseJson(R"({
    "name": "Live",
    "parameters": ["Bank"],
    "buttons": [ { "id": 10, "name": "Menu", "buttonMode": "Momentary", "actions": [] } ],
    "banks": [ { "name": "Bank1", "patches": [] } ]
  })");

  Mode mode(doc.as<JsonVariant>());

  EXPECT_EQ(mode.name, "Live");
  ASSERT_EQ(mode.parameters.size(), 1u);
  EXPECT_EQ(mode.parameters[0], Parameter::BankParameter);
  ASSERT_EQ(mode.buttons.size(), 1u);
  ASSERT_EQ(mode.banks.size(), 1u);
  EXPECT_EQ(mode.banks[0].name, "Bank1");
}

TEST_F(ConfigParsingTest, Config_ParsesHeaderColourCodeAndModes) {
  JsonDocument doc = parseJson(R"({
    "headerColourCode": [10, 20, 30],
    "modes": [
      { "name": "ModeA", "banks": [] },
      { "name": "ModeB", "banks": [] }
    ]
  })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.headerColourCode.red, 10);
  EXPECT_EQ(parsedConfig.headerColourCode.green, 20);
  EXPECT_EQ(parsedConfig.headerColourCode.blue, 30);
  ASSERT_EQ(parsedConfig.modes.size(), 2u);
  EXPECT_EQ(parsedConfig.modes[0].name, "ModeA");
  EXPECT_EQ(parsedConfig.modes[1].name, "ModeB");
}

TEST_F(ConfigParsingTest, Config_WithNoHeaderColourCode_DefaultsPerSchema) {
  JsonDocument doc = parseJson(R"({ "modes": [] })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.headerColourCode.red, 22);
  EXPECT_EQ(parsedConfig.headerColourCode.green, 22);
  EXPECT_EQ(parsedConfig.headerColourCode.blue, 22);
}

TEST_F(ConfigParsingTest, Config_LegacyDefaultColourCodeKey_IsStillAccepted) {
  // "defaultColourCode" was this property's name before the schema update -
  // still read as a fallback so old config files don't need editing
  JsonDocument doc = parseJson(R"({ "defaultColourCode": [1, 2, 3], "modes": [] })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.headerColourCode.red, 1);
  EXPECT_EQ(parsedConfig.headerColourCode.green, 2);
  EXPECT_EQ(parsedConfig.headerColourCode.blue, 3);
}

TEST_F(ConfigParsingTest, Config_WithNoMidiChannel_DefaultsToOne) {
  JsonDocument doc = parseJson(R"({ "modes": [] })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.midiChannel, 1);
}

TEST_F(ConfigParsingTest, Config_WithMidiChannel_ParsesIt) {
  JsonDocument doc = parseJson(R"({ "midiChannel": 9, "modes": [] })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.midiChannel, 9);
}

TEST_F(ConfigParsingTest, Config_ParsesRootParametersAndButtons) {
  JsonDocument doc = parseJson(R"({
    "parameters": ["Mode", "Config"],
    "buttons": [ { "id": 12, "name": "Global", "buttonMode": "Momentary", "actions": [] } ],
    "modes": []
  })");

  Config parsedConfig(doc);

  ASSERT_EQ(parsedConfig.parameters.size(), 2u);
  EXPECT_EQ(parsedConfig.parameters[0], Parameter::ModeParameter);
  EXPECT_EQ(parsedConfig.parameters[1], Parameter::ConfigParameter);
  ASSERT_EQ(parsedConfig.buttons.size(), 1u);
  EXPECT_EQ(parsedConfig.buttons[0].id, 12);
}

TEST_F(ConfigParsingTest, Config_GetModeNames_JoinsWithCommas) {
  JsonDocument doc = parseJson(R"({
    "modes": [ { "name": "Solo" }, { "name": "Chord" }, { "name": "FX" } ]
  })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.getModeNames(), "Solo, Chord, FX");
}

TEST_F(ConfigParsingTest, Config_GetModeNames_WithOneMode_HasNoComma) {
  JsonDocument doc = parseJson(R"({
    "modes": [ { "name": "OnlyOne" } ]
  })");

  Config parsedConfig(doc);

  EXPECT_EQ(parsedConfig.getModeNames(), "OnlyOne");
}

TEST_F(ConfigParsingTest, Mode_WithMidiChannel_ParsesIt) {
  JsonDocument doc = parseJson(R"({ "name": "Live", "midiChannel": 5 })");

  Mode mode(doc.as<JsonVariant>());

  EXPECT_EQ(mode.midiChannel, 5);
}

TEST_F(ConfigParsingTest, Mode_WithNoMidiChannel_DefaultsToUnset) {
  JsonDocument doc = parseJson(R"({ "name": "Live" })");

  Mode mode(doc.as<JsonVariant>());

  EXPECT_EQ(mode.midiChannel, -1);
}

TEST_F(ConfigParsingTest, ModeBank_ParsesSubheaderColourCode) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "subheaderColourCode": [5, 6, 7] })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.subheaderColourCode.red, 5);
  EXPECT_EQ(bank.subheaderColourCode.green, 6);
  EXPECT_EQ(bank.subheaderColourCode.blue, 7);
}

TEST_F(ConfigParsingTest, ModeBank_WithNoSubheaderColourCode_DefaultsPerSchema) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.subheaderColourCode.red, 44);
  EXPECT_EQ(bank.subheaderColourCode.green, 44);
  EXPECT_EQ(bank.subheaderColourCode.blue, 44);
}

TEST_F(ConfigParsingTest, ModeBank_ParsesScreenLayout) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "screenLayout": "Live" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.screenLayout, ScreenLayout::ScreenLayoutLive);
}

TEST_F(ConfigParsingTest, ModeBank_WithNoScreenLayout_DefaultsToGrid) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.screenLayout, ScreenLayout::ScreenLayoutGrid);
}

TEST_F(ConfigParsingTest, ModeBank_WithUnrecognisedScreenLayout_FallsBackToGrid) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "screenLayout": "Giant" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.screenLayout, ScreenLayout::ScreenLayoutGrid);
}

TEST_F(ConfigParsingTest, ModeBank_ParsesBankType) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "bankType": "PersistentButtons" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.bankType, BankType::BankTypePersistentButtons);
}

TEST_F(ConfigParsingTest, ModeBank_WithNoBankType_DefaultsToBankTypeDefault) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.bankType, BankType::BankTypeDefault);
}

TEST_F(ConfigParsingTest, ModeBank_WithUnknownBankType_DefaultsToBankTypeDefault) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "bankType": "SomethingNew" })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.bankType, BankType::BankTypeDefault);
}

TEST_F(ConfigParsingTest, ModeBank_ParsesDefaultPatchIndex) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "defaultPatchIndex": 2,
    "patches": [ { "name": "P1" }, { "name": "P2" }, { "name": "P3" } ] })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.defaultPatchIndex, 2);
  EXPECT_EQ(bank.resolveDefaultPatchIndex(), 2);
}

TEST_F(ConfigParsingTest, ModeBank_WithNoDefaultPatchIndex_DefaultsToZero) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank",
    "patches": [ { "name": "P1" }, { "name": "P2" } ] })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.defaultPatchIndex, 0);
  EXPECT_EQ(bank.resolveDefaultPatchIndex(), 0);
}

TEST_F(ConfigParsingTest, ModeBank_ResolveDefaultPatchIndex_ClampsPastTheEndOfPatches) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "defaultPatchIndex": 99,
    "patches": [ { "name": "P1" }, { "name": "P2" } ] })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.resolveDefaultPatchIndex(), 1);   // clamped to the last patch
}

TEST_F(ConfigParsingTest, ModeBank_ResolveDefaultPatchIndex_IsMinusOneWithNoPatches) {
  JsonDocument doc = parseJson(R"({ "name": "Song Bank", "defaultPatchIndex": 3 })");

  ModeBank bank(doc.as<JsonVariant>());

  EXPECT_EQ(bank.resolveDefaultPatchIndex(), -1);
}

TEST_F(ConfigParsingTest, Patch_ParsesParameters) {
  JsonDocument doc = parseJson(R"({ "name": "Lead Tone", "parameters": ["Patch", "MidiChannel"] })");

  Patch patch(doc.as<JsonVariant>());

  ASSERT_EQ(patch.parameters.size(), 2u);
  EXPECT_EQ(patch.parameters[0], Parameter::PatchParameter);
  EXPECT_EQ(patch.parameters[1], Parameter::MIDIChannelParameter);
}

TEST_F(ConfigParsingTest, Patch_WithNoParameters_LeavesThemEmpty) {
  JsonDocument doc = parseJson(R"({ "name": "Lead Tone" })");

  Patch patch(doc.as<JsonVariant>());

  EXPECT_TRUE(patch.parameters.empty());
}

// ---- presets (ExecutePreset) ------------------------------------------------

// a presets array shared by the tests below: preset 1 is a program change then
// a control change, preset 2 is a single note
static const char *PRESETS_JSON = R"([
  { "id": 1, "name": "Clean", "actions": [
      { "type": "ProgramChange", "actionParameters": { "programNumber": 4 } },
      { "type": "ControlChange", "actionParameters": { "controllerNumber": 91, "controlValue": 64 } }
  ]},
  { "id": 2, "name": "Chord", "actions": [
      { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"] } }
  ]}
])";

TEST_F(ConfigParsingTest, Button_ExecutePreset_ExpandsToThePresetsActions) {
  JsonDocument presets = parseJson(PRESETS_JSON);
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Verse", "buttonMode": "Momentary",
    "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } } ]
  })");

  Button button(doc.as<JsonVariant>(), presets.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 2u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::ProgramChange);
  EXPECT_EQ(button.actions[1]->getType(), ActionType::ControlChange);
}

TEST_F(ConfigParsingTest, Button_ExecutePreset_RunsInPlaceBetweenCustomActions) {
  // custom action, then a preset, then another custom action: the preset's
  // actions land exactly where the ExecutePreset action was
  JsonDocument presets = parseJson(PRESETS_JSON);
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Mixed", "buttonMode": "Momentary",
    "actions": [
      { "type": "PlayNotes", "actionParameters": { "noteNames": ["E3"] } },
      { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } },
      { "type": "SetPage", "actionParameters": { "page": 2 } }
    ]
  })");

  Button button(doc.as<JsonVariant>(), presets.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 4u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::PlayNotes);
  EXPECT_EQ(button.actions[1]->getType(), ActionType::ProgramChange);
  EXPECT_EQ(button.actions[2]->getType(), ActionType::ControlChange);
  EXPECT_EQ(button.actions[3]->getType(), ActionType::SetPage);
}

TEST_F(ConfigParsingTest, Button_ExecutePreset_CanRunSeveralPresetsAndTheSameOneTwice) {
  JsonDocument presets = parseJson(PRESETS_JSON);
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Many", "buttonMode": "Momentary",
    "actions": [
      { "type": "ExecutePreset", "actionParameters": { "presetId": 2 } },
      { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } },
      { "type": "ExecutePreset", "actionParameters": { "presetId": 2 } }
    ]
  })");

  Button button(doc.as<JsonVariant>(), presets.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 4u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::PlayNotes);
  EXPECT_EQ(button.actions[1]->getType(), ActionType::ProgramChange);
  EXPECT_EQ(button.actions[2]->getType(), ActionType::ControlChange);
  EXPECT_EQ(button.actions[3]->getType(), ActionType::PlayNotes);
  // each use is its own object, never a shared one
  EXPECT_NE(button.actions[0].get(), button.actions[3].get());
}

TEST_F(ConfigParsingTest, Button_ExecutePreset_UnknownPresetIsSkipped) {
  JsonDocument presets = parseJson(PRESETS_JSON);
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Dangling", "buttonMode": "Momentary",
    "actions": [
      { "type": "ExecutePreset", "actionParameters": { "presetId": 99 } },
      { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } }
    ]
  })");

  Button button(doc.as<JsonVariant>(), presets.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 1u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::ProgramChange);
}

TEST_F(ConfigParsingTest, Button_ExecutePreset_WithNoPresetsArrayOrNoIdIsSkipped) {
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "NoPresets", "buttonMode": "Momentary",
    "actions": [
      { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } },
      { "type": "ExecutePreset" }
    ]
  })");

  Button button(doc.as<JsonVariant>());

  EXPECT_TRUE(button.actions.empty());
}

TEST_F(ConfigParsingTest, Button_ExecutePreset_NestedExecutePresetInsideAPresetIsSkipped) {
  JsonDocument presets = parseJson(R"([
    { "id": 1, "name": "Loop", "actions": [
        { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } },
        { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } }
    ]}
  ])");
  JsonDocument doc = parseJson(R"({
    "id": 0, "name": "Nested", "buttonMode": "Momentary",
    "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 1 } } ]
  })");

  Button button(doc.as<JsonVariant>(), presets.as<JsonVariant>());

  ASSERT_EQ(button.actions.size(), 1u);
  EXPECT_EQ(button.actions[0]->getType(), ActionType::ProgramChange);
}

TEST_F(ConfigParsingTest, Config_PresetsReachButtonsAtEveryLevel) {
  JsonDocument doc = parseJson(R"({
    "presets": [
      { "id": 7, "name": "PC", "actions": [
          { "type": "ProgramChange", "actionParameters": { "programNumber": 3 } } ] }
    ],
    "buttons": [ { "id": 0, "name": "File", "buttonMode": "Momentary",
        "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 7 } } ] } ],
    "modes": [ {
      "name": "M",
      "buttons": [ { "id": 1, "name": "Mode", "buttonMode": "Momentary",
          "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 7 } } ] } ],
      "banks": [ {
        "name": "B",
        "buttons": [ { "id": 2, "name": "Bank", "buttonMode": "Momentary",
            "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 7 } } ] } ],
        "patches": [ {
          "name": "P",
          "buttons": [ { "id": 3, "name": "Patch", "buttonMode": "Momentary",
              "actions": [ { "type": "ExecutePreset", "actionParameters": { "presetId": 7 } } ] } ]
        } ]
      } ]
    } ]
  })");

  Config parsedConfig(doc);

  ASSERT_EQ(parsedConfig.buttons[0].actions.size(), 1u);
  EXPECT_EQ(parsedConfig.buttons[0].actions[0]->getType(), ActionType::ProgramChange);
  const Mode &mode = parsedConfig.modes[0];
  ASSERT_EQ(mode.buttons[0].actions.size(), 1u);
  const ModeBank &bank = mode.banks[0];
  ASSERT_EQ(bank.buttons[0].actions.size(), 1u);
  ASSERT_EQ(bank.patches[0].buttons[0].actions.size(), 1u);
  EXPECT_EQ(bank.patches[0].buttons[0].actions[0]->getType(), ActionType::ProgramChange);
}
