//----------------------------------------------------------------------------//
//    Tests for Note.h - note name parsing (noteNameToMidiNumber) and the    //
//    Note class's JSON constructor.                                         //
//----------------------------------------------------------------------------//

TEST(NoteTest, NoteNameToMidiNumber_NaturalNotes_UseC4Equals60Convention) {
  // C4 = 60 is the widely-used "middle C" convention this project follows
  EXPECT_EQ(noteNameToMidiNumber("C4"), 60);
  EXPECT_EQ(noteNameToMidiNumber("C-1"), 0);
  EXPECT_EQ(noteNameToMidiNumber("G9"), 127);
}

TEST(NoteTest, NoteNameToMidiNumber_AllNaturalLettersInOneOctave) {
  EXPECT_EQ(noteNameToMidiNumber("C3"), 48);
  EXPECT_EQ(noteNameToMidiNumber("D3"), 50);
  EXPECT_EQ(noteNameToMidiNumber("E3"), 52);
  EXPECT_EQ(noteNameToMidiNumber("F3"), 53);
  EXPECT_EQ(noteNameToMidiNumber("G3"), 55);
  EXPECT_EQ(noteNameToMidiNumber("A3"), 57);
  EXPECT_EQ(noteNameToMidiNumber("B3"), 59);
}

TEST(NoteTest, NoteNameToMidiNumber_SharpRaisesBySemitone) {
  EXPECT_EQ(noteNameToMidiNumber("C#4"), noteNameToMidiNumber("C4") + 1);
  EXPECT_EQ(noteNameToMidiNumber("F#2"), noteNameToMidiNumber("F2") + 1);
}

TEST(NoteTest, NoteNameToMidiNumber_FlatLowersBySemitone) {
  EXPECT_EQ(noteNameToMidiNumber("Db4"), noteNameToMidiNumber("D4") - 1);
  EXPECT_EQ(noteNameToMidiNumber("Ab2"), noteNameToMidiNumber("A2") - 1);
}

TEST(NoteTest, NoteNameToMidiNumber_EnharmonicEquivalentsMatch) {
  // C#4 and Db4 are the same physical note - the parser should agree
  EXPECT_EQ(noteNameToMidiNumber("C#4"), noteNameToMidiNumber("Db4"));
}

TEST(NoteTest, NoteNameToMidiNumber_InvalidLetterReturnsMinusOne) {
  EXPECT_EQ(noteNameToMidiNumber("H4"), -1);
  EXPECT_EQ(noteNameToMidiNumber("X4"), -1);
}

TEST(NoteTest, NoteNameToMidiNumber_TooShortReturnsMinusOne) {
  EXPECT_EQ(noteNameToMidiNumber("C"), -1);
  EXPECT_EQ(noteNameToMidiNumber(""), -1);
}

TEST(NoteTest, Constructor_FromJsonString_SetsNameAndMidiNoteNumber) {
  JsonDocument doc = parseJson("\"E3\"");
  Note note(doc.as<JsonVariant>());

  EXPECT_EQ(note.name, "E3");
  EXPECT_EQ(note.midiNoteNumber, 52);
}

TEST(NoteTest, Constructor_Default_HasSensibleDefaults) {
  Note note;

  // an unconfigured Note shouldn't be mistaken for a real, playable one
  EXPECT_EQ(note.midiNoteNumber, -1);
  EXPECT_EQ(note.velocity, 99);
  EXPECT_EQ(note.midiChannel, 1);
}
