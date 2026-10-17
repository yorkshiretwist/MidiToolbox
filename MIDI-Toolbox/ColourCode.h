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

// include guard - see the top of Note.h for a full explanation
#ifndef ColourCode_h
#define ColourCode_h

#include <string>
#include <math.h>
#include <ArduinoJson.h>

// represents a single RBG colour code
class ColourCode {
  public:
    // three plain fields, no default value given - see Patch.h/Mode.h for
    // more on what that means for a fresh ColourCode object
    int red;
    int green;
    int blue;

    // constructs a new ColourCode
    //
    // an empty-bodied constructor ("{}") taking no parameters - similar to
    // writing "public ColourCode() {}" in C#. Fields aren't set to
    // anything specific here (see above), so a ColourCode created this way
    // starts with red/green/blue containing whatever garbage was already
    // in memory - this constructor only really makes sense as a stepping
    // stone before assigning real values, e.g. inside Patch/Mode/ModeBank's
    // own JSON-reading constructors.
    ColourCode(){}

    // constructs a new ColourCode from three explicit 0-255 components -
    // used to set the built-in default colours (see Config.h's
    // headerColourCode and ModeBank.h's subheaderColourCode) without
    // needing a JsonArray to read them from
    ColourCode(int redValue, int greenValue, int blueValue) {
      red = redValue;
      green = greenValue;
      blue = blueValue;
    }

    // constructs a new ColourCode using the data in the given JsonArray
    //
    // JsonArray is an ArduinoJson type representing a JSON array (like
    // config.json's "[255, 0, 128]") - roughly comparable to a C#
    // JArray or a Python list pulled out of parsed JSON.
    ColourCode(JsonArray jsonArray) {
      // "jsonArray[0]" indexes into the JSON array like a C# array or
      // Python list ([0] is the first item). ".as<JsonVariant>().as<int>()"
      // converts that item first to a generic JSON value, then to an int -
      // similar to C#'s (int)someObject or Python's int(some_value).
      red = jsonArray[0].as<JsonVariant>().as<int>();
      green = jsonArray[1].as<JsonVariant>().as<int>();
      blue = jsonArray[2].as<JsonVariant>().as<int>();
    }

    // A regular method (not a constructor) - like a method in a C# class or
    // Python class. "uint16_t" is a 16-bit unsigned integer type (holds
    // values 0-65535) - C++/embedded code often uses these precisely-sized
    // integer types instead of a plain "int" when the exact number of bits
    // matters (here, because a specific colour format needs exactly 16
    // bits packed a certain way); C# and Python don't normally make you
    // think about this.
    //
    // get the RGB565 value (the native pixel format for the ILI9341, and
    // what most display driver libraries - including LVGL's - expect)
    uint16_t getValue() {
      // packs the three 0-255 colour components into a single 16-bit
      // number using bitwise operators: "&" masks off unwanted bits,
      // "<<"/">>" shift bits left/right, and "|" combines the pieces
      // together. C# and Python both support these same bitwise operators
      // (&, |, <<, >>) with the same meaning, but code like this - packing
      // several small numbers into one larger one - comes up far more
      // often in embedded/C++ code than in typical C#/Python code.
      return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
    }

    // get the value as a string
    String getString() {
      // "(String)red" converts an int to a String - similar to C#'s
      // red.ToString() or Python's str(red). The + operator then
      // concatenates String pieces together, same as C#/Python's +
      // for strings.
      return "[" + (String)red + "," + (String)green + "," + (String)blue + "]";
    }

    // This colour's relative luminance, as defined by the WCAG accessibility
    // guidelines: 0.0 for black, 1.0 for white. It's a measure of how bright
    // the colour looks to the human eye - green counts for far more than
    // blue because our eyes are far more sensitive to it. contrastRatio()
    // (below the class) builds on this to compare two colours.
    //
    // "double" (not int) because every step here works in fractions between
    // 0 and 1 - C#/Python would use double/float here too. "const" after the
    // "()" promises this method only reads the object, never changes it.
    double relativeLuminance() const {
      return 0.2126 * srgbToLinear(red)
           + 0.7152 * srgbToLinear(green)
           + 0.0722 * srgbToLinear(blue);
    }

  private:
    // Converts one 0-255 sRGB colour channel into "linear light" - undoing
    // the gamma curve that displays bake into colour values - which is the
    // per-channel first step of the relativeLuminance() formula above.
    // "pow(x, 2.4)" raises x to the power 2.4, the same "pow" as C#'s
    // Math.Pow or Python's "x ** 2.4" / math.pow.
    static double srgbToLinear(int channel) {
      double c = channel / 255.0;
      if (c <= 0.03928) {
        return c / 12.92;
      }
      return pow((c + 0.055) / 1.055, 2.4);
    }
};

// ----------------------------------------------------------------------------
// Colour contrast helpers
//
// Free functions (not tied to a single ColourCode instance), kept here next
// to ColourCode so anywhere in the project that works with colours can reach
// for them - the header/screen bars are the first users, but the same
// "which foreground colour reads best on this fill?" question comes up
// wherever colours from a config file get drawn.

// The WCAG contrast ratio between two colours: 1.0 when they're identical,
// rising to 21.0 for black against pure white. Symmetric - the argument
// order doesn't matter. As a rough guide, the guidelines want at least 4.5
// for ordinary body text and 3.0 for large text.
double contrastRatio(const ColourCode &a, const ColourCode &b) {
  double la = a.relativeLuminance();
  double lb = b.relativeLuminance();
  // the formula is (lighter + 0.05) / (darker + 0.05), so sort the two
  // luminances rather than assuming which is which
  double lighter = (la > lb) ? la : lb;
  double darker  = (la > lb) ? lb : la;
  return (lighter + 0.05) / (darker + 0.05);
}

// Picks pure black or pure white - whichever a caller should draw text (or
// any small foreground detail) in so it stands out most against
// `background`. "Greater contrast wins": a yellow fill gets black text, a
// dark-grey fill gets white text. A dead tie (vanishingly unlikely) goes to
// white.
ColourCode contrastingTextColour(const ColourCode &background) {
  ColourCode black(0, 0, 0);
  ColourCode white(255, 255, 255);
  return (contrastRatio(background, white) >= contrastRatio(background, black)) ? white : black;
}

// matches #ifndef/#define above
#endif
