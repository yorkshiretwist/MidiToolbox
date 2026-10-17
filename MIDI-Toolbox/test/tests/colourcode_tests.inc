//----------------------------------------------------------------------------//
//    Tests for ColourCode.h - JSON parsing and RGB565 packing.              //
//----------------------------------------------------------------------------//

TEST(ColourCodeTest, Constructor_FromJsonArray_SetsRedGreenBlue) {
  JsonDocument doc = parseJson("[255, 128, 0]");
  ColourCode colour(doc.as<JsonArray>());

  EXPECT_EQ(colour.red, 255);
  EXPECT_EQ(colour.green, 128);
  EXPECT_EQ(colour.blue, 0);
}

TEST(ColourCodeTest, GetValue_PacksIntoRgb565) {
  JsonDocument doc = parseJson("[255, 255, 255]");
  ColourCode white(doc.as<JsonArray>());

  // full white in RGB565 is all bits set: 0xFFFF
  EXPECT_EQ(white.getValue(), 0xFFFF);
}

TEST(ColourCodeTest, GetValue_BlackPacksToZero) {
  JsonDocument doc = parseJson("[0, 0, 0]");
  ColourCode black(doc.as<JsonArray>());

  EXPECT_EQ(black.getValue(), 0x0000);
}

TEST(ColourCodeTest, GetValue_MatchesKnownRgb565Conversion) {
  // a mid-range colour, cross-checked against the standard RGB565 formula
  // by hand: R(0xF8)=0xE0 << 8, G(0xFC)=0x1C << 3, B >> 3 = 0x1D
  JsonDocument doc = parseJson("[224, 226, 234]");
  ColourCode colour(doc.as<JsonArray>());

  uint16_t expected = ((224 & 0xF8) << 8) | ((226 & 0xFC) << 3) | (234 >> 3);
  EXPECT_EQ(colour.getValue(), expected);
}

TEST(ColourCodeTest, GetString_FormatsAsCommaSeparatedList) {
  JsonDocument doc = parseJson("[1, 2, 3]");
  ColourCode colour(doc.as<JsonArray>());

  EXPECT_EQ(colour.getString(), "[1,2,3]");
}
