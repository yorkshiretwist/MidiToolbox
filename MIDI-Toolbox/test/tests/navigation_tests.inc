//----------------------------------------------------------------------------//
//    Tests for Navigation.h - the "pending cursor" behind deferred           //
//    Config / Mode / Bank / Patch / MIDI-channel navigation (see             //
//    docs/navigation.md). This is step 1 of the rollout: only the     //
//    pending state and the pure cursor-movement helpers exist, so every      //
//    test here asserts that moving the cursor changes ONLY the pending       //
//    values - the live selection and the MIDI output stay untouched until    //
//    an activation, which is a later step.                                   //
//----------------------------------------------------------------------------//

#include <filesystem>
#include <fstream>

// ---------------------------------------------------------------------------
// in-memory fixture (Mode / Bank / Patch / MidiChannel) - no filesystem
// ---------------------------------------------------------------------------

// M1: two banks (2 patches, then 1 patch).  M2: one bank (3 patches).
static const char *NAV_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "M1B1P1" }, { "name": "M1B1P2" } ] },
      { "name": "M1B2", "patches": [ { "name": "M1B2P1" } ] }
    ]},
    { "name": "M2", "banks": [
      { "name": "M2B1", "patches": [ { "name": "M2B1P1" }, { "name": "M2B1P2" }, { "name": "M2B1P3" } ] }
    ]}
  ]
})";

class NavigationTest : public ::testing::Test {
  protected:
    void SetUp() override {
      resetTestState();
    }

    // load a config and put the LIVE selection on Mode 0 / Bank 0 / whichever
    // Patch Bank 0 configures as its defaultPatchIndex (0 unless the tree
    // says otherwise - see ModeBank::resolveDefaultPatchIndex()), fire that
    // selection's default-on buttons (as a real config load would), then
    // drop the setup MIDI so assertions start from a clean log, with pending
    // synced to live. Mirrors resetRuntimeSelection() in ConfigStore.h.
    void loadTree(const char *json = NAV_TREE) {
      JsonDocument doc = parseJson(json);
      config = Config(doc);
      currentModeIndex = 0;
      currentBankIndex = 0;
      currentPatchIndex = (!config.modes.empty() && !config.modes[0].banks.empty())
          ? config.modes[0].banks[0].resolveDefaultPatchIndex() : -1;
      currentPage = 1;
      currentMidiChannel = 1;
      currentParameterIndex = 0;
      // mirror resetRuntimeSelection(): a multi-page landing patch drops the
      // selector onto Page, and every page's default-on buttons fire
      autoSelectPageParameterIfMultiPage();
      applyDefaultButtonStatuses();
      clearMidiLog();
      resetPendingToLive();
    }
};

TEST_F(NavigationTest, FreshlyLoaded_IsNotDirty) {
  loadTree();
  EXPECT_FALSE(navigationDirty());
  EXPECT_FALSE(navigationPatchDirty());
  EXPECT_FALSE(navigationChannelDirty());
}

TEST_F(NavigationTest, PendingCyclePatch_MovesPendingOnly_LeavesLiveAndMidiUntouched) {
  loadTree();

  pendingCyclePatch(CycleDirection::CycleUp);

  EXPECT_EQ(pendingPatchIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);   // live untouched
  EXPECT_TRUE(navigationDirty());
  EXPECT_TRUE(navigationPatchDirty());
  EXPECT_TRUE(midiLog().empty());    // nothing sent
}

TEST_F(NavigationTest, PendingCyclePatch_WrapsWithinTheBank_BothDirections) {
  loadTree();

  pendingCyclePatch(CycleDirection::CycleUp);   // 0 -> 1
  pendingCyclePatch(CycleDirection::CycleUp);   // 1 -> 0 (wrap, 2 patches)
  EXPECT_EQ(pendingPatchIndex, 0);

  pendingCyclePatch(CycleDirection::CycleDown); // 0 -> 1 (wrap the other way)
  EXPECT_EQ(pendingPatchIndex, 1);
}

TEST_F(NavigationTest, PendingCyclePatch_BackToTheLiveValue_ClearsDirty) {
  loadTree();

  pendingCyclePatch(CycleDirection::CycleUp);
  pendingCyclePatch(CycleDirection::CycleDown);

  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, PendingCyclePatch_IsANoOp_WhenTheBankHasASinglePatch) {
  loadTree();
  currentBankIndex = 1;   // M1B2 has one patch
  currentPatchIndex = 0;
  resetPendingToLive();

  pendingCyclePatch(CycleDirection::CycleUp);

  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, PendingCycleBank_ResetsPendingPatchToZeroOfThePendingBank) {
  loadTree();
  pendingCyclePatch(CycleDirection::CycleUp);   // pending patch -> 1

  pendingCycleBank(CycleDirection::CycleUp);    // -> M1B2

  EXPECT_EQ(pendingBankIndex, 1);
  EXPECT_EQ(pendingPatchIndex, 0);              // reset against M1B2
  EXPECT_EQ(currentBankIndex, 0);               // live untouched
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationTest, PendingCycleBank_ResetsPendingPatchToTheBanksDefaultPatchIndex) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "P1" } ] },
      { "name": "M1B2", "defaultPatchIndex": 2, "patches": [ { "name": "P1" }, { "name": "P2" }, { "name": "P3" } ] }
    ]}]
  })");

  pendingCycleBank(CycleDirection::CycleUp);    // -> M1B2

  EXPECT_EQ(pendingBankIndex, 1);
  EXPECT_EQ(pendingPatchIndex, 2);              // M1B2's configured default, not 0
}

TEST_F(NavigationTest, NavSetBank_LandsOnTheBanksDefaultPatchIndex) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "P1" } ] },
      { "name": "M1B2", "defaultPatchIndex": 1, "patches": [ { "name": "P1" }, { "name": "P2" } ] }
    ]}]
  })");

  navSetBank(1);

  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_EQ(currentPatchIndex, 1);
}

TEST_F(NavigationTest, NavSetBank_ClampsAnOutOfRangeDefaultPatchIndex) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "P1" } ] },
      { "name": "M1B2", "defaultPatchIndex": 50, "patches": [ { "name": "P1" }, { "name": "P2" } ] }
    ]}]
  })");

  navSetBank(1);

  EXPECT_EQ(currentPatchIndex, 1);              // clamped to the last patch
}

TEST_F(NavigationTest, NavSetBank_FiresTheDefaultOnButtonOfTheAutoSelectedPatch) {
  // M1B2's default patch is P2 (index 1), not P1 - its own defaultStatus-On
  // button (id 3) should fire, and P1's (id 2) should not, proving the
  // screen/LED state (Button.isOn, read by resolveButton() in both
  // applyDefaultButtonStatuses() and the screen button-grid draw in
  // Screen.h) tracks the auto-selected patch rather than assuming patch 0.
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "P1" } ] },
      { "name": "M1B2", "defaultPatchIndex": 1, "patches": [
        { "name": "P1", "buttons": [
          { "id": 2, "name": "p1On", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "velocity": 100, "midiChannel": 1 } } ] } ] },
        { "name": "P2", "buttons": [
          { "id": 3, "name": "p2On", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "velocity": 100, "midiChannel": 1 } } ] } ] }
      ] }
    ]}]
  })");

  navSetBank(1);

  ASSERT_EQ(currentPatchIndex, 1);              // landed on P2, the bank's default
  EXPECT_TRUE(resolveButton(3)->isOn);          // P2's default-on button fired/lit
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog().back().a, 40);            // E2 = MIDI note 40
}

TEST_F(NavigationTest, DefaultStatusOnButton_TargetingItsOwnDefaultPatch_DoesNotRecurseForever) {
  // Regression test: a bank whose defaultPatchIndex lands on a Patch whose
  // own defaultStatus:"On" button fires a SetPatch action targeting that
  // same Patch. Before this was guarded against, applyDefaultButtonStatuses()
  // -> turnButtonOn() -> the button's OnPress() -> navSetPatch() ->
  // applySelection() -> applyDefaultButtonStatuses() again resolved the
  // very same Button object, still not yet marked isOn (turnButtonOn used
  // to mark it on only AFTER running OnPress()), so it fired again - and
  // again, without end, overflowing the stack and crashing the device. If
  // either fix regresses (turnButtonOn's isOn-first ordering in Runtime.h,
  // or navSetPatch()'s already-selected guard in Navigation.h), this test
  // hangs/crashes the test binary instead of completing.
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "M1B1", "patches": [ { "name": "P1" } ] },
      { "name": "M1B2", "defaultPatchIndex": 1, "patches": [
        { "name": "P1" },
        { "name": "P2", "buttons": [
          { "id": 2, "name": "selfSelect", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [
              { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } },
              { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "velocity": 100, "midiChannel": 1 } }
            ] } ] }
      ] }
    ]}]
  })");

  navSetBank(1);

  ASSERT_EQ(currentPatchIndex, 1);              // landed on P2, the bank's default
  EXPECT_TRUE(resolveButton(2)->isOn);          // its own default-on button fired/lit
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog().back().a, 40);            // E2 = MIDI note 40, fired exactly once
}

// Regression coverage for two bugs a defaultStatus:"On" bank-level
// Exclusive SetPatch button (briefly shipped in "Live Rig.json"'s Pads mode,
// to keep a voicing-select button's LED lit for the bank's
// defaultPatchIndex) exposed together:
//
//   1. applyDefaultButtonStatuses() used to re-run in full on every patch
//      change within a bank, not just the first landing on it - correct for
//      a Patch-level "auto-hold this chord" button (it SHOULD reseed every
//      time you return to that Patch), wrong for a Bank-level one, which is
//      always in scope regardless of which Patch is active. Press a sibling
//      ("Low", SetPatch 0): the Exclusive-set logic turns "Middle" off
//      first, Low's own SetPatch(0) tears down and reapplies the selection,
//      and the reapply used to see "Middle" off and re-fire its SetPatch(1)
//      action - dragging the selection straight back to patch 1 regardless
//      of which button was actually pressed. Fixed by scoping the reapply:
//      a same-bank patch change now only re-seeds the incoming Patch's own
//      buttons (applyDefaultButtonStatuses(patchLevelOnly=true) - see
//      applySelection() in Navigation.h), so a Bank-level button seeded once
//      on the bank's own entry is left alone by every later sibling press.
//
//   2. Separately, an Exclusive/Latched button whose own action navigates
//      never used to end up visibly lit: turnButtonOn() marks it on BEFORE
//      running its actions (load-bearing for a different reentrancy reason
//      - see that function's own comment), but the action's own
//      applySelection() then ran turnOffAllActiveButtons(), which
//      unconditionally turned off every currently-on button - including the
//      one whose own press was still on the call stack. Fixed via
//      buttonBeingPressed (Runtime.h): triggerButton() records which id is
//      mid-press, and both teardown sweeps (turnOffAllActiveButtons() /
//      turnOffAllButtonsInPatch()) skip it.
TEST_F(NavigationTest, DefaultStatusOnBankLevelExclusiveButton_SeedsOnceAndSurvivesSiblingPresses) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "B1", "defaultPatchIndex": 1, "maintainActiveButtonOnPatchChange": true,
        "buttons": [
          { "id": 5, "name": "Low", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 0 } } ] },
          { "id": 6, "name": "Middle", "buttonMode": "Exclusive", "defaultStatus": "On", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] },
          { "id": 7, "name": "Wide", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 2 } } ] },
          { "id": 8, "name": "High", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 3 } } ] }
        ],
        "patches": [ { "name": "P0" }, { "name": "P1" }, { "name": "P2" }, { "name": "P3" } ] }
    ]}]
  })");
  // loadTree() lands on Bank 0 already, and applyDefaultButtonStatuses()
  // (which it also calls) finds "Middle" in scope immediately - a Bank-level
  // button is never patch-scoped, so it doesn't need an explicit navSetBank()
  // to be seeded.
  ASSERT_EQ(currentPatchIndex, 1);
  ASSERT_TRUE(resolveButton(6)->isOn);

  triggerButton(5);   // Low
  EXPECT_EQ(currentPatchIndex, 0) << "Low should select patch 0, not get hijacked back to Middle's";
  EXPECT_TRUE(resolveButton(5)->isOn)   << "Low should end up lit - it's the live selection";
  EXPECT_FALSE(resolveButton(6)->isOn)  << "Middle should no longer be lit";

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

// The same Exclusive buttonSet and defaultPatchIndex, but WITHOUT
// defaultStatus:"On" on any bank-level SetPatch button. Every sibling still
// correctly selects its own patch AND ends up lit - this is the
// buttonBeingPressed fix (bug 2 above) working on its own, independent of
// the applyDefaultButtonStatuses scoping fix (bug 1), since there's no
// defaultStatus:"On" button here to hijack anything.
TEST_F(NavigationTest, ExclusiveBankLevelSetPatchButtons_EachSelectsAndLightsItsOwnPatch) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "B0", "patches": [ { "name": "OnlyPatch" } ] },
      { "name": "B1", "defaultPatchIndex": 1, "maintainActiveButtonOnPatchChange": true,
        "buttons": [
          { "id": 5, "name": "Low", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 0 } } ] },
          { "id": 6, "name": "Middle", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] },
          { "id": 7, "name": "Wide", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 2 } } ] },
          { "id": 8, "name": "High", "buttonMode": "Exclusive", "buttonSet": [5,6,7,8],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 3 } } ] }
        ],
        "patches": [ { "name": "P0" }, { "name": "P1" }, { "name": "P2" }, { "name": "P3" } ] }
    ]}]
  })");
  navSetBank(1);
  ASSERT_EQ(currentPatchIndex, 1);   // defaultPatchIndex still honoured

  triggerButton(5);   // Low
  EXPECT_EQ(currentPatchIndex, 0) << "Low should select patch 0, not get hijacked elsewhere";
  EXPECT_TRUE(resolveButton(5)->isOn);

  triggerButton(7);   // Wide
  EXPECT_EQ(currentPatchIndex, 2) << "Wide should select patch 2, not get hijacked elsewhere";
  EXPECT_TRUE(resolveButton(7)->isOn);
  EXPECT_FALSE(resolveButton(5)->isOn);

  triggerButton(8);   // High
  EXPECT_EQ(currentPatchIndex, 3) << "High should select patch 3, not get hijacked elsewhere";
  EXPECT_TRUE(resolveButton(8)->isOn);

  triggerButton(6);   // back to Middle
  EXPECT_EQ(currentPatchIndex, 1) << "Middle should select patch 1, not get hijacked elsewhere";
  EXPECT_TRUE(resolveButton(6)->isOn);
  EXPECT_FALSE(resolveButton(8)->isOn);
}

// Isolates bug 2 (see the block comment above) from Exclusive-set / bank-
// level specifics entirely: a plain button whose own action navigates
// should end up lit, whether it's Exclusive or Latched, and whether or not
// it shares a buttonSet with anything.
TEST_F(NavigationTest, ButtonWhoseActionNavigates_EndsUpLit_ExclusiveAndLatched) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "banks": [
      { "name": "B1", "buttons": [
          { "id": 5, "name": "Low", "buttonMode": "Exclusive", "buttonSet": [5,6],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] },
          { "id": 6, "name": "Middle", "buttonMode": "Exclusive", "buttonSet": [5,6],
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 0 } } ] },
          { "id": 9, "name": "Latch", "buttonMode": "Latched",
            "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] }
        ],
        "patches": [ { "name": "P0" }, { "name": "P1" } ] }
    ]}]
  })");

  triggerButton(5);   // Low (Exclusive) - selects patch 1
  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_TRUE(resolveButton(5)->isOn) << "Low should end up lit - it's the live selection";

  triggerButton(6);   // Middle (Exclusive) - back to patch 0
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(resolveButton(6)->isOn);
  EXPECT_FALSE(resolveButton(5)->isOn);

  triggerButton(9);   // Latch (Latched) - to patch 1
  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_TRUE(resolveButton(9)->isOn) << "a Latched navigating button should end up lit too";
}

TEST_F(NavigationTest, PendingCycleBank_WrapsWithinTheMode) {
  loadTree();

  pendingCycleBank(CycleDirection::CycleUp);    // 0 -> 1
  pendingCycleBank(CycleDirection::CycleUp);    // 1 -> 0 (wrap, 2 banks)

  EXPECT_EQ(pendingBankIndex, 0);
}

TEST_F(NavigationTest, PendingCycleMode_ResetsPendingBankAndPatch_AgainstThePendingMode) {
  loadTree();
  pendingCyclePatch(CycleDirection::CycleUp);   // dirty the patch first

  pendingCycleMode(CycleDirection::CycleUp);    // -> M2

  EXPECT_EQ(pendingModeIndex, 1);
  EXPECT_EQ(pendingBankIndex, 0);
  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_EQ(currentModeIndex, 0);               // live untouched
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(navigationDirty());
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationTest, PendingCycleMode_ThenPendingCyclePatch_WrapsWithinTheNewMode) {
  loadTree();

  pendingCycleMode(CycleDirection::CycleUp);    // -> M2 (bank 0, 3 patches)
  pendingCyclePatch(CycleDirection::CycleUp);   // 0 -> 1
  pendingCyclePatch(CycleDirection::CycleUp);   // 1 -> 2
  pendingCyclePatch(CycleDirection::CycleUp);   // 2 -> 0 (wrap at 3)

  EXPECT_EQ(pendingPatchIndex, 0);
}

TEST_F(NavigationTest, PendingCycleMode_WrapsBothDirections) {
  loadTree();

  pendingCycleMode(CycleDirection::CycleUp);    // 0 -> 1
  pendingCycleMode(CycleDirection::CycleUp);    // 1 -> 0 (wrap, 2 modes)
  EXPECT_EQ(pendingModeIndex, 0);

  pendingCycleMode(CycleDirection::CycleDown);  // 0 -> 1 (wrap the other way)
  EXPECT_EQ(pendingModeIndex, 1);
}

TEST_F(NavigationTest, PendingCycleMode_BackToTheLiveMode_RestoresLiveBankAndPatch_ClearingDirty) {
  loadTree();
  currentBankIndex = 0;
  currentPatchIndex = 1;                        // live: M1 / M1B1 / M1B1P2
  resetPendingToLive();
  ASSERT_FALSE(navigationDirty());

  pendingCycleMode(CycleDirection::CycleUp);    // -> M2 (pending bank/patch reset to 0)
  EXPECT_TRUE(navigationDirty());
  EXPECT_EQ(pendingPatchIndex, 0);

  pendingCycleMode(CycleDirection::CycleUp);    // -> back to M1 (2 modes, wraps)

  EXPECT_EQ(pendingModeIndex, currentModeIndex);
  EXPECT_EQ(pendingBankIndex, currentBankIndex);    // restored, not left at 0
  EXPECT_EQ(pendingPatchIndex, currentPatchIndex);
  EXPECT_FALSE(navigationDirty());                  // no pending change -> "Set?" clears
}

TEST_F(NavigationTest, PendingCycleBank_BackToTheLiveBank_RestoresLivePatch_ClearingDirty) {
  loadTree();
  currentBankIndex = 0;
  currentPatchIndex = 1;                        // live: M1 / M1B1 / M1B1P2
  resetPendingToLive();

  pendingCycleBank(CycleDirection::CycleUp);    // -> M1B2 (pending patch reset)
  EXPECT_TRUE(navigationDirty());

  pendingCycleBank(CycleDirection::CycleUp);    // -> back to M1B1 (2 banks, wraps)

  EXPECT_EQ(pendingBankIndex, currentBankIndex);
  EXPECT_EQ(pendingPatchIndex, currentPatchIndex);  // restored to 1
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, PendingNavigation_NeverMovesTheParameterSelector) {
  loadTree();
  currentParameterIndex = 2;

  pendingCyclePatch(CycleDirection::CycleUp);
  pendingCycleBank(CycleDirection::CycleUp);
  pendingCycleMode(CycleDirection::CycleUp);
  pendingCycleMidiChannel(CycleDirection::CycleUp);

  EXPECT_EQ(currentParameterIndex, 2);
}

TEST_F(NavigationTest, PendingCycleMidiChannel_MovesPendingOnly_AndWraps) {
  loadTree();

  pendingCycleMidiChannel(CycleDirection::CycleUp);
  EXPECT_EQ(pendingMidiChannel, 2);
  EXPECT_EQ(currentMidiChannel, 1);
  EXPECT_TRUE(navigationChannelDirty());
  EXPECT_TRUE(navigationDirty());

  pendingMidiChannel = 16;
  pendingCycleMidiChannel(CycleDirection::CycleUp);
  EXPECT_EQ(pendingMidiChannel, 1);            // wrapped past 16
  pendingCycleMidiChannel(CycleDirection::CycleDown);
  EXPECT_EQ(pendingMidiChannel, 16);          // wrapped past 1
}

TEST_F(NavigationTest, ChannelDirty_DoesNotFlashTheHierarchyLabels) {
  loadTree();

  pendingCycleMidiChannel(CycleDirection::CycleUp);

  EXPECT_TRUE(navigationChannelDirty());
  EXPECT_FALSE(navigationPatchDirty());
  EXPECT_FALSE(navigationBankDirty());
  EXPECT_FALSE(navigationModeDirty());
  EXPECT_FALSE(navigationConfigDirty());
}

TEST_F(NavigationTest, BankDirty_FlashesBankAndPatch_ButNotMode) {
  loadTree();

  pendingCycleBank(CycleDirection::CycleUp);

  EXPECT_TRUE(navigationBankDirty());
  EXPECT_TRUE(navigationPatchDirty());   // cascades down
  EXPECT_FALSE(navigationModeDirty());   // but not up
  EXPECT_FALSE(navigationConfigDirty());
}

TEST_F(NavigationTest, PendingSetMode_ByName_LandsAndResetsBankPatch_RejectsUnknown) {
  loadTree();

  EXPECT_TRUE(pendingSetMode("M2"));
  EXPECT_EQ(pendingModeIndex, 1);
  EXPECT_EQ(pendingBankIndex, 0);
  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_EQ(currentModeIndex, 0);       // live untouched

  EXPECT_FALSE(pendingSetMode("Nope"));
  EXPECT_EQ(pendingModeIndex, 1);       // unchanged by the failed call
}

TEST_F(NavigationTest, PendingSetBank_ByName_RejectsUnknown) {
  loadTree();

  EXPECT_TRUE(pendingSetBank("M1B2"));
  EXPECT_EQ(pendingBankIndex, 1);
  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);

  EXPECT_FALSE(pendingSetBank("Nope"));
  EXPECT_EQ(pendingBankIndex, 1);
}

TEST_F(NavigationTest, PendingSetPatch_ByName_RejectsUnknown) {
  loadTree();

  EXPECT_TRUE(pendingSetPatch("M1B1P2"));
  EXPECT_EQ(pendingPatchIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);

  EXPECT_FALSE(pendingSetPatch("Nope"));
  EXPECT_EQ(pendingPatchIndex, 1);
}

TEST_F(NavigationTest, PendingSetMidiChannel_TakesValidValue_RejectsOutOfRange) {
  loadTree();

  pendingSetMidiChannel(9);
  EXPECT_EQ(pendingMidiChannel, 9);
  EXPECT_EQ(currentMidiChannel, 1);

  pendingSetMidiChannel(0);
  pendingSetMidiChannel(17);
  EXPECT_EQ(pendingMidiChannel, 9);   // both rejected, unchanged
}

TEST_F(NavigationTest, ResetPendingToLive_ClearsDirty) {
  loadTree();
  pendingCycleMode(CycleDirection::CycleUp);
  pendingCycleMidiChannel(CycleDirection::CycleUp);
  ASSERT_TRUE(navigationDirty());

  resetPendingToLive();

  EXPECT_FALSE(navigationDirty());
  EXPECT_EQ(pendingModeIndex, currentModeIndex);
  EXPECT_EQ(pendingBankIndex, currentBankIndex);
  EXPECT_EQ(pendingPatchIndex, currentPatchIndex);
  EXPECT_EQ(pendingMidiChannel, currentMidiChannel);
}

// ---------------------------------------------------------------------------
// activation (applySelection / activateSelection) - same-config paths
// ---------------------------------------------------------------------------

// note numbers: C2=36, E2=40, G2=43 (all channel 1), C4=60 (channel 2)
//
//   M1 / M1B1 / P1 : button 1, Latched, plays C2      (player latches this)
//   M1 / M1B1 / P2 : button 2, defaultStatus On, E2
//   M1 / M1B2 / P3 : button 3, defaultStatus On, G2
//   M2 / M2B1 / MP1: button 4, defaultStatus On, C4 on channel 2
static const char *ACTIVATION_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1", "banks": [
      { "name": "M1B1", "patches": [
        { "name": "P1", "buttons": [
          { "id": 1, "name": "hold", "buttonMode": "Latched",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "midiChannel": 1 } } ] }
        ]},
        { "name": "P2", "buttons": [
          { "id": 2, "name": "p2", "buttonMode": "Momentary", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "midiChannel": 1 } } ] }
        ]}
      ]},
      { "name": "M1B2", "patches": [
        { "name": "P3", "buttons": [
          { "id": 3, "name": "p3", "buttonMode": "Momentary", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["G2"], "midiChannel": 1 } } ] }
        ]}
      ]}
    ]},
    { "name": "M2", "banks": [
      { "name": "M2B1", "patches": [
        { "name": "MP1", "buttons": [
          { "id": 4, "name": "m2", "buttonMode": "Momentary", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C4"], "midiChannel": 2 } } ] }
        ]}
      ]}
    ]}
  ]
})";

TEST_F(NavigationTest, Activate_WhenNothingIsPending_IsANoOp) {
  loadTree(ACTIVATION_TREE);

  activateSelection();

  EXPECT_TRUE(midiLog().empty());
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, Activate_PendingPatch_StopsHeldNotesThenFiresTheNewDefaults) {
  loadTree(ACTIVATION_TREE);        // live M1 / M1B1 / P1

  triggerButton(1);                 // player latches P1 -> C2 sounds
  ASSERT_EQ(midiLog().size(), 1u);
  ASSERT_EQ(midiLog()[0].kind, MidiCall::NoteOn);
  ASSERT_EQ(midiLog()[0].a, 36);

  clearMidiLog();
  pendingCyclePatch(CycleDirection::CycleUp);   // navigate to P2 - nothing yet
  EXPECT_TRUE(midiLog().empty());
  EXPECT_EQ(currentPatchIndex, 0);

  activateSelection();

  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_FALSE(navigationDirty());
  ASSERT_EQ(midiLog().size(), 2u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);   // C2 off first (teardown)
  EXPECT_EQ(midiLog()[0].a, 36);
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);    // then P2's default: E2
  EXPECT_EQ(midiLog()[1].a, 40);
}

TEST_F(NavigationTest, Activate_PendingBank_LandsOnBankZeroPatchZero_WithTeardownFirst) {
  loadTree(ACTIVATION_TREE);
  triggerButton(1);                 // latch C2
  clearMidiLog();

  pendingCycleBank(CycleDirection::CycleUp);    // -> M1B2 (pending)
  activateSelection();

  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);
  ASSERT_EQ(midiLog().size(), 2u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);
  EXPECT_EQ(midiLog()[0].a, 36);                // C2 off
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[1].a, 43);                // P3's default: G2
}

TEST_F(NavigationTest, Activate_PendingMode_LandsOnBankZeroPatchZero) {
  loadTree(ACTIVATION_TREE);
  triggerButton(1);
  clearMidiLog();

  pendingCycleMode(CycleDirection::CycleUp);    // -> M2
  activateSelection();

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  ASSERT_EQ(midiLog().size(), 2u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);
  EXPECT_EQ(midiLog()[0].a, 36);                // C2 off
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[1].a, 60);                // MP1's default: C4
  EXPECT_EQ(midiLog()[1].channel, 2);
}

TEST_F(NavigationTest, Activate_PreservesTheParameterSelector) {
  loadTree(ACTIVATION_TREE);
  currentParameterIndex = 3;

  pendingCyclePatch(CycleDirection::CycleUp);
  activateSelection();

  EXPECT_EQ(currentParameterIndex, 3);   // the regression that started the redesign
}

TEST_F(NavigationTest, Activate_ResetsPendingToLive) {
  loadTree(ACTIVATION_TREE);
  pendingCycleMode(CycleDirection::CycleUp);
  pendingCycleMidiChannel(CycleDirection::CycleUp);
  ASSERT_TRUE(navigationDirty());

  activateSelection();

  EXPECT_FALSE(navigationDirty());
  EXPECT_EQ(pendingModeIndex, currentModeIndex);
  EXPECT_EQ(pendingBankIndex, currentBankIndex);
  EXPECT_EQ(pendingPatchIndex, currentPatchIndex);
  EXPECT_EQ(pendingMidiChannel, currentMidiChannel);
}

TEST_F(NavigationTest, Activate_ChannelOnly_AppliesChannelWithoutStoppingHeldNotes) {
  loadTree(ACTIVATION_TREE);
  triggerButton(1);                 // latch C2 - must keep sounding
  clearMidiLog();

  pendingCycleMidiChannel(CycleDirection::CycleUp);   // pending channel 2
  activateSelection();

  EXPECT_EQ(currentMidiChannel, 2);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(midiLog().empty());    // no teardown, no re-fired defaults
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, Activate_PatchAndChannelTogether_SwitchesPatchAndCarriesTheChannel) {
  loadTree(ACTIVATION_TREE);

  pendingCyclePatch(CycleDirection::CycleUp);         // -> P2
  pendingCycleMidiChannel(CycleDirection::CycleUp);   // channel 2
  activateSelection();

  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_EQ(currentMidiChannel, 2);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, Activate_AfterRapidNavigation_AppliesOnlyTheFinalDestination) {
  loadTree(ACTIVATION_TREE);
  triggerButton(1);
  clearMidiLog();

  pendingCyclePatch(CycleDirection::CycleUp);   // P1 -> P2
  pendingCycleBank(CycleDirection::CycleUp);    // -> M1B2 / P3
  pendingCycleMode(CycleDirection::CycleUp);    // -> M2 / M2B1 / MP1
  activateSelection();

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);
  // exactly one teardown (C2 off) and one defaults burst (C4 on) - no
  // intermediate patches were ever loaded
  ASSERT_EQ(midiLog().size(), 2u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);
  EXPECT_EQ(midiLog()[0].a, 36);
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[1].a, 60);
}

// ---------------------------------------------------------------------------
// the Action classes route through Navigation.h now
//
//   Cycle*Action  -> pending cursor (deferred, flashes, needs activation)
//   Set*Action    -> applySelection() immediately
//   ActivateSelectionAction -> commits the pending cursor
// ---------------------------------------------------------------------------

// Mode-level buttons (resolve from any Bank/Patch): 5 CyclePatch+, 6
// CycleBank+, 7 CycleMode+, 8 SetPatch(1), 9 ActivateSelection,
// 4 CycleParameterValue+.
static const char *NAV_ACTION_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1",
      "parameters": ["Patch"],
      "buttons": [
        { "id": 5, "name": "p+", "buttonMode": "Momentary",
          "actions": [ { "type": "CyclePatch", "actionParameters": { "direction": "Up" } } ] },
        { "id": 6, "name": "b+", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleBank", "actionParameters": { "direction": "Up" } } ] },
        { "id": 7, "name": "m+", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleMode", "actionParameters": { "direction": "Up" } } ] },
        { "id": 8, "name": "sp1", "buttonMode": "Momentary",
          "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] },
        { "id": 9, "name": "go", "buttonMode": "Momentary",
          "actions": [ { "type": "ActivateSelection" } ] },
        { "id": 4, "name": "v+", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleParameterValue", "actionParameters": { "direction": "Up" } } ] }
      ],
      "banks": [
        { "name": "M1B1", "patches": [ { "name": "P1" }, { "name": "P2" } ] },
        { "name": "M1B2", "patches": [ { "name": "P3" } ] }
      ]
    },
    { "name": "M2", "banks": [ { "name": "M2B1", "patches": [ { "name": "MP1" } ] } ] }
  ]
})";

TEST_F(NavigationTest, CyclePatchAction_MovesPendingPatchOnly) {
  loadTree(NAV_ACTION_TREE);        // live M1 / M1B1 / P1

  triggerButton(5);                 // CyclePatch Up

  EXPECT_EQ(pendingPatchIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);  // live untouched
  EXPECT_TRUE(navigationDirty());
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationTest, CycleBankAction_MovesPendingBankOnly_ResettingPendingPatch) {
  loadTree(NAV_ACTION_TREE);

  triggerButton(6);                 // CycleBank Up

  EXPECT_EQ(pendingBankIndex, 1);
  EXPECT_EQ(pendingPatchIndex, 0);  // reset against the pending bank
  EXPECT_EQ(currentBankIndex, 0);   // live untouched
  EXPECT_TRUE(navigationDirty());
}

TEST_F(NavigationTest, CycleModeAction_MovesPendingModeOnly) {
  loadTree(NAV_ACTION_TREE);

  triggerButton(7);                 // CycleMode Up

  EXPECT_EQ(pendingModeIndex, 1);
  EXPECT_EQ(currentModeIndex, 0);   // live untouched
  EXPECT_TRUE(navigationDirty());
}

TEST_F(NavigationTest, CycleParameterValueAction_MovesPendingValueOfTheSelectedParameter) {
  loadTree(NAV_ACTION_TREE);        // mode parameters = ["Patch"], selector on Patch

  triggerButton(4);                 // CycleParameterValue Up -> pending Patch

  EXPECT_EQ(pendingPatchIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(navigationDirty());
}

TEST_F(NavigationTest, CycleAction_ThenActivateSelectionAction_Commits) {
  loadTree(NAV_ACTION_TREE);

  triggerButton(7);                 // navigate: pending mode -> M2
  triggerButton(9);                 // ActivateSelection

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetPatchAction_IsImmediate_NotDeferred) {
  loadTree(NAV_ACTION_TREE);

  triggerButton(8);                 // SetPatch patchIndex 1

  EXPECT_EQ(currentPatchIndex, 1);  // applied at once
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetPatchAction_SupersedesAPendingNavigation) {
  loadTree(NAV_ACTION_TREE);
  triggerButton(6);                 // navigate: pending bank -> M1B2
  ASSERT_TRUE(navigationDirty());

  triggerButton(8);                 // SetPatch 1 (immediate)

  EXPECT_EQ(currentBankIndex, 0);   // the pending bank move was discarded
  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_FALSE(navigationDirty());
}

// ---------------------------------------------------------------------------
// SetBankAction - a direct Set: applies at once (via navSetBank ->
// applySelection), landing on the target bank's resolveDefaultPatchIndex(),
// never leaving pending state behind. id 0 -> bank 1, id 1 -> bank 0,
// id 2 -> bank 5 (out of range). B0's landing patch carries a default-on
// button so the "already there" no-op can be checked by an empty MIDI log.
// ---------------------------------------------------------------------------
static const char *SETBANK_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1",
      "buttons": [
        { "id": 0, "name": "toB1", "buttonMode": "Momentary",
          "actions": [ { "type": "SetBank", "actionParameters": { "bankIndex": 1 } } ] },
        { "id": 1, "name": "toB0", "buttonMode": "Momentary",
          "actions": [ { "type": "SetBank", "actionParameters": { "bankIndex": 0 } } ] },
        { "id": 2, "name": "toGhost", "buttonMode": "Momentary",
          "actions": [ { "type": "SetBank", "actionParameters": { "bankIndex": 5 } } ] }
      ],
      "banks": [
        { "name": "B0", "patches": [
          { "name": "B0P1", "buttons": [
            { "id": 9, "name": "seed", "buttonMode": "Latched", "defaultStatus": "On",
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 7 } } ] }
          ]},
          { "name": "B0P2" }
        ]},
        { "name": "B1", "defaultPatchIndex": 1, "patches": [ { "name": "B1P1" }, { "name": "B1P2" } ] }
      ]
    }
  ]
})";

TEST_F(NavigationTest, SetBankAction_SelectsTheBankImmediately_LandingOnItsDefaultPatch) {
  loadTree(SETBANK_TREE);           // live M1 / B0 / P0

  triggerButton(0);                 // SetBank bankIndex 1

  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_EQ(currentPatchIndex, 1);  // B1's defaultPatchIndex
  EXPECT_EQ(pendingBankIndex, 1);   // immediate: pending stays synced to live
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetBankAction_OutOfRangeIndex_IsIgnored) {
  loadTree(SETBANK_TREE);

  triggerButton(2);                 // SetBank bankIndex 5 - only two banks exist

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetBankAction_TheLiveBankOnItsDefaultPatch_ResetsIt) {
  loadTree(SETBANK_TREE);           // live B0 / P0 (B0's default patch); id 9 default-on already fired, log cleared

  triggerButton(1);                 // SetBank bankIndex 0 - already there

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  // reset: the default-on button was torn down and seeded again
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::ProgramChange);
  EXPECT_EQ(midiLog().back().a, 7);
  EXPECT_TRUE(resolveButton(9)->isOn);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetBankAction_TheLiveBankFromAnotherPatch_GoesHomeToItsDefaultPatch) {
  loadTree(SETBANK_TREE);
  navSetPatch(1);                   // B0's second patch
  ASSERT_EQ(currentPatchIndex, 1);

  triggerButton(1);                 // SetBank bankIndex 0 - the bank we are in

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);  // back to B0's default patch
}

TEST_F(NavigationTest, SetBankAction_ADefaultOnButtonSelectingItsOwnBank_DoesNotRecurse) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [ { "name": "M1", "banks": [ { "name": "B1", "patches": [
      { "name": "P1", "buttons": [
        { "id": 2, "name": "selfBank", "buttonMode": "Latched", "defaultStatus": "On",
          "actions": [
            { "type": "SetBank", "actionParameters": { "bankIndex": 0 } },
            { "type": "ProgramChange", "actionParameters": { "programNumber": 5, "midiChannel": 1 } }
          ] } ] }
    ] } ] } ]
  })");
  int programChanges = 0;
  for (const MidiCall &call : midiLog()) if (call.kind == MidiCall::ProgramChange) programChanges++;
  // loadTree() cleared the log after seeding; fire a deliberate reset and count
  clearMidiLog();

  navSetBank(0);

  for (const MidiCall &call : midiLog()) if (call.kind == MidiCall::ProgramChange) programChanges++;
  EXPECT_EQ(programChanges, 1);     // the button fired exactly once, not in a loop
  EXPECT_EQ(selectionPassDepth, 0);
}

// ---------------------------------------------------------------------------
// button input routing (performanceButtonPress / resolveButton10 / ...)
// ---------------------------------------------------------------------------

// id 0: PlayNotes only (pure performance).  id 1: SetMode + PlayNotes
// (carries immediate navigation).  id 2: CyclePatch (a navigate verb on a
// 0-9 button).  id 3: SetParameterValue while the selector is on MidiChannel
// (a Set, but not of a hierarchy level).
static const char *NAV_INPUT_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1",
      "parameters": ["MidiChannel"],
      "buttons": [
        { "id": 0, "name": "note", "buttonMode": "Momentary",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] },
        { "id": 1, "name": "setmode+note", "buttonMode": "Momentary",
          "actions": [
            { "type": "SetMode", "actionParameters": { "modeIndex": 1 } },
            { "type": "PlayNotes", "actionParameters": { "noteNames": ["D3"], "midiChannel": 1 } }
          ] },
        { "id": 2, "name": "cyclepatch", "buttonMode": "Momentary",
          "actions": [ { "type": "CyclePatch", "actionParameters": { "direction": "Up" } } ] },
        { "id": 3, "name": "setchan", "buttonMode": "Momentary",
          "actions": [ { "type": "SetParameterValue", "actionParameters": { "parameterValue": "5" } } ] }
      ],
      "banks": [ { "name": "M1B1", "patches": [ { "name": "P1" }, { "name": "P2" } ] } ]
    },
    { "name": "M2", "banks": [ { "name": "M2B1", "patches": [ { "name": "MP1" } ] } ] }
  ]
})";

TEST_F(NavigationTest, ButtonIsPurePerformance_TrueOnlyWhenEveryActionIsPlayNotesPcOrCc) {
  loadTree(NAV_INPUT_TREE);
  EXPECT_TRUE(buttonIsPurePerformance(*resolveButton(0)));  // PlayNotes only
  EXPECT_FALSE(buttonIsPurePerformance(*resolveButton(1))); // SetMode + PlayNotes
  EXPECT_FALSE(buttonIsPurePerformance(*resolveButton(2))); // CyclePatch (navigate verb)
  EXPECT_FALSE(buttonIsPurePerformance(*resolveButton(3))); // SetParameterValue
}

TEST_F(NavigationTest, PerformanceButtonPress_PurePerformanceButton_CommitsPendingFirstThenRuns) {
  loadTree(NAV_INPUT_TREE);
  triggerButton(2);                     // CyclePatch -> pending patch 1
  ASSERT_TRUE(navigationDirty());
  clearMidiLog();

  performanceButtonPress(0, "physical"); // PlayNotes only

  EXPECT_EQ(currentPatchIndex, 1);       // pending committed
  EXPECT_FALSE(navigationDirty());
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog().back().a, 48);     // C3, on the now-live patch
}

TEST_F(NavigationTest, PerformanceButtonPress_ButtonWithItsOwnSet_SupersedesPendingInsteadOfCommittingIt) {
  loadTree(NAV_INPUT_TREE);
  triggerButton(2);                     // CyclePatch -> pending patch 1
  ASSERT_TRUE(navigationDirty());

  performanceButtonPress(1, "physical"); // SetMode(1) + PlayNotes

  EXPECT_EQ(currentModeIndex, 1);        // its own SetMode won
  EXPECT_EQ(currentPatchIndex, 0);       // pending patch move discarded (M2/patch 0)
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, PerformanceButtonPress_CycleVerbButton_StepsThePendingChangeWithoutCommitting) {
  loadTree(NAV_INPUT_TREE);              // M1 / M1B1 (2 patches) / P1
  triggerButton(2);                     // CyclePatch -> pending patch 1
  ASSERT_TRUE(navigationDirty());
  clearMidiLog();

  performanceButtonPress(2, "physical"); // CyclePatch again - must NOT pre-commit

  EXPECT_EQ(currentPatchIndex, 0);       // live never moved to 1 - nothing committed
  EXPECT_TRUE(midiLog().empty());        // no teardown, no re-fired defaults
  EXPECT_EQ(pendingPatchIndex, 0);       // pending stepped again (2 patches -> wrapped)
  EXPECT_FALSE(navigationDirty());       // ... and landed back on live, so it's clean
}

TEST_F(NavigationTest, PerformanceButtonPress_NotDirty_JustRunsTheButton) {
  loadTree(NAV_INPUT_TREE);
  clearMidiLog();

  performanceButtonPress(0, "physical");

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[0].a, 48);
}

TEST_F(NavigationTest, PerformanceButtonPress_UnboundId_DoesNothing) {
  loadTree(NAV_INPUT_TREE);
  clearMidiLog();

  performanceButtonPress(7, "physical");   // nothing bound to id 7

  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationTest, ResolveButton10_TapAdvancesWhenClean_ActivatesWhenPending_RevertsOnLongPress) {
  EXPECT_EQ(resolveButton10(50, false),   Button10Advance);
  EXPECT_EQ(resolveButton10(5000, false), Button10Advance);   // clean: hold length irrelevant
  EXPECT_EQ(resolveButton10(50, true),    Button10Activate);
  EXPECT_EQ(resolveButton10(499, true),   Button10Activate);
  EXPECT_EQ(resolveButton10(500, true),   Button10Revert);
  EXPECT_EQ(resolveButton10(2000, true),  Button10Revert);
}

TEST_F(NavigationTest, NavButton10Completed_NotDirty_AdvancesTheParameterSelector) {
  loadTree(NAV_TREE);                    // no "parameters" -> full default set (Patch..Config, Page)
  ASSERT_EQ(currentParameterIndex, 0);

  navButton10Completed(50);

  EXPECT_EQ(currentParameterIndex, 1);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, NavButton10Completed_DirtyQuickTap_ActivatesThePending) {
  loadTree(NAV_TREE);
  pendingCyclePatch(CycleDirection::CycleUp);
  ASSERT_TRUE(navigationDirty());

  navButton10Completed(50);

  EXPECT_FALSE(navigationDirty());
  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_EQ(currentParameterIndex, 0);  // selector NOT advanced when it was a commit
}

TEST_F(NavigationTest, NavButton10Completed_DirtyLongPress_RevertsToLive) {
  loadTree(NAV_TREE);
  pendingCycleMode(CycleDirection::CycleUp);
  ASSERT_TRUE(navigationDirty());

  navButton10Completed(600);

  EXPECT_FALSE(navigationDirty());
  EXPECT_EQ(currentModeIndex, 0);       // live untouched
  EXPECT_EQ(pendingModeIndex, 0);       // pending reverted
}

TEST_F(NavigationTest, NavStepValue_StepsThePendingValueOfTheSelectedParameter) {
  loadTree(NAV_TREE);                    // selector index 0 = Patch (default set)

  navStepValue(CycleDirection::CycleUp);

  EXPECT_EQ(pendingPatchIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(navigationDirty());
}

// ---------------------------------------------------------------------------
// readouts for Screen.h (label / arrows / preview names)
// ---------------------------------------------------------------------------

TEST_F(NavigationTest, Button10Label_ShowsTheSelectedParameter_WhenClean) {
  loadTree(NAV_TREE);   // no "parameters" -> default set, selector at 0 = Patch
  EXPECT_EQ(navigationButton10Label(), "Patch");

  navButton10Completed(50);   // advance the selector -> Bank
  EXPECT_EQ(navigationButton10Label(), "Bank");
}

TEST_F(NavigationTest, Button10Label_ShowsSetPrompt_WhileAChangeIsPending) {
  loadTree(NAV_TREE);
  pendingCyclePatch(CycleDirection::CycleUp);

  EXPECT_EQ(navigationButton10Label(), "Set?");
}

TEST_F(NavigationTest, ValueCount_ReflectsTheSelectedParameter) {
  loadTree(NAV_TREE);         // selector 0 = Patch; M1B1 has 2 patches
  EXPECT_EQ(navigationValueCount(), 2);

  navButton10Completed(50);   // -> Bank; M1 has 2 banks
  EXPECT_EQ(navigationValueCount(), 2);

  navButton10Completed(50);   // -> Mode; 2 modes
  EXPECT_EQ(navigationValueCount(), 2);

  navButton10Completed(50);   // -> MidiChannel
  EXPECT_EQ(navigationValueCount(), 16);
}

TEST_F(NavigationTest, PreviewNames_TrackThePendingCursor) {
  loadTree(NAV_TREE);         // live M1 / M1B1 / M1B1P1

  pendingCyclePatch(CycleDirection::CycleUp);
  EXPECT_EQ(navigationPreviewPatchName(), "M1B1P2");
  EXPECT_EQ(navigationPreviewModeName(), "M1");   // mode isn't dirty

  resetPendingToLive();
  pendingCycleMode(CycleDirection::CycleUp);      // -> M2 / M2B1 / M2B1P1
  EXPECT_EQ(navigationPreviewModeName(), "M2");
  EXPECT_EQ(navigationPreviewBankName(), "M2B1");
  EXPECT_EQ(navigationPreviewPatchName(), "M2B1P1");
}

TEST_F(NavigationTest, PreviewChannel_IsThePendingChannel) {
  loadTree(NAV_TREE);
  pendingCycleMidiChannel(CycleDirection::CycleUp);

  EXPECT_EQ(navigationPreviewChannel(), 2);
}

TEST_F(NavigationTest, PreviewName_ReturnsTheLiveValue_AfterAValueRoundTripClearsDirty) {
  loadTree(NAV_TREE);                        // live M1 / M1B1 / M1B1P1

  navStepValue(CycleDirection::CycleUp);     // pending patch -> M1B1P2
  ASSERT_TRUE(navigationPatchDirty());
  EXPECT_EQ(navigationPreviewPatchName(), "M1B1P2");

  navStepValue(CycleDirection::CycleDown);   // back to the live patch

  EXPECT_FALSE(navigationPatchDirty());
  // the getter now reports the LIVE name - screenSyncParamEdit() writes this
  // unconditionally, so the label can't be left showing the stale preview
  EXPECT_EQ(navigationPreviewPatchName(), "M1B1P1");
}

TEST_F(NavigationTest, Button10_AdvancesOnlyThroughAnExplicitParametersList) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "M1", "parameters": ["Bank", "MidiChannel"],
                "banks": [ { "name": "B1", "patches": [ { "name": "P1" } ] },
                           { "name": "B2", "patches": [ { "name": "P2" } ] } ] }]
  })");
  EXPECT_EQ(navigationButton10Label(), "Bank");

  navButton10Completed(50);
  EXPECT_EQ(navigationButton10Label(), "MidiChannel");

  navButton10Completed(50);
  EXPECT_EQ(navigationButton10Label(), "Bank");   // only the two configured, wraps
}

// ---------------------------------------------------------------------------
// pages (Button.page) - see docs/navigation.md
//
// A Patch may spread its buttons across several pages so one Patch can
// define more buttons than the 13-button hardware has. All pages are
// initialised together on entry to the patch (every page's defaultStatus:On
// buttons fire), a page change is a pure immediate cursor move (no teardown,
// no "Set?"), and leaving the patch tears down EVERY page, not just the one
// on screen. A button with page 0 shows on every page. (id, page) is a
// composite key within a patch.
// ---------------------------------------------------------------------------

// M1 / B1 (ordinary bank):
//   P1 : multi-page (2 pages + a page-0 button)
//        page 1 : id 0 Latched      -> CC102 (127 on / 0 off), ch 1
//                 id 6 Latched, defaultStatus On -> PlayNotes C2 (36), ch 1
//        page 2 : id 0 Latched      -> CC108 (127 on / 0 off), ch 1
//                 id 5 Latched, defaultStatus On -> PlayNotes E2 (40), ch 1
//                 id 1 / id 2 Exclusive set {1,2} -> CC 20 / CC 21
//        page 0 : id 9 Latched, defaultStatus On -> PlayNotes G2 (43), ch 1
//                 (shows on every page)
//   P2 : single page
//        id 0 Latched -> CC 64 (127 on / 0 off), ch 1
// M1 / B2 : one patch "BX" (somewhere to leave to)
static const char *PAGES_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1", "banks": [
      { "name": "B1", "patches": [
        { "name": "P1", "buttons": [
          { "id": 0, "page": 1, "name": "cc102", "buttonMode": "Latched",
            "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 102, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] },
          { "id": 6, "page": 1, "name": "seedP1", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "midiChannel": 1 } } ] },
          { "id": 0, "page": 2, "name": "cc108", "buttonMode": "Latched",
            "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 108, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] },
          { "id": 5, "page": 2, "name": "seedP2", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "midiChannel": 1 } } ] },
          { "id": 1, "page": 2, "name": "x1", "buttonMode": "Exclusive", "buttonSet": [2],
            "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 20, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] },
          { "id": 2, "page": 2, "name": "x2", "buttonMode": "Exclusive", "buttonSet": [1],
            "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 21, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] },
          { "id": 9, "page": 0, "name": "everyPage", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["G2"], "midiChannel": 1 } } ] }
        ]},
        { "name": "P2", "buttons": [
          { "id": 0, "name": "cc64", "buttonMode": "Latched",
            "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 64, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] }
        ]}
      ]},
      { "name": "B2", "patches": [ { "name": "BX" } ] }
    ]}
  ]
})";

// true iff midiLog() contains a ControlChange for controller `cc` with value `v`
static bool midiLogHasCC(int cc, int v) {
  for (const MidiCall &call : midiLog()) {
    if (call.kind == MidiCall::ControlChange && call.a == cc && call.b == v) {
      return true;
    }
  }
  return false;
}

// the live-tree Button object for (patchIndex, page, buttonId) in M1 / B1,
// or null - (id, page) is a composite key so the page must be given
static Button *pageTreeButton(int patchIndex, int page, int buttonId) {
  ModeBank &bank = config.modes[0].banks[0];
  for (Button &b : bank.patches[patchIndex].buttons) {
    if (b.id == buttonId && b.page == page) {
      return &b;
    }
  }
  return nullptr;
}

// the index of the Page parameter in the applicable parameter list, or -1
static int pageParameterIndex() {
  std::vector<Parameter> &params = getCurrentParameters();
  for (int i = 0; i < (int)params.size(); i++) {
    if (params[i] == Parameter::PageParameter) {
      return i;
    }
  }
  return -1;
}

// ---- initialisation across all pages ----

TEST_F(NavigationTest, MultiPagePatch_InitialisesDefaultStatusOnEveryPage) {
  // loadTree() seeds the landing patch exactly as a real config load would
  JsonDocument doc = parseJson(PAGES_TREE);
  config = Config(doc);
  currentModeIndex = 0;
  currentBankIndex = 0;
  currentPatchIndex = 0;
  currentPage = 1;
  currentParameterIndex = 0;
  applyDefaultButtonStatuses();

  // page 1's, page 2's AND the page-0 default-on buttons all fired, even
  // though only page 1 is on screen
  EXPECT_TRUE(pageTreeButton(0, 1, 6)->isOn);   // page 1 seed
  EXPECT_TRUE(pageTreeButton(0, 2, 5)->isOn);   // page 2 seed - the key win over the old bank model
  EXPECT_TRUE(pageTreeButton(0, 0, 9)->isOn);   // page-0 "every page" seed

  // C2 (36), E2 (40) and G2 (43) each sent exactly once
  int noteOns = 0;
  for (const MidiCall &call : midiLog()) {
    if (call.kind == MidiCall::NoteOn) {
      noteOns++;
    }
  }
  EXPECT_EQ(noteOns, 3);
}

TEST_F(NavigationTest, Page0Button_ResolvesOnEveryPage_AndSeedsOnlyOnce) {
  loadTree(PAGES_TREE);

  currentPage = 1;
  EXPECT_EQ(resolveButton(9), pageTreeButton(0, 0, 9));
  currentPage = 2;
  EXPECT_EQ(resolveButton(9), pageTreeButton(0, 0, 9));
}

// ---- entering a multi-page patch auto-selects Page ----

TEST_F(NavigationTest, EnteringMultiPagePatch_AutoSelectsThePageParameter) {
  loadTree(PAGES_TREE);            // lands P1 (2 pages) -> selector should be Page

  ASSERT_GE(pageParameterIndex(), 0);
  EXPECT_EQ(currentParameterIndex, pageParameterIndex());
  EXPECT_EQ(navigationButton10Label(), "Page");
  EXPECT_EQ(currentPage, 1);
}

TEST_F(NavigationTest, EnteringSinglePagePatch_LeavesTheSelectorAlone_ButPageStillListed) {
  loadTree(PAGES_TREE);
  currentParameterIndex = 0;       // pretend the player parked it on Patch

  navSetPatch(1);                  // -> P2, a single-page patch

  EXPECT_EQ(currentParameterIndex, 0);        // NOT auto-moved
  ASSERT_GE(pageParameterIndex(), 0);         // Page is still in the list...

  currentParameterIndex = pageParameterIndex();
  EXPECT_EQ(navigationValueCount(), 1);       // ...it just has nothing to step
  navStepValue(CycleDirection::CycleUp);      // and stepping it is a no-op
  EXPECT_EQ(currentPage, 1);
}

TEST_F(NavigationTest, PageValueCount_IsThePatchPageCount) {
  loadTree(PAGES_TREE);            // P1 has 2 pages, selector auto-on Page
  EXPECT_EQ(navigationValueCount(), 2);
}

// ---- a page change is immediate and side-effect-free ----

TEST_F(NavigationTest, PageChange_IsImmediate_NoPendingState) {
  loadTree(PAGES_TREE);
  ASSERT_EQ(currentParameterIndex, pageParameterIndex());

  navCyclePage(CycleDirection::CycleUp);

  EXPECT_EQ(currentPage, 2);
  EXPECT_FALSE(navigationDirty());            // no "Set?"
  navCyclePage(CycleDirection::CycleUp);
  EXPECT_EQ(currentPage, 1);                  // wraps within the patch
}

TEST_F(NavigationTest, PageChange_DoesNotTearDownHeldSound) {
  loadTree(PAGES_TREE);
  triggerButton(0);                           // page 1 id 0 -> CC 102 = 127
  ASSERT_TRUE(midiLogHasCC(102, 127));
  clearMidiLog();

  navCyclePage(CycleDirection::CycleUp);      // -> page 2

  EXPECT_TRUE(midiLog().empty());             // nothing sent at all
  EXPECT_FALSE(midiLogHasCC(102, 0));         // held CC never released
  EXPECT_TRUE(pageTreeButton(0, 1, 0)->isOn); // page 1 id 0 still on
}

TEST_F(NavigationTest, PageChange_AddressesTheOtherPagesButtons) {
  loadTree(PAGES_TREE);

  EXPECT_EQ(resolveButton(0), pageTreeButton(0, 1, 0));   // page 1 id 0 (CC102)
  navCyclePage(CycleDirection::CycleUp);
  EXPECT_EQ(resolveButton(0), pageTreeButton(0, 2, 0));   // page 2 id 0 (CC108)

  triggerButton(0);
  EXPECT_TRUE(midiLogHasCC(108, 127));                    // the page-2 button ran
}

TEST_F(NavigationTest, SameIdOnDifferentPages_AreIndependentObjects) {
  loadTree(PAGES_TREE);

  triggerButton(0);                           // page 1 id 0 on  (CC 102)
  navCyclePage(CycleDirection::CycleUp);
  triggerButton(0);                           // page 2 id 0 on  (CC 108)

  EXPECT_TRUE(pageTreeButton(0, 1, 0)->isOn);
  EXPECT_TRUE(pageTreeButton(0, 2, 0)->isOn); // both held at once
}

TEST_F(NavigationTest, ExclusiveSet_CancelsSiblings_WithinAPage) {
  loadTree(PAGES_TREE);
  navCyclePage(CycleDirection::CycleUp);      // -> page 2 (Exclusive pair id 1 / id 2)

  triggerButton(1);
  EXPECT_TRUE(pageTreeButton(0, 2, 1)->isOn);

  triggerButton(2);                           // displaces id 1 within page 2
  EXPECT_FALSE(pageTreeButton(0, 2, 1)->isOn);
  EXPECT_TRUE(pageTreeButton(0, 2, 2)->isOn);
}

TEST_F(NavigationTest, CyclePage_OnASinglePagePatch_IsANoOp) {
  loadTree(PAGES_TREE);
  navSetPatch(1);                             // -> P2, single page
  clearMidiLog();

  navCyclePage(CycleDirection::CycleUp);

  EXPECT_EQ(currentPage, 1);
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationTest, NavSetPage_JumpsDirectly_RejectsOutOfRange) {
  loadTree(PAGES_TREE);

  navSetPage(2);
  EXPECT_EQ(currentPage, 2);

  navSetPage(0);
  navSetPage(3);
  EXPECT_EQ(currentPage, 2);                  // both rejected, unchanged
}

TEST_F(NavigationTest, StepPageParameterValue_ViaNavStepValue_IsImmediate) {
  loadTree(PAGES_TREE);
  ASSERT_EQ(currentParameterIndex, pageParameterIndex());

  navStepValue(CycleDirection::CycleUp);

  EXPECT_EQ(currentPage, 2);
  EXPECT_FALSE(navigationDirty());
}

// ---- leaving a multi-page patch tears down every page ----

TEST_F(NavigationTest, LeavingMultiPagePatch_TearsDownEveryPage) {
  loadTree(PAGES_TREE);
  triggerButton(0);                           // CC 102 = 127 held on page 1
  navCyclePage(CycleDirection::CycleUp);      // -> page 2
  triggerButton(0);                           // CC 108 = 127 held on page 2
  clearMidiLog();

  navSetBank(1);                              // leave P1 -> B2 / BX (full teardown)

  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_TRUE(midiLogHasCC(102, 0));          // page 1's stranded CC released...
  EXPECT_TRUE(midiLogHasCC(108, 0));          // ...and page 2's, though page 2 was on screen
  EXPECT_FALSE(pageTreeButton(0, 1, 0)->isOn);
  EXPECT_FALSE(pageTreeButton(0, 2, 0)->isOn);
  EXPECT_FALSE(pageTreeButton(0, 1, 6)->isOn); // default-on buttons torn down too
  EXPECT_FALSE(pageTreeButton(0, 2, 5)->isOn);
  EXPECT_FALSE(pageTreeButton(0, 0, 9)->isOn);
}

TEST_F(NavigationTest, ReturningToAMultiPagePatch_ReseedsEveryPageAndResetsToPageOne) {
  loadTree(PAGES_TREE);
  triggerButton(6);                           // turn page 1's seed off
  navCyclePage(CycleDirection::CycleUp);      // -> page 2
  ASSERT_EQ(currentPage, 2);

  navSetBank(1);                              // leave...
  navSetBank(0);                              // ...and come back -> P1

  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_EQ(currentPage, 1);                  // always reopens on page 1
  EXPECT_TRUE(pageTreeButton(0, 1, 6)->isOn); // re-seeded from defaultStatus
  EXPECT_TRUE(pageTreeButton(0, 2, 5)->isOn);
}

// ---- the subheader page suffix ----

TEST_F(NavigationTest, PreviewPatchName_HasPageSuffix_WhenLivePatchIsMultiPage) {
  loadTree(PAGES_TREE);
  EXPECT_EQ(navigationPreviewPatchName(), "P1 / 1");

  navCyclePage(CycleDirection::CycleUp);
  EXPECT_EQ(navigationPreviewPatchName(), "P1 / 2");
}

TEST_F(NavigationTest, PreviewPatchName_HasNoSuffix_ForASinglePagePatch) {
  loadTree(PAGES_TREE);
  navSetPatch(1);                             // -> P2, single page
  EXPECT_EQ(navigationPreviewPatchName(), "P2");
}

TEST_F(NavigationTest, PreviewPatchName_HasNoSuffix_WhilePreviewingAnotherPatch) {
  loadTree(PAGES_TREE);
  currentParameterIndex = 0;                  // selector on Patch
  navStepValue(CycleDirection::CycleUp);      // pending patch -> P2

  ASSERT_TRUE(navigationPatchDirty());
  EXPECT_EQ(navigationPreviewPatchName(), "P2");   // bare candidate name, no " / N"
}

// ---- ordinary (single-page) patch behaviour is unchanged ----

TEST_F(NavigationTest, OrdinaryPatchChange_StillTearsDownAndReappliesDefaults) {
  loadTree(PAGES_TREE);
  currentPatchIndex = 1;                      // live M1 / B1 / P2 (single-page)
  currentPage = 1;
  currentParameterIndex = 0;
  resetPendingToLive();

  triggerButton(0);                           // P2 id 0 -> CC 64 = 127
  clearMidiLog();

  navSetPatch(0);                             // -> P1: full teardown + re-seed

  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_TRUE(midiLogHasCC(64, 0));           // P2's held CC released
  EXPECT_FALSE(pageTreeButton(1, 1, 0)->isOn);
  EXPECT_TRUE(pageTreeButton(0, 1, 6)->isOn); // landing on P1 re-seeds every page
  EXPECT_TRUE(pageTreeButton(0, 2, 5)->isOn);
}

TEST_F(NavigationTest, OrdinaryCyclePatch_StillDefersBehindSet) {
  loadTree(PAGES_TREE);
  currentBankIndex = 0;
  currentPatchIndex = 1;                      // P2 (single page, ordinary bank)
  resetPendingToLive();

  navCyclePatch(CycleDirection::CycleUp);

  EXPECT_EQ(currentPatchIndex, 1);            // live NOT moved - deferred as before
  EXPECT_EQ(pendingPatchIndex, 0);
  EXPECT_TRUE(navigationDirty());
}

// ---- ModeBank.MaintainActiveButtonOnPatchChange ----
//
// The ChordPlayer scenario this exists for: button 1 in M1B1 plays a chord
// (held via Latched); switching between M1B1's patches is really switching
// the chord's *voicing* (a different set of notes for the same button), so
// the chord should keep sounding across that switch rather than cutting out
// and needing re-pressed. M1B2 does not set the flag, so it's the ordinary-
// behaviour control case.
static const char *MAINTAIN_BUTTON_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1", "banks": [
      { "name": "M1B1", "maintainActiveButtonOnPatchChange": true, "patches": [
        { "name": "P1", "buttons": [
          { "id": 1, "name": "chord-voicing-1", "buttonMode": "Latched",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "midiChannel": 1 } } ] }
        ]},
        { "name": "P2", "buttons": [
          { "id": 1, "name": "chord-voicing-2", "buttonMode": "Latched",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "midiChannel": 1 } } ] }
        ]}
      ]},
      { "name": "M1B2", "patches": [
        { "name": "P3", "buttons": [
          { "id": 1, "name": "other-bank", "buttonMode": "Latched",
            "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["G2"], "midiChannel": 1 } } ] }
        ]}
      ]}
    ]}
  ]
})";

TEST_F(NavigationTest, MaintainActiveButtonOnPatchChange_CarriesHeldButtonAcrossSameBankPatchChange) {
  loadTree(MAINTAIN_BUTTON_TREE);   // live M1 / M1B1 / P1

  triggerButton(1);                 // latch P1's button 1 -> C2 sounds
  ASSERT_TRUE(resolveButton(1)->isOn);
  clearMidiLog();

  navSetPatch(1);                   // -> P2, same bank (voicing change)

  EXPECT_EQ(currentPatchIndex, 1);
  ASSERT_EQ(midiLog().size(), 2u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);   // P1's C2 released first
  EXPECT_EQ(midiLog()[0].a, 36);
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);    // P2's button 1 re-triggered: E2
  EXPECT_EQ(midiLog()[1].a, 40);
  EXPECT_TRUE(resolveButton(1)->isOn);               // carried over, on in the new patch
}

TEST_F(NavigationTest, MaintainActiveButtonOnPatchChange_DoesNotApplyAcrossABankChange) {
  loadTree(MAINTAIN_BUTTON_TREE);   // live M1 / M1B1 / P1

  triggerButton(1);                 // latch P1's button 1 -> C2 sounds
  clearMidiLog();

  navSetBank(1);                    // -> M1B2 / P3 - a different bank, not a same-bank patch change

  EXPECT_EQ(currentBankIndex, 1);
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);   // only the teardown - no re-trigger
  EXPECT_FALSE(resolveButton(1)->isOn);              // P3's button 1 was left off
}

TEST_F(NavigationTest, MaintainActiveButtonOnPatchChange_DoesNothingWhenTheButtonWasNotOn) {
  loadTree(MAINTAIN_BUTTON_TREE);   // live M1 / M1B1 / P1, button 1 untouched

  navSetPatch(1);                   // -> P2, same bank

  EXPECT_EQ(currentPatchIndex, 1);
  EXPECT_TRUE(midiLog().empty());   // nothing was on, so nothing to release or restore
  EXPECT_FALSE(resolveButton(1)->isOn);
}

// ---- CyclePageAction / SetPageAction wiring ----

TEST_F(NavigationTest, CyclePageAction_StepsTheLivePageImmediately) {
  loadTree(PAGES_TREE);

  JsonDocument doc = parseJson(R"({ "type": "CyclePage", "actionParameters": { "direction": "Up" } })");
  CyclePageAction action(ButtonMode::Momentary, doc.as<JsonVariant>());
  action.OnPress();

  EXPECT_EQ(currentPage, 2);
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, SetPageAction_JumpsToTheGivenPageImmediately) {
  loadTree(PAGES_TREE);

  JsonDocument doc = parseJson(R"({ "type": "SetPage", "actionParameters": { "page": 2 } })");
  SetPageAction action(ButtonMode::Momentary, doc.as<JsonVariant>());
  action.OnPress();

  EXPECT_EQ(currentPage, 2);
  EXPECT_FALSE(navigationDirty());
}


// ---------------------------------------------------------------------------
// filesystem-backed fixture (Config level) - mirrors ParameterEditConfigTest
// ---------------------------------------------------------------------------

class NavigationConfigTest : public ::testing::Test {
  protected:
    std::filesystem::path testDir;

    void SetUp() override {
      resetTestState();
      std::string testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
      testDir = std::filesystem::temp_directory_path() / ("miditoolbox_nav_" + testName);
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

TEST_F(NavigationConfigTest, PendingCycleConfig_PreviewsWithoutLoading_AndCascadesDirty) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [ { "name": "BP" } ] } ] } ]
  })");
  writeFile("configs/Other.json", R"({
    "modes": [ { "name": "OtherMode", "banks": [ { "name": "OB", "patches": [ { "name": "OP" } ] } ] } ]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();
  ASSERT_FALSE(navigationDirty());

  pendingCycleConfig(CycleDirection::CycleUp);

  EXPECT_EQ(pendingConfigFilename, "Other.json");
  EXPECT_EQ(currentConfigFilename, "Base.json");   // not loaded
  EXPECT_TRUE(navigationConfigDirty());
  EXPECT_TRUE(navigationModeDirty());              // cascades down every level
  EXPECT_TRUE(navigationBankDirty());
  EXPECT_TRUE(navigationPatchDirty());
  EXPECT_TRUE(navigationDirty());
}

TEST_F(NavigationConfigTest, PendingCycleConfig_FullLoopBackToRunningConfig_ClearsDirty) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [ { "name": "BP" } ] } ] } ]
  })");
  writeFile("configs/Other.json", R"({ "modes": [ { "name": "OtherMode", "banks": [] } ] })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  pendingCycleConfig(CycleDirection::CycleUp);   // Base -> Other
  pendingCycleConfig(CycleDirection::CycleUp);   // Other -> Base (2 files, wraps)

  EXPECT_EQ(pendingConfigFilename, "Base.json");
  EXPECT_FALSE(navigationDirty());               // pending tree restored to live
}

TEST_F(NavigationConfigTest, PendingSetConfig_RejectsUnknownFile) {
  writeFile("configs/Base.json", R"({ "modes": [ { "name": "BaseMode", "banks": [] } ] })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  EXPECT_FALSE(pendingSetConfig("Ghost.json"));
  EXPECT_EQ(pendingConfigFilename, "Base.json");
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationConfigTest, Activate_PendingConfig_LoadsItAfterTearingDownHeldNotes) {
  // Base holds a latched C2 (36); Other has a default-on G3 (55)
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 1, "name": "h", "buttonMode": "Latched",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "midiChannel": 1 } } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Other.json", R"({
    "modes": [ { "name": "OtherMode", "banks": [ { "name": "OB", "patches": [
      { "name": "OP", "buttons": [
        { "id": 2, "name": "d", "buttonMode": "Momentary", "defaultStatus": "On",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["G3"], "midiChannel": 1 } } ] }
      ]}
    ]}]}]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();
  triggerButton(1);                 // latch C2 in Base
  ASSERT_EQ(midiLog().size(), 1u);
  clearMidiLog();

  pendingCycleConfig(CycleDirection::CycleUp);   // pending -> Other.json
  ASSERT_EQ(currentConfigFilename, "Base.json"); // not loaded yet
  ASSERT_TRUE(midiLog().empty());

  activateSelection();

  EXPECT_EQ(currentConfigFilename, "Other.json");
  EXPECT_FALSE(navigationDirty());

  // C2 off (Base teardown) must come before G3 on (Other's default)
  ASSERT_GE(midiLog().size(), 2u);
  EXPECT_EQ(midiLog().front().kind, MidiCall::NoteOff);
  EXPECT_EQ(midiLog().front().a, 36);
  int offAt = -1, onAt = -1;
  for (int i = 0; i < (int)midiLog().size(); i++) {
    if (midiLog()[i].kind == MidiCall::NoteOff && midiLog()[i].a == 36) offAt = i;
    if (midiLog()[i].kind == MidiCall::NoteOn && midiLog()[i].a == 55) onAt = i;
  }
  EXPECT_GE(offAt, 0);
  EXPECT_GE(onAt, 0);
  EXPECT_LT(offAt, onAt);
}

TEST_F(NavigationConfigTest, NavPeekConfigFirstNames_ReturnsTheFirstModeBankPatch) {
  writeFile("configs/Other.json", R"({
    "modes": [ { "name": "OMode", "banks": [ { "name": "OBank", "patches": [ { "name": "OPatch" } ] } ] } ]
  })");

  String mode, bank, patch;
  bool ok = navPeekConfigFirstNames("Other.json", mode, bank, patch);

  EXPECT_TRUE(ok);
  EXPECT_EQ(mode, "OMode");
  EXPECT_EQ(bank, "OBank");
  EXPECT_EQ(patch, "OPatch");
}

TEST_F(NavigationConfigTest, NavPeekConfigFirstNames_MissingFile_ReturnsFalseAndDashes) {
  String mode, bank, patch;
  bool ok = navPeekConfigFirstNames("DoesNotExist.json", mode, bank, patch);

  EXPECT_FALSE(ok);
  EXPECT_EQ(mode, "-");
  EXPECT_EQ(bank, "-");
  EXPECT_EQ(patch, "-");
}

TEST_F(NavigationConfigTest, PreviewNames_UsePeekedValues_WhileAConfigChangeIsPending) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [ { "name": "BP" } ] } ] } ]
  })");
  writeFile("configs/Other.json", R"({
    "modes": [ { "name": "OtherMode", "banks": [ { "name": "OB", "patches": [ { "name": "OP" } ] } ] } ]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  pendingCycleConfig(CycleDirection::CycleUp);   // -> Other.json (not loaded)

  EXPECT_EQ(navigationPreviewConfigName(), "Other.json");
  EXPECT_EQ(navigationPreviewModeName(), "OtherMode");   // peeked, not loaded
  EXPECT_EQ(navigationPreviewBankName(), "OB");
  EXPECT_EQ(navigationPreviewPatchName(), "OP");
  EXPECT_EQ(currentConfigFilename, "Base.json");

  navButton10Completed(50);   // quick tap -> activate
  EXPECT_EQ(currentConfigFilename, "Other.json");
  EXPECT_EQ(navigationPreviewModeName(), "OtherMode");   // now the live value
}

TEST_F(NavigationConfigTest, Activate_PendingConfig_MissingFile_LeavesLiveConfigInPlace) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [ { "name": "BP" } ] } ] } ]
  })");
  writeFile("configs/Gone.json", R"({ "modes": [ { "name": "GoneMode", "banks": [] } ] })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  pendingCycleConfig(CycleDirection::CycleUp);   // pending -> Gone.json
  std::filesystem::remove("configs/Gone.json");  // ... which then disappears

  activateSelection();

  EXPECT_EQ(currentConfigFilename, "Base.json"); // load failed, config unchanged
  EXPECT_FALSE(navigationDirty());               // pending re-synced to live
}

// ---------------------------------------------------------------------------
// SetConfigAction - a direct Set: navSetConfig -> applySelection loads the
// named file straight away (after tearing down the outgoing config's held
// sound), with pending left synced to live. An unknown file is a no-op.
// ---------------------------------------------------------------------------

TEST_F(NavigationConfigTest, SetConfigAction_LoadsTheNamedConfigImmediately) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 0, "name": "go", "buttonMode": "Momentary",
          "actions": [ { "type": "SetConfig", "actionParameters": { "configFilename": "Target.json" } } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Target.json", R"({
    "modes": [ { "name": "TargetMode", "banks": [ { "name": "TB", "patches": [ { "name": "TP" } ] } ] } ]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  triggerButton(0);                 // SetConfig -> Target.json (immediate)

  EXPECT_EQ(currentConfigFilename, "Target.json");
  EXPECT_EQ(getCurrentMode().name, "TargetMode");
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationConfigTest, SetConfigAction_UnknownFile_KeepsTheCurrentConfig) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 0, "name": "go", "buttonMode": "Momentary",
          "actions": [ { "type": "SetConfig", "actionParameters": { "configFilename": "Ghost.json" } } ] }
      ]}
    ]}]}]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  triggerButton(0);                 // SetConfig -> a file that does not exist

  EXPECT_EQ(currentConfigFilename, "Base.json");
  EXPECT_EQ(getCurrentMode().name, "BaseMode");
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationConfigTest, SetConfigAction_TearsDownHeldNotesBeforeLoadingTheNewConfig) {
  // Base holds a latched C2 (36); Target has a default-on G3 (55)
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 0, "name": "go", "buttonMode": "Momentary",
          "actions": [ { "type": "SetConfig", "actionParameters": { "configFilename": "Target.json" } } ] },
        { "id": 1, "name": "hold", "buttonMode": "Latched",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C2"], "midiChannel": 1 } } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Target.json", R"({
    "modes": [ { "name": "TargetMode", "banks": [ { "name": "TB", "patches": [
      { "name": "TP", "buttons": [
        { "id": 2, "name": "d", "buttonMode": "Momentary", "defaultStatus": "On",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["G3"], "midiChannel": 1 } } ] }
      ]}
    ]}]}]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();
  triggerButton(1);                 // latch C2 in Base
  ASSERT_EQ(midiLog().size(), 1u);
  clearMidiLog();

  triggerButton(0);                 // SetConfig -> Target.json

  EXPECT_EQ(currentConfigFilename, "Target.json");

  // C2 off (Base teardown) must come before G3 on (Target's default)
  int offAt = -1, onAt = -1;
  for (int i = 0; i < (int)midiLog().size(); i++) {
    if (midiLog()[i].kind == MidiCall::NoteOff && midiLog()[i].a == 36) offAt = i;
    if (midiLog()[i].kind == MidiCall::NoteOn  && midiLog()[i].a == 55) onAt = i;
  }
  EXPECT_GE(offAt, 0);
  EXPECT_GE(onAt, 0);
  EXPECT_LT(offAt, onAt);
}

// ---------------------------------------------------------------------------
// CycleConfigAction - a deferred "navigate" verb: navCycleConfig ->
// pendingCycleConfig moves the pending cursor to the next/previous config
// file alphabetically; nothing loads until an ActivateSelection.
// ---------------------------------------------------------------------------

TEST_F(NavigationConfigTest, CycleConfigAction_MovesThePendingConfigOnly_WithoutLoading) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 0, "name": "cc", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleConfig", "actionParameters": { "direction": "Up" } } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Other.json", R"({ "modes": [ { "name": "OtherMode", "banks": [] } ] })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();
  clearMidiLog();

  triggerButton(0);                 // CycleConfig Up

  EXPECT_EQ(pendingConfigFilename, "Other.json");
  EXPECT_EQ(currentConfigFilename, "Base.json");   // nothing loaded
  EXPECT_TRUE(navigationDirty());
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(NavigationConfigTest, CycleConfigAction_Down_MovesToThePreviousConfigAlphabetically) {
  writeFile("configs/A.json", R"({ "modes": [ { "name": "AMode", "banks": [] } ] })");
  writeFile("configs/M.json", R"({
    "modes": [ { "name": "MMode", "banks": [ { "name": "MB", "patches": [
      { "name": "MP", "buttons": [
        { "id": 0, "name": "ccdown", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleConfig", "actionParameters": { "direction": "Down" } } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Z.json", R"({ "modes": [ { "name": "ZMode", "banks": [] } ] })");
  ASSERT_TRUE(loadConfigFile("M.json"));
  resetPendingToLive();

  triggerButton(0);                 // CycleConfig Down -> previous alphabetical entry

  EXPECT_EQ(pendingConfigFilename, "A.json");
  EXPECT_EQ(currentConfigFilename, "M.json");      // still not loaded
  EXPECT_TRUE(navigationDirty());
}

TEST_F(NavigationConfigTest, CycleConfigAction_ThenActivateSelection_LoadsThePendingConfig) {
  writeFile("configs/Base.json", R"({
    "modes": [ { "name": "BaseMode", "banks": [ { "name": "BB", "patches": [
      { "name": "BP", "buttons": [
        { "id": 0, "name": "cc", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleConfig", "actionParameters": { "direction": "Up" } } ] },
        { "id": 1, "name": "go", "buttonMode": "Momentary",
          "actions": [ { "type": "ActivateSelection" } ] }
      ]}
    ]}]}]
  })");
  writeFile("configs/Other.json", R"({
    "modes": [ { "name": "OtherMode", "banks": [ { "name": "OB", "patches": [ { "name": "OP" } ] } ] } ]
  })");
  ASSERT_TRUE(loadConfigFile("Base.json"));
  resetPendingToLive();

  triggerButton(0);                 // CycleConfig Up -> pending Other.json
  ASSERT_EQ(currentConfigFilename, "Base.json");
  ASSERT_TRUE(navigationDirty());

  triggerButton(1);                 // ActivateSelection -> commit the pending config

  EXPECT_EQ(currentConfigFilename, "Other.json");
  EXPECT_EQ(getCurrentMode().name, "OtherMode");
  EXPECT_FALSE(navigationDirty());
}


// ---------------------------------------------------------------------------
// button-name overlay ("toast")
// ---------------------------------------------------------------------------

// id 0: Momentary note.  id 1: Latched CC.  id 2: Exclusive PC.  id 3: SetPatch
// only.  id 4: CyclePatch only.  id 5: SetPatch + CC (navigates AND sends MIDI).
// id 6: a MIDI button with no name.
static const char *TOAST_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [ { "name": "M1",
    "buttons": [
      { "id": 0, "name": "Lead", "buttonMode": "Momentary", "colourCode": [200, 10, 10],
        "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] },
      { "id": 1, "name": "Wah", "buttonMode": "Latched",
        "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 11, "controlValue": 127, "releaseValue": 0, "midiChannel": 1 } } ] },
      { "id": 2, "name": "Clean", "buttonMode": "Exclusive", "buttonSet": [2],
        "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 3, "midiChannel": 1 } } ] },
      { "id": 3, "name": "Next", "buttonMode": "Momentary",
        "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } } ] },
      { "id": 4, "name": "Step", "buttonMode": "Momentary",
        "actions": [ { "type": "CyclePatch", "actionParameters": { "direction": "Up" } } ] },
      { "id": 5, "name": "Both", "buttonMode": "Momentary",
        "actions": [
          { "type": "SetPatch", "actionParameters": { "patchIndex": 1 } },
          { "type": "ControlChange", "actionParameters": { "controllerNumber": 1, "controlValue": 1, "releaseValue": 0, "midiChannel": 1 } }
        ] },
      { "id": 6, "name": "", "buttonMode": "Momentary",
        "actions": [ { "type": "ControlChange", "actionParameters": { "controllerNumber": 2, "controlValue": 1, "releaseValue": 0, "midiChannel": 1 } } ] }
    ],
    "banks": [ { "name": "B1", "subheaderColourCode": [1, 2, 3],
      "patches": [ { "name": "P1" }, { "name": "P2" } ] } ]
  } ]
})";

class ToastTest : public NavigationTest {
  protected:
    void SetUp() override {
      NavigationTest::SetUp();
      toastHide();
      loadTree(TOAST_TREE);
    }
};

TEST_F(ToastTest, MidiButton_ShowsItsName) {
  performanceButtonPress(0, "physical");

  EXPECT_TRUE(toastVisible(0));
  EXPECT_EQ(toastText, "Lead");
  EXPECT_EQ(toastColour.red, 200);   // the button's own colour
}

TEST_F(ToastTest, ButtonWithNoColour_UsesTheBanksColour) {
  performanceButtonPress(1, "physical");

  EXPECT_EQ(toastColour.red, 1);
  EXPECT_EQ(toastColour.green, 2);
  EXPECT_EQ(toastColour.blue, 3);
}

TEST_F(ToastTest, NavigationOnlyButtons_ShowNothing) {
  performanceButtonPress(3, "physical"); // SetPatch
  EXPECT_FALSE(toastVisible(0));

  performanceButtonPress(4, "physical"); // CyclePatch
  EXPECT_FALSE(toastVisible(0));
}

TEST_F(ToastTest, ButtonThatNavigatesAndSendsMidi_ShowsItsName) {
  performanceButtonPress(5, "physical");

  EXPECT_EQ(currentPatchIndex, 1);       // the SetPatch applied immediately
  EXPECT_TRUE(toastVisible(0));
  EXPECT_EQ(toastText, "Both");
}

TEST_F(ToastTest, UnnamedButton_ShowsNothing) {
  performanceButtonPress(6, "physical");

  EXPECT_FALSE(toastVisible(0));
}

TEST_F(ToastTest, LatchedButton_ShowsOnThenOff) {
  performanceButtonPress(1, "physical");
  EXPECT_EQ(toastText, "Wah ON");

  performanceButtonRelease(1);
  performanceButtonPress(1, "physical");
  EXPECT_EQ(toastText, "Wah OFF");
}

TEST_F(ToastTest, ExclusiveButton_ShowsOnThenOff) {
  performanceButtonPress(2, "physical");
  EXPECT_EQ(toastText, "Clean ON");

  performanceButtonPress(2, "physical");
  EXPECT_EQ(toastText, "Clean OFF");
}

TEST_F(ToastTest, ExpiresAfterTheMinimumTime) {
  performanceButtonPress(1, "physical"); // Latched: shown at t=0 (stub clock)

  EXPECT_TRUE(toastVisible(0));
  EXPECT_TRUE(toastVisible(TOAST_MIN_MS - 1));
  EXPECT_FALSE(toastVisible(TOAST_MIN_MS));
}

TEST_F(ToastTest, PressingAnotherMidiButton_ReplacesTheTextAndRestartsTheTimer) {
  toastShow("Old", ColourCode(0, 0, 0), -1, 100);
  EXPECT_FALSE(toastVisible(100 + TOAST_MIN_MS));

  toastShow("New", ColourCode(0, 0, 0), -1, 400);

  EXPECT_EQ(toastText, "New");
  EXPECT_TRUE(toastVisible(400 + TOAST_MIN_MS - 1));
}

TEST_F(ToastTest, HeldMomentaryButton_StaysUpUntilReleased_ThenFollowsTheMinimumTime) {
  performanceButtonPress(0, "physical"); // Momentary: held

  EXPECT_TRUE(toastVisible(10 * TOAST_MIN_MS)); // long past the minimum, still held

  performanceButtonRelease(0);

  EXPECT_FALSE(toastVisible(10 * TOAST_MIN_MS)); // released after the minimum: gone
}

TEST_F(ToastTest, QuickTapOfMomentaryButton_StillLastsTheMinimumTime) {
  performanceButtonPress(0, "physical");
  performanceButtonRelease(0);

  EXPECT_TRUE(toastVisible(TOAST_MIN_MS - 1));
  EXPECT_FALSE(toastVisible(TOAST_MIN_MS));
}

TEST_F(ToastTest, SurvivesWrapOfTheMillisClock) {
  toastShow("Wrap", ColourCode(0, 0, 0), -1, 0xFFFFFF00u);

  EXPECT_TRUE(toastVisible(0xFFFFFF00u + 10));
  EXPECT_TRUE(toastVisible(100));        // after the clock wrapped, still inside 500ms
  EXPECT_FALSE(toastVisible(0x1FFu));    // 0x100 + 0x1FF > 500ms after the show
}

// ---------------------------------------------------------------------------
// SetPatch / SetBank with a longer target path ("bank.patch", "mode.bank.patch",
// "mode.bank") - NAV_TREE: M1 = B1 (P1, P2), B2 (P1); M2 = B1 (3 patches)
// ---------------------------------------------------------------------------

TEST_F(NavigationTest, SetPatch_WithOnlyAPatchIndex_StaysInTheCurrentBank) {
  loadTree();

  navSetPatch(1);

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 1);
}

TEST_F(NavigationTest, SetPatch_WithABankIndex_LandsOnThatPatchOfThatBank) {
  loadTree();

  navSetPatch(0, 1);   // M1 > B2 > P1

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_EQ(currentPatchIndex, 0);
}

TEST_F(NavigationTest, SetPatch_WithBankAndModeIndex_LandsInAnotherMode) {
  loadTree();

  navSetPatch(2, 0, 1); // M2 > B1 > P3

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 2);
}

TEST_F(NavigationTest, SetPatch_ModeIndexWithoutABankIndex_IsIgnored) {
  loadTree();

  navSetPatch(1, -1, 1);

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentPatchIndex, 1);
}

TEST_F(NavigationTest, SetPatch_PatchMissingFromTheTargetBank_DoesNothing) {
  loadTree();

  navSetPatch(1, 1);    // M1 > B2 has only one patch

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
}

TEST_F(NavigationTest, SetPatch_SamePatchNumberInAnotherBank_IsNotTreatedAsAlreadyThere) {
  loadTree();            // live: M1 > B1 > P1

  navSetPatch(0, 1);     // M1 > B2 > P1 - patch index equal, bank differs

  EXPECT_EQ(currentBankIndex, 1);
}

// ---------------------------------------------------------------------------
// Pressing SetPatch for the patch that is ALREADY live resets it
// ---------------------------------------------------------------------------

// P1: id 0 a Latched chord, id 1 a Latched default-on program change, id 2 a
// Momentary SetPatch back to P1 (the "reset" button). P2 is empty.
static const char *RESET_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [ { "name": "M1", "banks": [ { "name": "B1", "patches": [
    { "name": "P1", "buttons": [
      { "id": 0, "name": "chord", "buttonMode": "Latched",
        "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] },
      { "id": 1, "name": "boot", "buttonMode": "Latched", "defaultStatus": "On",
        "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 7, "midiChannel": 1 } } ] },
      { "id": 2, "name": "reset", "buttonMode": "Momentary",
        "actions": [ { "type": "SetPatch", "actionParameters": { "patchIndex": 0 } } ] }
    ]},
    { "name": "P2" }
  ] } ] } ]
})";

TEST_F(NavigationTest, SetPatch_TheLiveTargetInFull_ResetsThatPatch) {
  loadTree(RESET_TREE);        // live M1 / B1 / P1; "boot" fired at load, log cleared
  triggerButton(0);            // the chord goes on
  ASSERT_TRUE(resolveButton(0)->isOn);
  clearMidiLog();

  navSetPatch(0, 0, 0);        // exactly where we already are

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(resolveButton(0)->isOn);   // held chord released...
  EXPECT_TRUE(resolveButton(1)->isOn);    // ...and the default-on button seeded again
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, ResettingThePatch_ReleasesFirst_ThenReSeedsTheDefaults) {
  loadTree(RESET_TREE);
  triggerButton(0);            // chord on (C3 = 48)
  clearMidiLog();

  triggerButton(2);            // the "reset" button: SetPatch for the patch we are on

  // the chord's note-off, then the default-on button's program change again
  bool sawNoteOff = false;
  bool programChangeAfterNoteOff = false;
  for (const MidiCall &call : midiLog()) {
    if (call.kind == MidiCall::NoteOff && call.a == 48) sawNoteOff = true;
    if (call.kind == MidiCall::ProgramChange && call.a == 7 && sawNoteOff) programChangeAfterNoteOff = true;
  }
  EXPECT_TRUE(sawNoteOff);
  EXPECT_TRUE(programChangeAfterNoteOff);
}

TEST_F(NavigationTest, ResettingThePatch_ReturnsToPage1_AndDiscardsAPendingChange) {
  loadTree(RESET_TREE);
  currentPage = 2;
  pendingCyclePatch(CycleDirection::CycleUp);   // a pending change, flashing "Set?"
  ASSERT_TRUE(navigationDirty());

  navSetPatch(0);

  EXPECT_EQ(currentPage, 1);
  EXPECT_EQ(currentPatchIndex, 0);              // the pending change was discarded, not applied
  EXPECT_FALSE(navigationDirty());
}

TEST_F(NavigationTest, ResettingThePatch_DoesNotMaintainAHeldChord) {
  // MaintainActiveButtonOnPatchChange carries a held button across a patch CHANGE;
  // a reset is not a change, so the chord is released like everything else.
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [ { "name": "M1", "banks": [ { "name": "B1", "maintainActiveButtonOnPatchChange": true, "patches": [
      { "name": "P1", "buttons": [
        { "id": 0, "name": "chord", "buttonMode": "Latched",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] } ] }
    ] } ] } ]
  })");
  triggerButton(0);
  ASSERT_TRUE(resolveButton(0)->isOn);

  navSetPatch(0);

  EXPECT_FALSE(resolveButton(0)->isOn);
}

TEST_F(NavigationTest, ADefaultOnButtonThatSelectsItsOwnPatch_StillDoesNotRecurse) {
  // The crash this guards against: seeding fires the button, its SetPatch targets
  // the patch being seeded, and honouring that would tear down and re-seed -
  // firing the same button again, forever. Seeding must fire it exactly once.
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [ { "name": "M1", "banks": [ { "name": "B1", "patches": [
      { "name": "P1", "buttons": [
        { "id": 2, "name": "selfSelect", "buttonMode": "Latched", "defaultStatus": "On",
          "actions": [
            { "type": "SetPatch", "actionParameters": { "patchIndex": 0 } },
            { "type": "PlayNotes", "actionParameters": { "noteNames": ["E2"], "velocity": 100, "midiChannel": 1 } }
          ] } ] }
    ] } ] } ]
  })");
  clearMidiLog();

  navSetPatch(0);            // a deliberate reset of that patch re-seeds it once...

  int noteOns = 0;
  for (const MidiCall &call : midiLog()) if (call.kind == MidiCall::NoteOn) noteOns++;
  EXPECT_EQ(noteOns, 1);     // ...and the button fires exactly once, not in a loop
  EXPECT_TRUE(resolveButton(2)->isOn);
  EXPECT_EQ(selectionPassDepth, 0);   // every selection pass was closed again
}

TEST_F(NavigationTest, SetBank_WithAModeIndex_LandsOnThatBanksDefaultPatch) {
  loadTree(R"({
    "headerColourCode": [0,0,0],
    "modes": [
      { "name": "M1", "banks": [ { "name": "B1", "patches": [ { "name": "P1" } ] } ] },
      { "name": "M2", "banks": [
        { "name": "B1", "patches": [ { "name": "P1" } ] },
        { "name": "B2", "defaultPatchIndex": 1, "patches": [ { "name": "P1" }, { "name": "P2" } ] }
      ] }
    ]
  })");

  navSetBank(1, 1);      // M2 > B2

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, 1);
  EXPECT_EQ(currentPatchIndex, 1);   // B2's default patch, not patch 0
}

TEST_F(NavigationTest, SetBank_WithoutAModeIndex_StaysInTheCurrentMode) {
  loadTree();

  navSetBank(1);

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 1);
}

TEST_F(NavigationTest, SetBank_SameBankNumberInAnotherMode_IsNotTreatedAsAlreadyThere) {
  loadTree();            // live: M1 > B1

  navSetBank(0, 1);      // M2 > B1

  EXPECT_EQ(currentModeIndex, 1);
}

TEST_F(NavigationTest, SetBank_BankMissingFromTheTargetMode_DoesNothing) {
  loadTree();

  navSetBank(1, 1);      // M2 has only one bank

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);
}

TEST_F(ConfigParsingTest, SetPatchAction_ParsesOptionalBankAndModeIndex) {
  JsonDocument doc = parseJson(R"({ "actionParameters": { "patchIndex": 2, "bankIndex": 1, "modeIndex": 3 } })");
  SetPatchAction action(ButtonMode::Momentary, doc.as<JsonVariant>());
  EXPECT_EQ(action.patchIndex, 2);
  EXPECT_EQ(action.bankIndex, 1);
  EXPECT_EQ(action.modeIndex, 3);

  JsonDocument plain = parseJson(R"({ "actionParameters": { "patchIndex": 2 } })");
  SetPatchAction bare(ButtonMode::Momentary, plain.as<JsonVariant>());
  EXPECT_EQ(bare.bankIndex, -1);
  EXPECT_EQ(bare.modeIndex, -1);
}

TEST_F(ConfigParsingTest, SetBankAction_ParsesOptionalModeIndex) {
  JsonDocument doc = parseJson(R"({ "actionParameters": { "bankIndex": 1, "modeIndex": 2 } })");
  SetBankAction action(ButtonMode::Momentary, doc.as<JsonVariant>());
  EXPECT_EQ(action.bankIndex, 1);
  EXPECT_EQ(action.modeIndex, 2);

  JsonDocument plain = parseJson(R"({ "actionParameters": { "bankIndex": 1 } })");
  SetBankAction bare(ButtonMode::Momentary, plain.as<JsonVariant>());
  EXPECT_EQ(bare.modeIndex, -1);
}


// ---------------------------------------------------------------------------
// SetMode for the mode that is already live
// ---------------------------------------------------------------------------

// M1: B1 (default patch 1 of 2), B2.  M2 has no banks at all.
static const char *SETMODE_SELF_TREE = R"({
  "headerColourCode": [0,0,0],
  "modes": [
    { "name": "M1", "banks": [
      { "name": "B1", "defaultPatchIndex": 1, "patches": [
        { "name": "P1" },
        { "name": "P2", "buttons": [
          { "id": 9, "name": "seed", "buttonMode": "Latched", "defaultStatus": "On",
            "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 3, "midiChannel": 1 } } ] } ] }
      ] },
      { "name": "B2", "patches": [ { "name": "P1" } ] }
    ] },
    { "name": "M2", "buttons": [
      { "id": 4, "name": "selfMode", "buttonMode": "Latched", "defaultStatus": "On",
        "actions": [ { "type": "SetMode", "actionParameters": { "modeIndex": 1 } } ] } ],
      "banks": [] }
  ]
})";

TEST_F(NavigationTest, SetMode_TheLiveModeFromAnotherBank_GoesBackToItsFirstBankAndDefaultPatch) {
  loadTree(SETMODE_SELF_TREE);        // live M1 / B1 / P2 (B1's default patch)
  navSetBank(1);                      // move to B2
  ASSERT_EQ(currentBankIndex, 1);

  navSetMode(0);                      // the mode we are in

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 1);
}

TEST_F(NavigationTest, SetMode_TheLiveModeAtItsStartingPoint_ResetsIt) {
  loadTree(SETMODE_SELF_TREE);        // live at the starting point; "seed" fired and the log was cleared

  navSetMode(0);

  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 1);
  ASSERT_FALSE(midiLog().empty());    // the default-on button was seeded again
  EXPECT_EQ(midiLog().back().kind, MidiCall::ProgramChange);
  EXPECT_TRUE(resolveButton(9)->isOn);
}

TEST_F(NavigationTest, SetMode_ADefaultOnButtonSelectingItsOwnBanklessMode_DoesNotRecurse) {
  loadTree(SETMODE_SELF_TREE);
  navSetMode(1);                      // M2 has no banks: bank and patch are both -1
  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, -1);
  EXPECT_TRUE(resolveButton(4)->isOn);   // its default-on SetMode(self) ran inside seeding and stopped there

  navSetMode(1);                      // a deliberate reset of the bankless mode terminates too

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(selectionPassDepth, 0);
}
