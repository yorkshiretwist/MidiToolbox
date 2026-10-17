//----------------------------------------------------------------------------//
//    Tests for DeviceStore.h - reading and checking /device.json.            //
//    The valid file used throughout is config/live-rig-device.json, the      //
//    same file the Designer's own tests check its output against, so a       //
//    change that makes the Designer and the firmware disagree fails here.    //
//----------------------------------------------------------------------------//

#include <filesystem>
#include <fstream>
#include <sstream>

// the real profile for the original unit, from the repo's config/ folder.
// __FILE__ is this .inc's path (test/tests/...), so config/ is three levels up.
static std::string liveRigDeviceJson() {
  std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." / ".." / "config" / "live-rig-device.json";
  std::ifstream in(path);
  EXPECT_TRUE(in.good()) << "cannot read " << path;
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

class DeviceStoreTest : public ::testing::Test {
  protected:
    std::filesystem::path testDir;

    void SetUp() override {
      std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
      testDir = std::filesystem::temp_directory_path() / ("miditoolbox_device_test_" + testName);
      std::filesystem::remove_all(testDir);
      std::filesystem::create_directories(testDir);
      previousCwd = std::filesystem::current_path();
      liveRig = liveRigDeviceJson();
      std::filesystem::current_path(testDir);
    }

    void TearDown() override {
      std::filesystem::current_path(previousCwd);
      std::filesystem::remove_all(testDir);
    }

    void writeFile(const std::string &relativePath, const std::string &content) {
      std::ofstream out(relativePath);
      out << content;
    }

    // parses `text` and runs it through parseDeviceProfile, returning whether it passed
    bool accepts(const std::string &text) {
      JsonDocument doc;
      EXPECT_EQ(deserializeJson(doc, text.c_str()), DeserializationError::Ok);
      DeviceProfile out = deviceProfileFromHeader();
      return parseDeviceProfile(doc.as<JsonVariantConst>(), out, error);
    }

    // the live rig profile with one change made to it by `edit`
    bool acceptsEdited(void (*edit)(JsonDocument &)) {
      JsonDocument doc;
      deserializeJson(doc, liveRig.c_str());
      edit(doc);
      std::string text;
      serializeJson(doc, text);
      return accepts(text);
    }

    std::string liveRig;
    String error;

  private:
    std::filesystem::path previousCwd;
};

TEST_F(DeviceStoreTest, TheLiveRigFileIsAcceptedAndMatchesTheCompiledInWiring) {
  JsonDocument doc;
  ASSERT_EQ(deserializeJson(doc, liveRig.c_str()), DeserializationError::Ok);
  DeviceProfile loaded;
  memset(&loaded, 0, sizeof(loaded));
  ASSERT_TRUE(parseDeviceProfile(doc.as<JsonVariantConst>(), loaded, error)) << error.c_str();

  // device.json for the original unit must describe exactly what the
  // firmware was compiled for, or the Designer and firmware have diverged
  DeviceProfile built = deviceProfileFromHeader();
  EXPECT_STREQ(loaded.name, built.name);
  EXPECT_EQ(loaded.buttonCount, built.buttonCount);
  EXPECT_EQ(loaded.navSelectId, built.navSelectId);
  EXPECT_EQ(loaded.navDownId, built.navDownId);
  EXPECT_EQ(loaded.navUpId, built.navUpId);
  EXPECT_EQ(loaded.shiftRegisters, built.shiftRegisters);
  EXPECT_EQ(loaded.gridRows, built.gridRows);
  EXPECT_EQ(loaded.gridHalfCols, built.gridHalfCols);
  for (int id = 0; id < built.buttonCount; id++) {
    EXPECT_EQ(loaded.buttonPins[id], built.buttonPins[id]) << "button " << id;
  }
  EXPECT_EQ(memcmp(loaded.gridButtonIds, built.gridButtonIds, sizeof(built.gridButtonIds)), 0);
}

TEST_F(DeviceStoreTest, LoadDeviceProfile_ReadsTheFileFromTheSdRoot) {
  writeFile("device.json", liveRig);
  DeviceProfile out;
  memset(&out, 0, sizeof(out));
  EXPECT_EQ(loadDeviceProfile(out, error), DeviceLoadResult::Loaded) << error.c_str();
  EXPECT_EQ(out.buttonCount, 13);
}

TEST_F(DeviceStoreTest, LoadDeviceProfile_WithNoFile_ReportsNotFound) {
  DeviceProfile out = deviceProfileFromHeader();
  EXPECT_EQ(loadDeviceProfile(out, error), DeviceLoadResult::NotFound);
}

TEST_F(DeviceStoreTest, LoadDeviceProfile_WithBrokenJson_IsInvalid) {
  writeFile("device.json", "{ \"schemaVersion\": 1, ");
  DeviceProfile out = deviceProfileFromHeader();
  EXPECT_EQ(loadDeviceProfile(out, error), DeviceLoadResult::Invalid);
  EXPECT_NE(error.indexOf("not valid JSON"), -1);
}

TEST_F(DeviceStoreTest, LoadDeviceProfile_WithAnOversizeFile_IsInvalid) {
  writeFile("device.json", std::string(DEVICE_FILE_MAX_BYTES + 1, ' '));
  DeviceProfile out = deviceProfileFromHeader();
  EXPECT_EQ(loadDeviceProfile(out, error), DeviceLoadResult::Invalid);
}

TEST_F(DeviceStoreTest, AFailedParseLeavesTheProfileUntouched) {
  JsonDocument doc;
  deserializeJson(doc, liveRig.c_str());
  doc["buttons"]["pins"][1] = doc["buttons"]["pins"][0]; // a duplicate pin
  DeviceProfile out = deviceProfileFromHeader();
  out.buttonCount = 99; // a recognisable value
  EXPECT_FALSE(parseDeviceProfile(doc.as<JsonVariantConst>(), out, error));
  EXPECT_EQ(out.buttonCount, 99);
}

TEST_F(DeviceStoreTest, RejectsNonObjects) {
  EXPECT_FALSE(accepts("[]"));
  EXPECT_FALSE(accepts("42"));
}

TEST_F(DeviceStoreTest, RejectsAnUnknownSchemaVersion) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["schemaVersion"] = 2; }));
}

TEST_F(DeviceStoreTest, RejectsADifferentMicrocontroller) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["microcontroller"] = "Teensy 4.0"; }));
}

TEST_F(DeviceStoreTest, RejectsAMissingOrOverlongName) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["name"] = ""; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["name"] = std::string(41, 'x'); }));
  EXPECT_TRUE(acceptsEdited([](JsonDocument &d) { d["name"] = std::string(40, 'x'); }));
}

TEST_F(DeviceStoreTest, RejectsTwoButtonsOnOnePin) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][5] = d["buttons"]["pins"][2]; }));
  EXPECT_NE(error.indexOf("used by both"), -1);
}

TEST_F(DeviceStoreTest, RejectsAButtonOnTheFixedLedPins) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = LED_PIN_DATA; }));
  EXPECT_NE(error.indexOf("LED chain"), -1);
}

TEST_F(DeviceStoreTest, RejectsAButtonOnAScreenPin) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = SCREEN_PIN_DC; }));
}

TEST_F(DeviceStoreTest, RejectsPinsThatDoNotExistOrBelongToTheSdSocket) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = 55; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = -1; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = 44; }));
  // the underside pads are real pins and allowed
  EXPECT_TRUE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = 52; }));
}

TEST_F(DeviceStoreTest, RejectsFractionalOrTextPins) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = 3.5; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["pins"][0] = "33"; }));
}

TEST_F(DeviceStoreTest, RejectsTooFewOrTooManyButtons) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) {
    JsonArray pins = d["buttons"]["pins"].to<JsonArray>();
    for (int p : {33, 34, 35, 36, 37}) pins.add(p);
  }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) {
    JsonArray pins = d["buttons"]["pins"].to<JsonArray>();
    for (int i = 0; i < MAX_BUTTONS + 1; i++) pins.add(i % 20);
  }));
}

TEST_F(DeviceStoreTest, RejectsNavigationButtonsThatDoNotExistOrRepeat) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["navigation"]["up"] = 13; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["navigation"]["up"] = 10; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["buttons"]["navigation"].remove("down"); }));
}

TEST_F(DeviceStoreTest, RejectsAGridCellForANavigationButtonOrAMissingButton) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["cells"][0][1] = 11; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["cells"][0][1] = 13; }));
}

TEST_F(DeviceStoreTest, RejectsAGridWithTwoCellsForOneButton) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["cells"][0][2] = 5; }));
}

TEST_F(DeviceStoreTest, RejectsAGridWhoseShapeDoesNotMatchItsSize) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["rows"] = 3; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["halfCols"] = 11; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["grid"]["cells"][1].remove(9); }));
}

TEST_F(DeviceStoreTest, PadsTheUnusedPartOfASmallerGridWithNoCell) {
  // a 1-row, 4-half-column grid holding two of the unit's buttons
  DeviceProfile out = deviceProfileFromHeader();
  JsonDocument doc;
  deserializeJson(doc, liveRig.c_str());
  doc["screen"]["grid"]["rows"] = 1;
  doc["screen"]["grid"]["halfCols"] = 4;
  JsonArray cells = doc["screen"]["grid"]["cells"].to<JsonArray>();
  JsonArray row = cells.add<JsonArray>();
  for (int v : {0, 255, 1, 255}) row.add(v);
  ASSERT_TRUE(parseDeviceProfile(doc.as<JsonVariantConst>(), out, error)) << error.c_str();
  EXPECT_EQ(out.gridButtonIds[0][2], 1);
  EXPECT_EQ(out.gridButtonIds[0][4], GRID_NO_CELL);
  EXPECT_EQ(out.gridButtonIds[1][0], GRID_NO_CELL);
}

TEST_F(DeviceStoreTest, RejectsTooFewShiftRegistersForTheLeds) {
  // 13 RGB LEDs need 39 outputs: 5 registers (40) is enough, 4 (32) is not
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["leds"]["registers"] = 4; }));
  EXPECT_TRUE(acceptsEdited([](JsonDocument &d) { d["leds"]["registers"] = 6; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["leds"]["registers"] = 0; }));
}

TEST_F(DeviceStoreTest, RejectsAScreenThisFirmwareWasNotBuiltFor) {
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["driver"] = "ST7789"; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["width"] = 240; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["orientation"] = "portrait"; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["pins"]["CS"] = 20; }));
  EXPECT_FALSE(acceptsEdited([](JsonDocument &d) { d["screen"]["pins"].remove("SCK"); }));
}
