//----------------------------------------------------------------------------//
//    Tests for button behaviour (Runtime.h) - the core focus of this test  //
//    suite. Covers Momentary/Latched/Exclusive semantics, the Patch >      //
//    Bank > Mode override rule, per-level scoping of Exclusive button      //
//    sets, and multiple actions firing from a single button press.        //
//----------------------------------------------------------------------------//

class ButtonBehaviorTest : public ::testing::Test {
  protected:
    void SetUp() override {
      resetTestState();
    }

    // Parses the given JSON as a full config ({ "modes": [...] }) and
    // selects Mode 0 as current, with no Bank/Patch selected - the
    // starting point every test below builds on. Individual tests then
    // select a Bank/Patch themselves if the scenario needs one.
    void loadConfig(const char *json) {
      JsonDocument doc = parseJson(json);
      config = Config(doc);
      currentModeIndex = 0;
      currentBankIndex = -1;
      currentPatchIndex = -1;
      // keep the deferred-navigation pending cursor (Navigation.h) in sync
      // with the live selection, so any test that fires a Cycle*/Set* action
      // starts from a clean (not-dirty) baseline
      resetPendingToLive();
    }
};

TEST_F(ButtonBehaviorTest, MomentaryButton_TurnsOnWhenPressed_AndOffWhenReleased) {
  // Arrange
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Note0", "buttonMode": "Momentary",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] }
      ]
    }]
  })");

  // Act: press
  triggerButton(0);

  // Assert: a note-on was sent, and the button is marked on
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[0].a, 48); // C3
  EXPECT_EQ(midiLog()[0].channel, 1);

  Button *button = resolveButton(0);
  ASSERT_NE(button, nullptr);
  EXPECT_TRUE(button->isOn);

  clearMidiLog();

  // Act: release
  releaseButton(0);

  // Assert: a note-off was sent, and the button is marked off again
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);
  EXPECT_EQ(midiLog()[0].a, 48);
  EXPECT_FALSE(button->isOn);
}

TEST_F(ButtonBehaviorTest, LatchedButton_TogglesOnThenOff_AndIgnoresPhysicalRelease) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Reverb", "buttonMode": "Latched",
          "actions": [ { "type": "ControlChange",
            "actionParameters": { "controllerNumber": 20, "controlValue": 127, "releaseValue": 0, "midiChannel": 2 } } ] }
      ]
    }]
  })");
  Button *button = resolveButton(0);
  ASSERT_NE(button, nullptr);

  // first press: turns on, sends CC = 127
  triggerButton(0);
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::ControlChange);
  EXPECT_EQ(midiLog()[0].b, 127);
  EXPECT_TRUE(button->isOn);

  // physically releasing a Latched button does nothing at all
  clearMidiLog();
  releaseButton(0);
  EXPECT_EQ(midiLog().size(), 0u);
  EXPECT_TRUE(button->isOn);

  // second press: turns off, sends CC = 0 (the configured releaseValue)
  clearMidiLog();
  triggerButton(0);
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::ControlChange);
  EXPECT_EQ(midiLog()[0].b, 0);
  EXPECT_FALSE(button->isOn);
}

TEST_F(ButtonBehaviorTest, ExclusiveButton_TurnsOffOtherOnSiblings_AndTurnsItselfOn) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "CleanA", "buttonMode": "Exclusive", "buttonSet": [0, 1],
          "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } } ] },
        { "id": 1, "name": "CleanB", "buttonMode": "Exclusive", "buttonSet": [0, 1],
          "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } } ] }
      ]
    }]
  })");
  Button *buttonA = resolveButton(0);
  Button *buttonB = resolveButton(1);
  ASSERT_NE(buttonA, nullptr);
  ASSERT_NE(buttonB, nullptr);

  // pressing button A turns it on
  triggerButton(0);
  EXPECT_TRUE(buttonA->isOn);
  EXPECT_FALSE(buttonB->isOn);

  // pressing button B turns A off and B on - Exclusive buttons in the same
  // set can never both be on at once
  clearMidiLog();
  triggerButton(1);
  EXPECT_FALSE(buttonA->isOn);
  EXPECT_TRUE(buttonB->isOn);
  // program 2 (button B's action) is the last message sent
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::ProgramChange);
  EXPECT_EQ(midiLog().back().a, 2);
}

TEST_F(ButtonBehaviorTest, ExclusiveButton_PressedAgainWhileOn_TogglesOffLikeLatched) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Solo", "buttonMode": "Exclusive", "buttonSet": [0],
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["A3"] } } ] }
      ]
    }]
  })");
  Button *button = resolveButton(0);
  ASSERT_NE(button, nullptr);

  triggerButton(0);
  EXPECT_TRUE(button->isOn);
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOn);

  clearMidiLog();
  triggerButton(0);
  EXPECT_FALSE(button->isOn);
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOff);
}

TEST_F(ButtonBehaviorTest, ExclusiveButton_PhysicalRelease_DoesNothing) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Solo", "buttonMode": "Exclusive", "buttonSet": [0],
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["A3"] } } ] }
      ]
    }]
  })");
  Button *button = resolveButton(0);

  triggerButton(0);
  ASSERT_TRUE(button->isOn);

  clearMidiLog();
  releaseButton(0);

  // releasing an Exclusive button is a no-op - it stays on, and nothing is sent
  EXPECT_TRUE(button->isOn);
  EXPECT_EQ(midiLog().size(), 0u);
}

TEST_F(ButtonBehaviorTest, ExclusiveSets_AreScopedToTheirOwnLevel_NotSharedAcrossLevels) {
  // Bank-level ids 0/1 form one Exclusive set; a different Patch, selected
  // afterwards, defines its OWN Exclusive set that happens to reuse id 0.
  // Turning on the Patch-level button 0 must not be affected by (or affect)
  // whatever the Bank-level button 0/1 pair is doing.
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "buttons": [
          { "id": 0, "name": "BankA", "buttonMode": "Exclusive", "buttonSet": [0, 1],
            "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } } ] },
          { "id": 1, "name": "BankB", "buttonMode": "Exclusive", "buttonSet": [0, 1],
            "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } } ] }
        ],
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "PatchA", "buttonMode": "Exclusive", "buttonSet": [0, 2],
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 10 } } ] },
            { "id": 2, "name": "PatchB", "buttonMode": "Exclusive", "buttonSet": [0, 2],
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 11 } } ] }
          ]
        }]
      }]
    }]
  })");
  currentBankIndex = 0;

  // turn on Bank-level button 1 while no Patch is selected
  triggerButton(1);
  Button *bankB = resolveButton(1);
  ASSERT_NE(bankB, nullptr);
  EXPECT_TRUE(bankB->isOn);

  // now select the Patch - its button 0 fully overrides the Bank's button 0
  currentPatchIndex = 0;
  clearMidiLog();
  triggerButton(0);

  Button *patchA = resolveButton(0); // now resolves to the Patch-level button
  ASSERT_NE(patchA, nullptr);
  EXPECT_EQ(patchA->name, "PatchA");
  EXPECT_TRUE(patchA->isOn);
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().a, 10); // the Patch-level action fired, not the Bank's
}

TEST_F(ButtonBehaviorTest, ButtonResolution_PatchOverridesBank_OverridesMode) {
  // the same button id (0) is defined at all three levels; whichever is
  // most specific to the current selection should be the one that fires
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "ModeButton", "buttonMode": "Momentary",
          "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } } ] }
      ],
      "banks": [{
        "name": "Bank1",
        "buttons": [
          { "id": 0, "name": "BankButton", "buttonMode": "Momentary",
            "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } } ] }
        ],
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "PatchButton", "buttonMode": "Momentary",
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 3 } } ] }
          ]
        }]
      }]
    }]
  })");

  // with nothing selected but the Mode, the Mode-level button applies
  triggerButton(0);
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().a, 1);

  // selecting the Bank makes its button override the Mode's
  currentBankIndex = 0;
  clearMidiLog();
  triggerButton(0);
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().a, 2);

  // selecting the Patch makes its button override the Bank's (and the Mode's)
  currentPatchIndex = 0;
  clearMidiLog();
  triggerButton(0);
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().a, 3);
}

TEST_F(ButtonBehaviorTest, UnconfiguredButtonId_DoesNothingWithoutCrashing) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "Mode1", "buttons": [] }]
  })");

  // pressing/releasing a button id nothing defines - and that has no
  // built-in fallback either (unlike ids 10-12, see
  // builtInNavButtons()/resolveButtonContainer() in Runtime.h) - should be a
  // safe no-op. Deliberately not id 10/11/12: those now resolve to the
  // built-in navigation default even when a config defines nothing for them.
  EXPECT_NO_FATAL_FAILURE(triggerButton(99));
  EXPECT_NO_FATAL_FAILURE(releaseButton(99));
  EXPECT_EQ(midiLog().size(), 0u);
}

TEST_F(ButtonBehaviorTest, NavButtonIds_FallBackToBuiltInRoleWhenUnconfigured) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{ "name": "Mode1", "buttons": [] }]
  })");

  // ids 10/11/12 are never truly "unconfigured" - resolveButton() falls back
  // to the built-in navigation role (docs/navigation.md)
  // when nothing in the config tree defines them, so a config author gets
  // that behaviour for free without writing anything into their JSON.
  Button *button10 = resolveButton(10);
  ASSERT_NE(button10, nullptr);
  ASSERT_EQ(button10->actions.size(), 1u);
  EXPECT_EQ(button10->actions[0]->getType(), ActionType::CycleActivateParameter);

  Button *button11 = resolveButton(11);
  ASSERT_NE(button11, nullptr);
  ASSERT_EQ(button11->actions.size(), 1u);
  EXPECT_EQ(button11->actions[0]->getType(), ActionType::CycleParameterValue);

  Button *button12 = resolveButton(12);
  ASSERT_NE(button12, nullptr);
  ASSERT_EQ(button12->actions.size(), 1u);
  EXPECT_EQ(button12->actions[0]->getType(), ActionType::CycleParameterValue);

  EXPECT_FALSE(buttonHasExplicitOverride(10));
  EXPECT_FALSE(buttonHasExplicitOverride(11));
  EXPECT_FALSE(buttonHasExplicitOverride(12));
}

TEST_F(ButtonBehaviorTest, NavButtonId_ConfigOverrideReplacesBuiltInRole) {
  // a Mode-level Button bound to id 11 (normally "cycle parameter value
  // down") should completely replace the built-in role, exactly like any
  // other id - see resolveButtonContainer()'s Patch > Bank > Mode > Config >
  // built-in precedence in Runtime.h.
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [{
        "id": 11,
        "name": "Custom",
        "buttonMode": "Momentary",
        "actions": [{ "type": "Note", "actionParameters": { "noteNames": ["C4"] } }]
      }]
    }]
  })");

  EXPECT_TRUE(buttonHasExplicitOverride(11));

  clearMidiLog();
  triggerButton(11);
  ASSERT_FALSE(midiLog().empty());
  EXPECT_EQ(midiLog().back().kind, MidiCall::NoteOn);
}

TEST_F(ButtonBehaviorTest, SingleButtonPress_CanFireMultipleActionsTogether) {
  // it should be possible for one button to send MIDI notes AND a program
  // change AND a control change from a single press
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Combo", "buttonMode": "Momentary",
          "actions": [
            { "type": "Note", "actionParameters": { "noteNames": ["C3", "E3", "G3"], "midiChannel": 1 } },
            { "type": "ProgramChange", "actionParameters": { "programNumber": 7, "midiChannel": 1 } },
            { "type": "ControlChange", "actionParameters": { "controllerNumber": 64, "controlValue": 127, "midiChannel": 1 } }
          ]
        }
      ]
    }]
  })");

  triggerButton(0);

  // 3 note-ons (one per note in the chord) + 1 program change + 1 control change
  ASSERT_EQ(midiLog().size(), 5u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[1].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[2].kind, MidiCall::NoteOn);
  EXPECT_EQ(midiLog()[3].kind, MidiCall::ProgramChange);
  EXPECT_EQ(midiLog()[3].a, 7);
  EXPECT_EQ(midiLog()[4].kind, MidiCall::ControlChange);
  EXPECT_EQ(midiLog()[4].a, 64);
}

TEST_F(ButtonBehaviorTest, NoteAction_WithoutOwnMidiChannel_UsesCurrentMidiChannel) {
  // an action with no configured midiChannel (-1, the default) should use
  // whatever the globally-selected MIDI channel currently is
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "NoChannel", "buttonMode": "Momentary",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
      ]
    }]
  })");
  currentMidiChannel = 9;

  triggerButton(0);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 9);
}

TEST_F(ButtonBehaviorTest, PlayNotesAction_WithVelocity_OverridesEveryNotesDefaultVelocity) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Chord", "buttonMode": "Momentary",
          "actions": [ { "type": "PlayNotes",
            "actionParameters": { "noteNames": ["C3", "E3", "G3"], "velocity": 42 } } ] }
      ]
    }]
  })");

  triggerButton(0);

  // all three notes should have been sent with the configured velocity
  // (42), not Note's own default (99, see Note.h)
  ASSERT_EQ(midiLog().size(), 3u);
  EXPECT_EQ(midiLog()[0].b, 42);
  EXPECT_EQ(midiLog()[1].b, 42);
  EXPECT_EQ(midiLog()[2].b, 42);
}

TEST_F(ButtonBehaviorTest, PlayNotesAction_WithoutVelocity_UsesEachNotesOwnDefault) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Chord", "buttonMode": "Momentary",
          "actions": [ { "type": "PlayNotes", "actionParameters": { "noteNames": ["C3"] } } ] }
      ]
    }]
  })");

  triggerButton(0);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].b, 99); // Note's own default velocity - see Note.h
}

TEST_F(ButtonBehaviorTest, NoteAction_WithOwnMidiChannel_OverridesCurrentMidiChannel) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "FixedChannel", "buttonMode": "Momentary",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 4 } } ] }
      ]
    }]
  })");
  currentMidiChannel = 9;

  triggerButton(0);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 4);
}

//----------------------------------------------------------------------------//
//    Tests for MIDI channel inheritance: Patch.midiChannel and              //
//    ModeBank.midiChannel, used by actions that don't configure their own  //
//    channel. Precedence is: action's own channel > current Patch's        //
//    midiChannel > current ModeBank's midiChannel > global                 //
//    currentMidiChannel.                                                   //
//----------------------------------------------------------------------------//

TEST_F(ButtonBehaviorTest, ActionWithoutOwnChannel_UsesPatchMidiChannel_WhenPatchHasOne) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "midiChannel": 5,
        "patches": [{
          "name": "PatchWithChannel",
          "midiChannel": 8,
          "buttons": [
            { "id": 0, "name": "NoOwnChannel", "buttonMode": "Momentary",
              "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
          ]
        }]
      }]
    }]
  })");
  currentMidiChannel = 1;
  currentBankIndex = 0;
  currentPatchIndex = 0;

  triggerButton(0);

  // the Patch's midiChannel (8) wins over both the Bank's (5) and the
  // global one (1)
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 8);
}

TEST_F(ButtonBehaviorTest, ActionWithoutOwnChannel_FallsBackToBankMidiChannel_WhenPatchHasNone) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "midiChannel": 5,
        "patches": [{
          "name": "PatchWithoutChannel",
          "buttons": [
            { "id": 0, "name": "NoOwnChannel", "buttonMode": "Momentary",
              "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
          ]
        }]
      }]
    }]
  })");
  currentMidiChannel = 1;
  currentBankIndex = 0;
  currentPatchIndex = 0;

  triggerButton(0);

  // the Patch has no midiChannel of its own, so the Bank's (5) applies
  // instead of the global one (1)
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 5);
}

TEST_F(ButtonBehaviorTest, ActionWithoutOwnChannel_UsesBankMidiChannel_WhenNoPatchIsSelected) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "midiChannel": 5,
        "buttons": [
          { "id": 1, "name": "BankButton", "buttonMode": "Momentary",
            "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
        ],
        "patches": []
      }]
    }]
  })");
  currentMidiChannel = 1;
  currentBankIndex = 0;
  currentPatchIndex = -1; // no Patch selected

  triggerButton(1);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 5);
}

TEST_F(ButtonBehaviorTest, ActionWithoutOwnChannel_UsesGlobalChannel_WhenNeitherBankNorPatchHasOne) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "NoOwnChannel", "buttonMode": "Momentary",
              "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
          ]
        }]
      }]
    }]
  })");
  currentMidiChannel = 11;
  currentBankIndex = 0;
  currentPatchIndex = 0;

  triggerButton(0);

  // neither the Bank nor the Patch configured a midiChannel, so the
  // globally-selected one is used, same as before this feature existed
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 11);
}

TEST_F(ButtonBehaviorTest, ActionOwnChannel_OverridesBothPatchAndBankMidiChannel) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "midiChannel": 5,
        "patches": [{
          "name": "Patch1",
          "midiChannel": 8,
          "buttons": [
            { "id": 0, "name": "OwnChannel", "buttonMode": "Momentary",
              "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 4 } } ] }
          ]
        }]
      }]
    }]
  })");
  currentMidiChannel = 1;
  currentBankIndex = 0;
  currentPatchIndex = 0;

  triggerButton(0);

  // the action's own channel (4) wins over the Patch's (8), the Bank's
  // (5), and the global one (1)
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 4);
}

//----------------------------------------------------------------------------//
//    Tests for the schema update: Mode-level midiChannel, root-level        //
//    (Config) buttons as the lowest-priority fallback, and the new          //
//    direct/cycling Mode/Bank/Patch and current-parameter actions.          //
//----------------------------------------------------------------------------//

TEST_F(ButtonBehaviorTest, ActionWithoutOwnChannel_FallsBackToModeMidiChannel) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "midiChannel": 7,
      "banks": [{
        "name": "Bank1",
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "ModeChannel", "buttonMode": "Momentary",
              "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"] } } ] }
          ]
        }]
      }]
    }]
  })");
  currentMidiChannel = 1;
  currentBankIndex = 0;
  currentPatchIndex = 0;

  triggerButton(0);

  // neither the action, Patch, nor Bank configured a channel, so the
  // Mode's (7) is used, ahead of the globally-selected one (1)
  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].channel, 7);
}

TEST_F(ButtonBehaviorTest, RootLevelButton_FiresWhenNoMoreSpecificLevelDefinesIt) {
  JsonDocument doc = parseJson(R"({
    "headerColourCode": [0,0,0],
    "buttons": [
      { "id": 3, "name": "Global", "buttonMode": "Momentary",
        "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 42 } } ] }
    ],
    "modes": [ { "name": "Mode1", "banks": [] } ]
  })");
  config = Config(doc);
  currentModeIndex = 0;
  currentBankIndex = -1;
  currentPatchIndex = -1;

  triggerButton(3);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::ProgramChange);
  EXPECT_EQ(midiLog()[0].a, 42);
}

TEST_F(ButtonBehaviorTest, ModeLevelButton_OverridesRootLevelButtonWithSameId) {
  JsonDocument doc = parseJson(R"({
    "headerColourCode": [0,0,0],
    "buttons": [
      { "id": 3, "name": "Global", "buttonMode": "Momentary",
        "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } } ] }
    ],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 3, "name": "ModeSpecific", "buttonMode": "Momentary",
          "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } } ] }
      ],
      "banks": []
    }]
  })");
  config = Config(doc);
  currentModeIndex = 0;
  currentBankIndex = -1;
  currentPatchIndex = -1;

  triggerButton(3);

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].a, 2);
}

TEST_F(ButtonBehaviorTest, SetModeAction_SelectsModeAndResetsBankAndPatch) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [
      {
        "name": "Mode1",
        "buttons": [
          { "id": 0, "name": "GoToMode2", "buttonMode": "Momentary",
            "actions": [ { "type": "SetMode", "actionParameters": { "modeIndex": 1 } } ] }
        ],
        "banks": []
      },
      {
        "name": "Mode2",
        "banks": [ { "name": "OnlyBank", "patches": [ { "name": "OnlyPatch", "buttons": [] } ] } ]
      }
    ]
  })");

  triggerButton(0);

  EXPECT_EQ(currentModeIndex, 1);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
}

// The Cycle*Action classes now move the deferred-navigation pending cursor
// rather than the live selection - that behaviour is covered end-to-end in
// navigation_tests.inc (search for "Action_" there). The old tests here,
// which called the now-legacy cycleMode()/cyclePatch()/cycleBank() runtime
// helpers directly and asserted an immediate live change, have moved there.

TEST_F(ButtonBehaviorTest, SetParameterThenSetParameterValue_SelectsBankByName) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Bank"],
      "banks": [
        { "name": "Verse", "patches": [] },
        { "name": "Chorus", "patches": [] }
      ]
    }]
  })");
  currentBankIndex = 0;

  // SetParameter makes "Bank" the current parameter (it's the only one
  // available here), then SetParameterValue jumps straight to the bank
  // named "Chorus". SetParameterValue is a direct Set, so it still applies
  // immediately (only the Cycle* verbs are deferred) - now via
  // Navigation.h's applyCurrentParameterValue().
  setCurrentParameter(Parameter::BankParameter);
  navSetParameterValue("Chorus");

  EXPECT_EQ(currentBankIndex, 1);
}

//----------------------------------------------------------------------------//
//    Tests for the SetParameter / CycleParameter actions - the two verbs    //
//    that move which system parameter (Patch / Bank / Mode / MidiChannel /  //
//    Config / Page) the parameter-VALUE actions then act on. Both move the  //
//    currentParameterIndex cursor into getCurrentParameters() only; they    //
//    never change the live selection and never send MIDI. See              //
//    SetParameterAction.h / CycleParameterAction.h.                        //
//----------------------------------------------------------------------------//

TEST_F(ButtonBehaviorTest, SetParameterAction_MakesTheNamedParameterCurrent) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank", "Mode"],
      "buttons": [
        { "id": 0, "name": "SelectMode", "buttonMode": "Momentary",
          "actions": [ { "type": "SetParameter", "actionParameters": { "parameterName": "Mode" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentParameterIndex = 0;   // selector starts on "Patch"

  triggerButton(0);

  std::vector<Parameter> &parameters = getCurrentParameters();
  EXPECT_EQ(parameters[currentParameterIndex], Parameter::ModeParameter);
}

TEST_F(ButtonBehaviorTest, SetParameterAction_IgnoresAParameterNotInTheCurrentList) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank"],
      "buttons": [
        { "id": 0, "name": "TryConfig", "buttonMode": "Momentary",
          "actions": [ { "type": "SetParameter", "actionParameters": { "parameterName": "Config" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentParameterIndex = 1;   // on "Bank"

  triggerButton(0);

  // "Config" is not in ["Patch", "Bank"], so setCurrentParameter() leaves
  // the cursor where it was
  std::vector<Parameter> &parameters = getCurrentParameters();
  EXPECT_EQ(parameters[currentParameterIndex], Parameter::BankParameter);
}

TEST_F(ButtonBehaviorTest, SetParameterAction_DoesNotChangeLiveSelectionOrSendMidi) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank"],
      "buttons": [
        { "id": 0, "name": "SelectBank", "buttonMode": "Momentary",
          "actions": [ { "type": "SetParameter", "actionParameters": { "parameterName": "Bank" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentBankIndex = 0;
  currentPatchIndex = 0;
  resetPendingToLive();   // clean (not-dirty) baseline now the live selection is set

  triggerButton(0);

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(ButtonBehaviorTest, CycleParameterAction_Up_MovesToTheNextParameter) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank", "Mode"],
      "buttons": [
        { "id": 0, "name": "NextParam", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleParameter", "actionParameters": { "direction": "Up" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentParameterIndex = 0;   // "Patch"

  triggerButton(0);

  std::vector<Parameter> &parameters = getCurrentParameters();
  EXPECT_EQ(parameters[currentParameterIndex], Parameter::BankParameter);
}

TEST_F(ButtonBehaviorTest, CycleParameterAction_Down_MovesBackwardAndWrapsPastTheStart) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank", "Mode"],
      "buttons": [
        { "id": 0, "name": "PrevParam", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleParameter", "actionParameters": { "direction": "Down" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentParameterIndex = 0;   // "Patch" - Down wraps round to the last entry

  triggerButton(0);

  std::vector<Parameter> &parameters = getCurrentParameters();
  EXPECT_EQ(parameters[currentParameterIndex], Parameter::ModeParameter);
}

TEST_F(ButtonBehaviorTest, CycleParameterAction_DefaultsToUp_WhenNoDirectionIsGiven) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank"],
      "buttons": [
        { "id": 0, "name": "StepParam", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleParameter", "actionParameters": {} } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentParameterIndex = 0;   // "Patch"

  triggerButton(0);

  std::vector<Parameter> &parameters = getCurrentParameters();
  EXPECT_EQ(parameters[currentParameterIndex], Parameter::BankParameter);
}

TEST_F(ButtonBehaviorTest, CycleParameterAction_DoesNotChangeLiveSelectionOrSendMidi) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "parameters": ["Patch", "Bank"],
      "buttons": [
        { "id": 0, "name": "StepParam", "buttonMode": "Momentary",
          "actions": [ { "type": "CycleParameter", "actionParameters": { "direction": "Up" } } ] }
      ],
      "banks": [ { "name": "B1", "patches": [ { "name": "P1", "buttons": [] } ] } ]
    }]
  })");
  currentBankIndex = 0;
  currentPatchIndex = 0;
  resetPendingToLive();   // clean (not-dirty) baseline now the live selection is set

  triggerButton(0);

  EXPECT_EQ(currentModeIndex, 0);
  EXPECT_EQ(currentBankIndex, 0);
  EXPECT_EQ(currentPatchIndex, 0);
  EXPECT_FALSE(navigationDirty());
  EXPECT_TRUE(midiLog().empty());
}

//----------------------------------------------------------------------------//
//    Tests for Button.defaultStatus: a button with "defaultStatus": "On"    //
//    has its actions run once, and is marked on, the moment the Patch/Bank/ //
//    Mode that defines it becomes the current selection - see              //
//    applyDefaultButtonStatuses() in Runtime.h.                            //
//----------------------------------------------------------------------------//

TEST_F(ButtonBehaviorTest, DefaultStatusOn_FiresButtonActionsAndMarksItOn_WhenPatchIsSelected) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "AlwaysOnBoost", "buttonMode": "Latched", "defaultStatus": "On",
              "actions": [ { "type": "ControlChange",
                "actionParameters": { "controllerNumber": 30, "controlValue": 127, "midiChannel": 1 } } ] },
            { "id": 1, "name": "NormalButton", "buttonMode": "Momentary",
              "actions": [ { "type": "ControlChange",
                "actionParameters": { "controllerNumber": 31, "controlValue": 127, "midiChannel": 1 } } ] }
          ]
        }]
      }]
    }]
  })");
  currentBankIndex = 0;

  // landing on the patch fires the default-on button's actions once, and
  // leaves that button - and only that button - marked on
  currentPatchIndex = 0;
  applyDefaultButtonStatuses();  // selectPatch() is gone - this is the part these tests exercise

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::ControlChange);
  EXPECT_EQ(midiLog()[0].a, 30);

  Button *boost = resolveButton(0);
  Button *normal = resolveButton(1);
  ASSERT_NE(boost, nullptr);
  ASSERT_NE(normal, nullptr);
  EXPECT_TRUE(boost->isOn);
  EXPECT_FALSE(normal->isOn);
}

TEST_F(ButtonBehaviorTest, DefaultStatusOff_IsTheDefault_AndDoesNotFireOnSelection) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "Explicit", "buttonMode": "Latched", "defaultStatus": "Off",
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 1 } } ] },
            { "id": 1, "name": "Implicit", "buttonMode": "Latched",
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 2 } } ] }
          ]
        }]
      }]
    }]
  })");
  currentBankIndex = 0;

  currentPatchIndex = 0;
  applyDefaultButtonStatuses();  // selectPatch() is gone - this is the part these tests exercise

  // nothing was sent, and neither button is on
  EXPECT_EQ(midiLog().size(), 0u);
  EXPECT_FALSE(resolveButton(0)->isOn);
  EXPECT_FALSE(resolveButton(1)->isOn);
}

TEST_F(ButtonBehaviorTest, DefaultStatusOn_DoesNotRefireWhenReselectingTheSamePatch) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "patches": [{
          "name": "Patch1",
          "buttons": [
            { "id": 0, "name": "Boost", "buttonMode": "Latched", "defaultStatus": "On",
              "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 5 } } ] }
          ]
        }]
      }]
    }]
  })");
  currentBankIndex = 0;

  currentPatchIndex = 0;
  applyDefaultButtonStatuses();  // selectPatch() is gone - this is the part these tests exercise
  ASSERT_EQ(midiLog().size(), 1u);

  // selecting the same patch again finds the button already on, so it must
  // not run its actions a second time
  clearMidiLog();
  currentPatchIndex = 0;
  applyDefaultButtonStatuses();  // selectPatch() is gone - this is the part these tests exercise

  EXPECT_EQ(midiLog().size(), 0u);
  EXPECT_TRUE(resolveButton(0)->isOn);
}

TEST_F(ButtonBehaviorTest, DefaultStatusOn_AtRootLevel_FiresOnConfigLoad) {
  JsonDocument doc = parseJson(R"({
    "headerColourCode": [0,0,0],
    "buttons": [
      { "id": 4, "name": "GlobalOn", "buttonMode": "Latched", "defaultStatus": "On",
        "actions": [ { "type": "ProgramChange", "actionParameters": { "programNumber": 9 } } ] }
    ],
    "modes": [ { "name": "Mode1", "banks": [] } ]
  })");
  config = Config(doc);

  // resetRuntimeSelection() runs as part of every config load - it settles
  // Mode/Bank/Patch and then applies default button statuses
  resetRuntimeSelection();

  ASSERT_EQ(midiLog().size(), 1u);
  EXPECT_EQ(midiLog()[0].kind, MidiCall::ProgramChange);
  EXPECT_EQ(midiLog()[0].a, 9);
  EXPECT_TRUE(resolveButton(4)->isOn);
}

TEST_F(ButtonBehaviorTest, PatchLevelParameters_OverrideBankLevelParameters) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "banks": [{
        "name": "Bank1",
        "parameters": ["Bank"],
        "patches": [ { "name": "Patch1", "parameters": ["MidiChannel"], "buttons": [] } ]
      }]
    }]
  })");
  currentBankIndex = 0;
  currentPatchIndex = 0;

  std::vector<Parameter> &parameters = getCurrentParameters();

  ASSERT_EQ(parameters.size(), 1u);
  EXPECT_EQ(parameters[0], Parameter::MIDIChannelParameter);
}

// ---- anyButtonActive(): used to refuse a restart while notes may be sounding ----

TEST_F(ButtonBehaviorTest, AnyButtonActive_IsFalseWithNoConfigLoaded) {
  EXPECT_FALSE(anyButtonActive());
}

TEST_F(ButtonBehaviorTest, AnyButtonActive_IsTrueWhileAButtonIsHeld_AndFalseOnceReleased) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Note0", "buttonMode": "Momentary",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] }
      ]
    }]
  })");
  EXPECT_FALSE(anyButtonActive());

  triggerButton(0);
  EXPECT_TRUE(anyButtonActive());

  releaseButton(0);
  EXPECT_FALSE(anyButtonActive());
}

TEST_F(ButtonBehaviorTest, ReleaseAllActiveButtons_SendsTheNoteOffsAndTurnsEverythingOff) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Drone", "buttonMode": "Latched",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] },
        { "id": 1, "name": "Held", "buttonMode": "Momentary",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["E3"], "midiChannel": 1 } } ] }
      ]
    }]
  })");
  triggerButton(0);
  releaseButton(0); // latched: stays on
  triggerButton(1); // momentary: still held
  ASSERT_TRUE(anyButtonActive());
  clearMidiLog();

  releaseAllActiveButtons();

  EXPECT_FALSE(anyButtonActive());
  // both notes got their note-off
  int noteOffs = 0;
  for (const MidiCall &call : midiLog()) {
    if (call.kind == MidiCall::NoteOff) noteOffs++;
  }
  EXPECT_EQ(noteOffs, 2);
}

TEST_F(ButtonBehaviorTest, ReleaseAllActiveButtons_WithNoConfigDoesNothing) {
  releaseAllActiveButtons();
  EXPECT_TRUE(midiLog().empty());
}

TEST_F(ButtonBehaviorTest, AnyButtonActive_IsTrueForALatchedButtonLeftOn) {
  loadConfig(R"({
    "headerColourCode": [0,0,0],
    "modes": [{
      "name": "Mode1",
      "buttons": [
        { "id": 0, "name": "Drone", "buttonMode": "Latched",
          "actions": [ { "type": "Note", "actionParameters": { "noteNames": ["C3"], "midiChannel": 1 } } ] }
      ]
    }]
  })");
  triggerButton(0);
  releaseButton(0);
  // a latched button stays on after the footswitch is released
  EXPECT_TRUE(anyButtonActive());

  triggerButton(0); // second press switches it off
  releaseButton(0);
  EXPECT_FALSE(anyButtonActive());
}
