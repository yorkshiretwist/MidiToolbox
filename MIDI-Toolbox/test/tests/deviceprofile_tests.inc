//----------------------------------------------------------------------------//
//    Tests for DeviceProfile.h - the running description of the unit.        //
//    Until it can be loaded from a file, `device` is built from              //
//    MidiToolboxDevice.h, so these check the two agree and that the          //
//    fixed-size arrays are padded safely.                                    //
//----------------------------------------------------------------------------//

TEST(DeviceProfileTest, CopiesTheButtonWiringFromTheHeader) {
  DeviceProfile profile = deviceProfileFromHeader();
  EXPECT_EQ(profile.buttonCount, BUTTON_COUNT);
  for (int id = 0; id < BUTTON_COUNT; id++) {
    EXPECT_EQ(profile.buttonPins[id], BUTTON_PINS[id]) << "button " << id;
  }
}

TEST(DeviceProfileTest, CopiesTheNavigationIdsAndLedChainLength) {
  DeviceProfile profile = deviceProfileFromHeader();
  EXPECT_EQ(profile.navSelectId, NAV_SELECT_ID);
  EXPECT_EQ(profile.navDownId, NAV_DOWN_ID);
  EXPECT_EQ(profile.navUpId, NAV_UP_ID);
  EXPECT_EQ(profile.shiftRegisters, SHIFT_REGISTERS);
}

TEST(DeviceProfileTest, CopiesTheGridAndPadsTheRestWithNoCell) {
  DeviceProfile profile = deviceProfileFromHeader();
  EXPECT_EQ(profile.gridRows, SCREEN_GRID_ROWS);
  EXPECT_EQ(profile.gridHalfCols, SCREEN_GRID_HALF_COLS);
  for (int row = 0; row < MAX_GRID_ROWS; row++) {
    for (int col = 0; col < MAX_GRID_HALF_COLS; col++) {
      if (row < SCREEN_GRID_ROWS && col < SCREEN_GRID_HALF_COLS) {
        EXPECT_EQ(profile.gridButtonIds[row][col], SCREEN_GRID_BUTTON_IDS[row][col]);
      } else {
        // outside the unit's real grid there must be no cell, never a stray 0
        // (which would read as "button 0 starts here")
        EXPECT_EQ(profile.gridButtonIds[row][col], GRID_NO_CELL);
      }
    }
  }
}

TEST(DeviceProfileTest, TheGlobalDeviceMatchesTheHeaderProfile) {
  DeviceProfile fromHeader = deviceProfileFromHeader();
  EXPECT_EQ(device.buttonCount, fromHeader.buttonCount);
  EXPECT_STREQ(device.name, DEVICE_NAME);
}
