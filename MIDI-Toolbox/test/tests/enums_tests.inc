//----------------------------------------------------------------------------//
//    Tests for Enums.h - the JSON string <-> enum conversion functions.     //
//    These are simple pure functions (no state, no side effects), so       //
//    they're tested directly with plain TEST() rather than a fixture.       //
//----------------------------------------------------------------------------//

TEST(EnumsTest, GetButtonModeEnum_ParsesEachKnownValue) {
  EXPECT_EQ(getButtonModeEnum(parseJson("\"Momentary\"").as<JsonVariant>()), ButtonMode::Momentary);
  EXPECT_EQ(getButtonModeEnum(parseJson("\"Latched\"").as<JsonVariant>()), ButtonMode::Latched);
  EXPECT_EQ(getButtonModeEnum(parseJson("\"Exclusive\"").as<JsonVariant>()), ButtonMode::Exclusive);
}

TEST(EnumsTest, GetButtonModeEnum_UnknownValueDefaultsToMomentary) {
  // an unrecognised or missing value should fail safe to the most ordinary
  // behaviour (Momentary), not crash or silently pick something surprising
  EXPECT_EQ(getButtonModeEnum(parseJson("\"NotARealMode\"").as<JsonVariant>()), ButtonMode::Momentary);
}

TEST(EnumsTest, GetButtonModeString_RoundTripsWithGetButtonModeEnum) {
  // for every enum value, converting to a string and back should return
  // the same value it started as
  for (ButtonMode mode : {ButtonMode::Momentary, ButtonMode::Latched, ButtonMode::Exclusive}) {
    std::string_view asString = getButtonModeString(mode);
    String json = "\"" + String(std::string(asString).c_str()) + "\"";
    EXPECT_EQ(getButtonModeEnum(parseJson(json.c_str()).as<JsonVariant>()), mode);
  }
}

TEST(EnumsTest, GetButtonStatusEnum_ParsesOnAndOff) {
  EXPECT_EQ(getButtonStatusEnum(parseJson("\"Off\"").as<JsonVariant>()), ButtonStatus::ButtonStatusOff);
  EXPECT_EQ(getButtonStatusEnum(parseJson("\"On\"").as<JsonVariant>()), ButtonStatus::ButtonStatusOn);
}

TEST(EnumsTest, GetButtonStatusEnum_UnknownOrMissingValueDefaultsToOff) {
  // anything that isn't exactly "On" - an unrecognised word, or the "null"
  // a missing JSON key deserialises to - should mean the button starts off,
  // the same fail-safe behaviour getButtonModeEnum has
  EXPECT_EQ(getButtonStatusEnum(parseJson("\"Enabled\"").as<JsonVariant>()), ButtonStatus::ButtonStatusOff);
  EXPECT_EQ(getButtonStatusEnum(parseJson("null").as<JsonVariant>()), ButtonStatus::ButtonStatusOff);
}

TEST(EnumsTest, GetButtonStatusString_RoundTripsWithGetButtonStatusEnum) {
  for (ButtonStatus status : {ButtonStatus::ButtonStatusOff, ButtonStatus::ButtonStatusOn}) {
    std::string_view asString = getButtonStatusString(status);
    String json = "\"" + String(std::string(asString).c_str()) + "\"";
    EXPECT_EQ(getButtonStatusEnum(parseJson(json.c_str()).as<JsonVariant>()), status);
  }
}

TEST(EnumsTest, GetBankTypeEnum_ParsesEachKnownValue) {
  EXPECT_EQ(getBankTypeEnum(parseJson("\"Default\"").as<JsonVariant>()), BankType::BankTypeDefault);
  EXPECT_EQ(getBankTypeEnum(parseJson("\"PersistentButtons\"").as<JsonVariant>()), BankType::BankTypePersistentButtons);
}

TEST(EnumsTest, GetBankTypeEnum_UnknownOrMissingValueDefaultsToDefault) {
  // an unrecognised name, or the "null" a missing JSON key deserialises to,
  // fails safe to the ordinary behaviour - the same approach as
  // getButtonModeEnum / getButtonStatusEnum
  EXPECT_EQ(getBankTypeEnum(parseJson("\"Cumulative\"").as<JsonVariant>()), BankType::BankTypeDefault);
  EXPECT_EQ(getBankTypeEnum(parseJson("null").as<JsonVariant>()), BankType::BankTypeDefault);
}

TEST(EnumsTest, GetBankTypeString_RoundTripsWithGetBankTypeEnum) {
  for (BankType bankType : {BankType::BankTypeDefault, BankType::BankTypePersistentButtons}) {
    std::string_view asString = getBankTypeString(bankType);
    String json = "\"" + String(std::string(asString).c_str()) + "\"";
    EXPECT_EQ(getBankTypeEnum(parseJson(json.c_str()).as<JsonVariant>()), bankType);
  }
}

TEST(EnumsTest, GetActionTypeEnum_ParsesEachKnownValue) {
  EXPECT_EQ(getActionTypeEnum(parseJson("\"PlayNotes\"").as<JsonVariant>()), ActionType::PlayNotes);
  // "Note" is accepted as a backward-compatible alias for "PlayNotes" - see Enums.h
  EXPECT_EQ(getActionTypeEnum(parseJson("\"Note\"").as<JsonVariant>()), ActionType::PlayNotes);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"ProgramChange\"").as<JsonVariant>()), ActionType::ProgramChange);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"ControlChange\"").as<JsonVariant>()), ActionType::ControlChange);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetParameter\"").as<JsonVariant>()), ActionType::SetParameter);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CycleParameter\"").as<JsonVariant>()), ActionType::CycleParameter);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetParameterValue\"").as<JsonVariant>()), ActionType::SetParameterValue);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CycleParameterValue\"").as<JsonVariant>()), ActionType::CycleParameterValue);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetPatch\"").as<JsonVariant>()), ActionType::SetPatch);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CyclePatch\"").as<JsonVariant>()), ActionType::CyclePatch);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetBank\"").as<JsonVariant>()), ActionType::SetBank);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CycleBank\"").as<JsonVariant>()), ActionType::CycleBank);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetMode\"").as<JsonVariant>()), ActionType::SetMode);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CycleMode\"").as<JsonVariant>()), ActionType::CycleMode);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetConfig\"").as<JsonVariant>()), ActionType::SetConfig);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CycleConfig\"").as<JsonVariant>()), ActionType::CycleConfig);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"SetPage\"").as<JsonVariant>()), ActionType::SetPage);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"CyclePage\"").as<JsonVariant>()), ActionType::CyclePage);
  EXPECT_EQ(getActionTypeEnum(parseJson("\"ExecutePreset\"").as<JsonVariant>()), ActionType::ExecutePreset);
}

TEST(EnumsTest, GetActionTypeEnum_UnknownValueGivesNoAction) {
  EXPECT_EQ(getActionTypeEnum(parseJson("\"NotARealAction\"").as<JsonVariant>()), ActionType::NoAction);
}

TEST(EnumsTest, GetCycleDirectionEnum_ParsesUpAndDown) {
  EXPECT_EQ(getCycleDirectionEnum(parseJson("\"Up\"").as<JsonVariant>()), CycleDirection::CycleUp);
  EXPECT_EQ(getCycleDirectionEnum(parseJson("\"Down\"").as<JsonVariant>()), CycleDirection::CycleDown);
}

TEST(EnumsTest, GetCycleDirectionEnum_UnknownValueDefaultsToDown) {
  EXPECT_EQ(getCycleDirectionEnum(parseJson("\"Sideways\"").as<JsonVariant>()), CycleDirection::CycleDown);
}

TEST(EnumsTest, GetParameterEnum_ParsesEachKnownValue) {
  EXPECT_EQ(getParameterEnum(parseJson("\"Patch\"").as<JsonVariant>()), Parameter::PatchParameter);
  EXPECT_EQ(getParameterEnum(parseJson("\"Bank\"").as<JsonVariant>()), Parameter::BankParameter);
  EXPECT_EQ(getParameterEnum(parseJson("\"Mode\"").as<JsonVariant>()), Parameter::ModeParameter);
  EXPECT_EQ(getParameterEnum(parseJson("\"MidiChannel\"").as<JsonVariant>()), Parameter::MIDIChannelParameter);
  // "MIDIChannel" (all-caps) is accepted as a backward-compatible alias for
  // "MidiChannel" - see Enums.h
  EXPECT_EQ(getParameterEnum(parseJson("\"MIDIChannel\"").as<JsonVariant>()), Parameter::MIDIChannelParameter);
  EXPECT_EQ(getParameterEnum(parseJson("\"Config\"").as<JsonVariant>()), Parameter::ConfigParameter);
  EXPECT_EQ(getParameterEnum(parseJson("\"Page\"").as<JsonVariant>()), Parameter::PageParameter);
}

TEST(EnumsTest, GetParameterString_RoundTripsWithGetParameterEnum) {
  for (Parameter parameter : {Parameter::PatchParameter, Parameter::BankParameter,
                              Parameter::ModeParameter, Parameter::MIDIChannelParameter,
                              Parameter::ConfigParameter, Parameter::PageParameter}) {
    String asString = getParameterString(parameter);
    EXPECT_EQ(getParameterEnum(parseJson(("\"" + std::string(asString.c_str()) + "\"").c_str()).as<JsonVariant>()),
              parameter);
  }
}

TEST(EnumsTest, GetParameterEnum_UnknownValueDefaultsToBank) {
  EXPECT_EQ(getParameterEnum(parseJson("\"Nonsense\"").as<JsonVariant>()), Parameter::BankParameter);
}

TEST(EnumsTest, GetNoteSpreadEnum_ParsesEachKnownValue) {
  EXPECT_EQ(getNoteSpreadEnum(parseJson("\"SpreadUp\"").as<JsonVariant>()), NoteSpread::SpreadUp);
  EXPECT_EQ(getNoteSpreadEnum(parseJson("\"SpreadDown\"").as<JsonVariant>()), NoteSpread::SpreadDown);
  EXPECT_EQ(getNoteSpreadEnum(parseJson("\"SpreadRandom\"").as<JsonVariant>()), NoteSpread::SpreadRandom);
}

TEST(EnumsTest, GetNoteSpreadEnum_UnknownValueDefaultsToNoSpread) {
  EXPECT_EQ(getNoteSpreadEnum(parseJson("\"Whatever\"").as<JsonVariant>()), NoteSpread::NoSpread);
}
