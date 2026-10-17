//----------------------------------------------------------------------------//
//    Tests for ConfigStore.h - EEPROM persistence and multi-file config    //
//    discovery/loading. These do touch a real (temporary) directory on     //
//    disk, since that's genuinely what ConfigStore.h's job is - unlike the //
//    other test files, this one isn't pure logic with no I/O.             //
//----------------------------------------------------------------------------//

#include <filesystem>
#include <fstream>

class ConfigStoreTest : public ::testing::Test {
  protected:
    std::filesystem::path testDir;

    void SetUp() override {
      resetTestState();
      // a fresh, uniquely-named temp directory per test, standing in for
      // the SD card root - keeps tests isolated from each other and from
      // whatever else might be on the real filesystem
      std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
      testDir = std::filesystem::temp_directory_path() / ("miditoolbox_test_" + testName);
      std::filesystem::remove_all(testDir);
      std::filesystem::create_directories(testDir / "configs");
      previousCwd = std::filesystem::current_path();
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

  private:
    std::filesystem::path previousCwd;
};

TEST_F(ConfigStoreTest, GetLastConfigFilename_OnFreshEeprom_ReturnsEmpty) {
  EXPECT_EQ(getLastConfigFilename(), "");
}

TEST_F(ConfigStoreTest, SaveThenGetLastConfigFilename_RoundTrips) {
  saveLastConfigFilename("My Rig.json");

  EXPECT_EQ(getLastConfigFilename(), "My Rig.json");
}

TEST_F(ConfigStoreTest, ListConfigFiles_ReturnsOnlyJsonFiles_SortedAlphabetically) {
  writeFile("configs/Zebra.json", "{}");
  writeFile("configs/Apple.json", "{}");
  writeFile("configs/notes.txt", "not json");
  writeFile("configs/Mango.json", "{}");

  std::vector<String> files = listConfigFiles();

  ASSERT_EQ(files.size(), 3u);
  EXPECT_EQ(files[0], "Apple.json");
  EXPECT_EQ(files[1], "Mango.json");
  EXPECT_EQ(files[2], "Zebra.json");
}

TEST_F(ConfigStoreTest, ListConfigFiles_WhenDirectoryMissing_ReturnsEmpty) {
  std::filesystem::remove_all(testDir / "configs");

  std::vector<String> files = listConfigFiles();

  EXPECT_TRUE(files.empty());
}

TEST_F(ConfigStoreTest, LoadConfigFile_Success_PopulatesConfigResetsSelectionAndPersistsChoice) {
  writeFile("configs/Test.json", R"({
    "headerColourCode": [1,2,3],
    "modes": [ { "name": "OnlyMode", "banks": [ { "name": "OnlyBank", "patches": [ { "name": "OnlyPatch" } ] } ] } ]
  })");

  bool ok = loadConfigFile("Test.json");

  EXPECT_TRUE(ok);
  EXPECT_EQ(currentConfigFilename, "Test.json");
  ASSERT_EQ(config.modes.size(), 1u);
  EXPECT_EQ(config.modes[0].name, "OnlyMode");
  // resetRuntimeSelection() should have auto-selected the first bank/patch
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  // and the choice should be remembered for the next boot
  EXPECT_EQ(getLastConfigFilename(), "Test.json");
}

TEST_F(ConfigStoreTest, LoadConfigFile_Success_HonoursTheFirstBanksDefaultPatchIndex) {
  writeFile("configs/Test.json", R"({
    "headerColourCode": [1,2,3],
    "modes": [ { "name": "OnlyMode", "banks": [ { "name": "OnlyBank", "defaultPatchIndex": 1,
      "patches": [ { "name": "P1" }, { "name": "P2" } ] } ] } ]
  })");

  bool ok = loadConfigFile("Test.json");

  EXPECT_TRUE(ok);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 1);   // the configured default, not 0
}

// Loads the real, shipped "Live Rig.json" (not a synthetic tree) and
// exercises its "Pads" / "Key: F" bank end to end: boot lands on the
// defaultPatchIndex with the matching voicing button lit, and every
// Exclusive sibling (Low/Middle/Wide/High, ids 5-8) still correctly selects
// - and lights - its own patch with no hijacking back to "Middle". This is
// the exact bank shape (defaultPatchIndex + defaultStatus:"On" on a
// bank-level Exclusive SetPatch button sharing a buttonSet with siblings)
// that once broke every button in the set - see the block comment on
// DefaultStatusOnBankLevelExclusiveButton_SeedsOnceAndSurvivesSiblingPresses
// in navigation_tests.inc for the full mechanics of what was fixed.
TEST_F(ConfigStoreTest, LiveRig_PadsKeyF_ExclusiveVoicingButtons_EachSelectsAndLightsItsOwnPatch) {
  std::filesystem::path repoConfig =
      std::filesystem::path(MIDITOOLBOX_PROJECT_DIR) / "configs" / "Live Rig.json";
  ASSERT_TRUE(std::filesystem::exists(repoConfig)) << repoConfig;
  std::filesystem::create_directories("configs");
  std::filesystem::copy_file(repoConfig, "configs/Live Rig.json",
                              std::filesystem::copy_options::overwrite_existing);

  ASSERT_TRUE(loadConfigFile("Live Rig.json"));
  ASSERT_EQ(getCurrentMode().name, "Pads");
  ASSERT_EQ(getCurrentBank()->name, "Key: F");
  ASSERT_EQ(currentPatchIndex, 1);          // defaultPatchIndex honoured
  ASSERT_TRUE(resolveButton(6)->isOn);      // "Middle" lit at boot

  triggerButton(5);   // Low
  EXPECT_EQ(currentPatchIndex, 0) << "Low should select patch 0";
  EXPECT_TRUE(resolveButton(5)->isOn);
  EXPECT_FALSE(resolveButton(6)->isOn);

  triggerButton(7);   // Wide
  EXPECT_EQ(currentPatchIndex, 2) << "Wide should select patch 2";
  EXPECT_TRUE(resolveButton(7)->isOn);
  EXPECT_FALSE(resolveButton(5)->isOn);

  triggerButton(8);   // High
  EXPECT_EQ(currentPatchIndex, 3) << "High should select patch 3";
  EXPECT_TRUE(resolveButton(8)->isOn);
  EXPECT_FALSE(resolveButton(7)->isOn);

  triggerButton(6);   // back to Middle
  EXPECT_EQ(currentPatchIndex, 1) << "Middle should select patch 1";
  EXPECT_TRUE(resolveButton(6)->isOn);
  EXPECT_FALSE(resolveButton(8)->isOn);
}

TEST_F(ConfigStoreTest, LoadConfigFile_MissingFile_ReturnsFalse_LeavesExistingConfigInPlace) {
  writeFile("configs/Existing.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Existing"}]})");
  ASSERT_TRUE(loadConfigFile("Existing.json"));

  bool ok = loadConfigFile("DoesNotExist.json");

  EXPECT_FALSE(ok);
  EXPECT_EQ(currentConfigFilename, "Existing.json");
  ASSERT_EQ(config.modes.size(), 1u);
  EXPECT_EQ(config.modes[0].name, "Existing");
}

TEST_F(ConfigStoreTest, LoadConfigFile_InvalidJson_ReturnsFalse) {
  writeFile("configs/Broken.json", "{ this is not valid json");

  bool ok = loadConfigFile("Broken.json");

  EXPECT_FALSE(ok);
}

// stepping the pending Config cursor walks listConfigFiles()'s alphabetical
// order and wraps - without loading anything (that only happens on activate).
// The full navigation flow is covered in navigation_tests.inc; this pins the
// alphabetical ordering that ConfigStore.h's listConfigFiles() provides.
TEST_F(ConfigStoreTest, PendingCycleConfig_WalksTheAlphabeticalFileListAndWraps) {
  writeFile("configs/Apple.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Apple"}]})");
  writeFile("configs/Mango.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Mango"}]})");
  writeFile("configs/Zebra.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Zebra"}]})");
  ASSERT_TRUE(loadConfigFile("Apple.json"));
  resetPendingToLive();

  pendingCycleConfig(CycleDirection::CycleUp);
  EXPECT_EQ(pendingConfigFilename, "Mango.json");
  EXPECT_EQ(currentConfigFilename, "Apple.json");   // not loaded

  pendingCycleConfig(CycleDirection::CycleUp);
  EXPECT_EQ(pendingConfigFilename, "Zebra.json");

  // wraps back around to the first file after the last
  pendingCycleConfig(CycleDirection::CycleUp);
  EXPECT_EQ(pendingConfigFilename, "Apple.json");

  // and stepping down wraps the other way
  pendingCycleConfig(CycleDirection::CycleDown);
  EXPECT_EQ(pendingConfigFilename, "Zebra.json");
}

TEST_F(ConfigStoreTest, LoadStartupConfig_PrefersPreviouslySelectedFile) {
  writeFile("configs/Apple.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Apple"}]})");
  writeFile("configs/Zebra.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Zebra"}]})");
  saveLastConfigFilename("Zebra.json");

  loadStartupConfig();

  EXPECT_EQ(currentConfigFilename, "Zebra.json");
}

TEST_F(ConfigStoreTest, LoadStartupConfig_FallsBackToFirstFile_WhenSavedChoiceIsMissing) {
  writeFile("configs/Apple.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Apple"}]})");
  writeFile("configs/Mango.json", R"({"headerColourCode":[0,0,0],"modes":[{"name":"Mango"}]})");
  saveLastConfigFilename("Deleted.json"); // no longer exists on "disk"

  loadStartupConfig();

  EXPECT_EQ(currentConfigFilename, "Apple.json"); // first alphabetically
}

TEST_F(ConfigStoreTest, LoadStartupConfig_FallsBackToLegacyFile_WhenNoConfigsDirectoryExists) {
  std::filesystem::remove_all(testDir / "configs");
  writeFile(std::string(LEGACY_CONFIG_FILENAME), R"({"headerColourCode":[0,0,0],"modes":[{"name":"Legacy"}]})");

  loadStartupConfig();

  EXPECT_EQ(currentConfigFilename, LEGACY_CONFIG_FILENAME);
  ASSERT_EQ(config.modes.size(), 1u);
  EXPECT_EQ(config.modes[0].name, "Legacy");
}
