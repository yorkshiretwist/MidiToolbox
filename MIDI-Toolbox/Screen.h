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
//                                                                            //
//    UNTESTED ON REAL HARDWARE - this is a first-pass skeleton, sketched     //
//    out and compile-checked against real LVGL v9.5 source and the real     //
//    ILI9341_T4 API surface, but not yet run against an actual Teensy 4.1   //
//    + screen. Treat pin numbers, timing, and layout as a starting point    //
//    to verify during bring-up, not as finished, tested values.             //
//                                                                            //
//    Drives an ILI9341 touchscreen via vindar's ILI9341_T4 driver (Teensy    //
//    4/4.1 only) and renders the UI with LVGL. Replaces the old ILI9341_t3-  //
//    based direct drawing in the .ino. This file only implements the        //
//    ILI9341 - see the SCREEN_DRIVER_ILI9341_T4 check right after the       //
//    include guard below - so a unit built around a different screen needs //
//    a driver adapter written for it first (see BUILD.md, downloaded        //
//    alongside a generated MidiToolboxDevice.h) before this file will even  //
//    compile against it.                                                   //
//                                                                            //
//    Its resolution, orientation, and every pin it uses come from           //
//    MidiToolboxDevice.h, not from constants written out here - the         //
//    reference unit is a 320x240 panel in landscape, but nothing in this    //
//    file assumes that specific size or orientation.                       //
//                                                                            //
//    ILI9341_T4 handles the XPT2046 touch controller itself when            //
//    PIN_TOUCH_CS/PIN_TOUCH_IRQ are passed to its constructor - no separate //
//    touch library is needed.                                              //
//                                                                            //
//    Like Runtime.h and ConfigStore.h, this lives in a .h file rather than  //
//    directly in the .ino: Arduino's automatic function-prototype           //
//    generator breaks on functions whose signatures use custom types        //
//    (Mode, Parameter...), and display() here needs several of those. See  //
//    the comment at the top of Runtime.h for the full explanation.          //
//                                                                            //
//    Assumes it's included from MIDI-Toolbox.ino after Config.h, after      //
//    Pins.h (which pulls in MidiToolboxDevice.h - see its own comment),     //
//    and after the "Runtime variables" section (config, currentModeIndex    //
//    etc.) and Runtime.h (getCurrentMode() / getCurrentBank() /             //
//    getCurrentPatch()) - it uses all of those directly.                    //
//                                                                            //
//----------------------------------------------------------------------------//

// include guard - see the top of Note.h for a full explanation
#ifndef Screen_h
#define Screen_h

// two third-party libraries: ILI9341_T4 (the screen/touch driver) and lvgl
// (the graphics/UI library that draws everything). Neither is written in
// this project or part of core Arduino - both were installed separately
// via the Arduino Library Manager, the same way you'd add a NuGet package
// in C# or a pip package in Python.
#include <ILI9341_T4.h>
#include <lvgl.h>
// for intptr_t - each main-grid cell stashes its physical button id in its
// LVGL event user-data pointer (see createScreenWidgets())
#include <stdint.h>

// This file only has a driver for the ILI9341 (via the ILI9341_T4 library
// below) - MidiToolboxDevice.h defines exactly one SCREEN_DRIVER_* macro,
// naming which screen controller THIS unit uses (see its own comment), and
// this is a compile-time check that it's the one this file knows how to
// drive. A unit built around a different screen needs a driver adapter
// written for it first - see BUILD.md (from the Designer app) for what that
// involves - rather than silently compiling against the wrong pin/timing
// assumptions.
#if !defined(SCREEN_DRIVER_ILI9341_T4)
#error "Screen.h only implements the ILI9341_T4 driver, but MidiToolboxDevice.h selects a different screen. Write a driver adapter for it (see BUILD.md), or point MidiToolboxDevice.h at an ILI9341 screen."
#endif

// ============================================================================
// wiring
//
// Every pin below - and SCREEN_LX/SCREEN_LY, the panel's resolution - comes
// from MidiToolboxDevice.h (see Pins.h, included earlier in the .ino, for
// where that's pulled in). This file just wires them into the driver;
// double-check them against your own build before power-up regardless -
// see the "MEASURE YOUR PARTS BEFORE YOU DRILL" warning the Designer app
// puts on every download.
//
// IMPORTANT (per the ILI9341_T4 docs): SCREEN_PIN_DC must be a pin capable
// of acting as a hardware CS pin on the SPI bus it's wired to for full
// speed - worth confirming during bring-up; if not, an alternate DC pin
// (one of the other valid CS-capable pins on the same SPI bus) will be
// needed.
#define SCREEN_SPI_SPEED  40000000

// ============================================================================
// buffers and driver objects

// "ILI9341_T4::DiffBuffStatic<8000>" - "ILI9341_T4::" is a namespace
// (roughly like a C# namespace, or the module part of a Python
// "module.ClassName") grouping together everything that library provides,
// so its names don't clash with anything else in this project. The
// "<8000>" part is a template argument - DiffBuffStatic is a class that
// takes a number as a compile-time parameter (its buffer size in bytes),
// somewhat like a C# generic List<T> takes a type, except here it's taking
// a plain number instead. Two separate 8000-byte buffers are created so
// the library can prepare the next screen update while still sending the
// previous one.
//
// 2 diff buffers, ~8KB each - used by ILI9341_T4 for partial-update tracking
ILI9341_T4::DiffBuffStatic<8000> screenDiff1;
ILI9341_T4::DiffBuffStatic<8000> screenDiff2;

// "uint16_t screenFramebuffer[SCREEN_LX * SCREEN_LY];" - a plain, fixed-size
// C-style array (see ConfigStore.h's note on "char buffer[]" for more on
// this array style), one uint16_t (16-bit number) per pixel of the screen
// (SCREEN_LX * SCREEN_LY pixels, ~2 bytes each - 150KB for the reference
// unit's 320x240 panel; a larger screen needs proportionally more).
// "DMAMEM" is a Teensy-specific macro that tells the compiler to place this
// array in a particular region of the chip's RAM (RAM2) reserved for large
// buffers like this one, leaving the faster RAM1 free for everything else
// the program needs - there's no C#/Python equivalent, since neither
// language normally lets you choose which physical memory bank something
// lives in.
//
// the driver's internal framebuffer - placed in DMAMEM (RAM2) to leave RAM1
// free for the JSON config, the Mode/Bank/Patch/Button tree, etc.
DMAMEM uint16_t screenFramebuffer[SCREEN_LX * SCREEN_LY];

// constructs the driver object (see Pins.h's note on stack-allocated
// objects) that talks to the physical screen and touch controller,
// wiring it up to all the pins defined above
//
// the screen + touch driver
ILI9341_T4::ILI9341Driver screenDriver(SCREEN_PIN_CS, SCREEN_PIN_DC, SCREEN_PIN_SCK, SCREEN_PIN_MOSI,
                                        SCREEN_PIN_MISO, SCREEN_PIN_RESET, SCREEN_PIN_T_CS, SCREEN_PIN_T_IRQ);

// LVGL's own draw buffer - a partial buffer (40 lines) is enough; LVGL
// renders into this, then hands it to screenFlushCb() below, which pushes
// it into screenDriver's framebuffer and on to the physical screen
#define SCREEN_LVGL_BUF_LINES 40
// "lv_color_t" is LVGL's own pixel type (defined by the library, sized
// according to how LVGL was configured - 16-bit here, to match the
// ILI9341's native format)
lv_color_t screenLvglBuffer[SCREEN_LX * SCREEN_LVGL_BUF_LINES];

// "lv_display_t *screenDisplay = nullptr;" - a pointer (see Runtime.h for
// a full explanation of pointers) that starts out null and gets pointed at
// a real LVGL-managed object once initScreen() runs. LVGL, like several
// C libraries used in this project, hands back pointers to its own
// internal objects rather than letting you create them directly - you
// call a function like lv_display_create() and it returns a pointer you
// then use for everything else.
lv_display_t *screenDisplay = nullptr;
lv_indev_t *screenIndev = nullptr;

// ============================================================================
// LVGL callbacks
//
// A "callback" is a function you hand to some other piece of code (here,
// LVGL) so that IT can call YOUR function later, when something relevant
// happens - you don't call these functions yourself anywhere in this
// project. This is the same idea as a C# delegate/Action/event handler, or
// passing a function as an argument in Python (e.g. to sorted(key=...));
// C++ can pass a plain function this way too, as seen in initScreen()
// below (lv_display_set_flush_cb(screenDisplay, screenFlushCb) - note
// screenFlushCb is passed WITHOUT parentheses, meaning "here's the
// function itself", not "call it and pass its result").

// pushes LVGL's rendered pixels to the screen
void screenFlushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const bool redrawNow = lv_display_flush_is_last(disp);
  // "(uint16_t *)px_map" is a "cast" - explicitly telling the compiler
  // "treat this pointer as if it points to uint16_t values instead of
  // uint8_t ones". C# has an equivalent explicit cast syntax
  // ("(SomeType)someValue"); C++ additionally allows this particular,
  // fairly blunt style of cast (a "C-style cast") inherited from C, which
  // can reinterpret raw memory this freely - something C#/Python
  // deliberately don't allow outside of unsafe/low-level escape hatches,
  // since it bypasses their normal type safety.
  screenDriver.updateRegion(redrawNow, (uint16_t *)px_map, area->x1, area->x2, area->y1, area->y2);
  // tells LVGL "I'm done with this buffer, you can reuse it" - required by
  // LVGL's flush callback contract
  lv_display_flush_ready(disp);
}

// reports touch state to LVGL
void screenTouchReadCb(lv_indev_t *indev, lv_indev_data_t *data) {
  // this callback's signature is fixed by LVGL (it always passes both
  // parameters), but this particular implementation doesn't need "indev" -
  // "(void)indev;" is a no-op that silences the "unused parameter"
  // compiler warning, same idea as in SetParameterAction.h
  (void)indev;
  int touchX, touchY;
  // "screenDriver.readTouch(touchX, touchY)" - touchX/touchY are passed by
  // reference here (see NoteAction.h for what "&" reference parameters
  // mean) even though there's no "&" visible at the call site - that's
  // because readTouch()'s own declaration (in the ILI9341_T4 library) is
  // what decides this, not the code calling it. This lets the function
  // "return" two values at once by writing into touchX/touchY directly,
  // while also using its actual return value (true/false) to report
  // whether the screen is being touched at all - something a C# method
  // could do with "out" parameters, and Python would more commonly do by
  // just returning a tuple like (touched, x, y) instead.
  if (screenDriver.readTouch(touchX, touchY)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// "static" on a free function like this limits it to being visible only
// within this one file - similar in spirit to a "private" top-level
// function, though C++'s free-function "static" and a class's "private"
// (see NoteAction.h) are different mechanisms achieving a related goal.
// There's no real equivalent in Python (nothing is ever truly file-private
// there); C# achieves the similar effect of "not visible outside this
// file" differently, typically via "internal" combined with how projects
// are organised into assemblies.
//
// LVGL's tick source
static uint32_t screenTickCb() {
  return millis();
}

// ============================================================================
// widgets
//
// The screen is 320x240, landscape. This first slice of the layout builds
// the two bars across the top; everything below them is added later (the
// parameter row, the button grid, and the touch handling).
//
//   +----------------------------------------------------+
//   | Header    <config name>              <Mode name>   |  <- headerColourCode bg
//   +----------------------------------------------------+
//   | SubHeader <Bank name>               <Patch name>   |  <- selected bank's
//   +----------------------------------------------------+     subheaderColourCode bg
//   |                                                    |
//   |   (rest of the screen - added later - showing      |
//   |    only the grey-to-black background gradient      |
//   |    for now)                                        |
//   +----------------------------------------------------+

// heights of the two top bars, in pixels, and the padding that insets each
// bar's text from the left/right screen edge
#define SCREEN_HEADER_H      28
#define SCREEN_SUBHEADER_H   40
#define SCREEN_EDGE_PAD       8

// the parameter row sits just below the SubHeader, with a small gap of
// background showing between them. It holds three buttons (mirroring the
// physical device.navDownId / device.navSelectId / device.navUpId footswitches - see the
// .ino) in a centred group, plus the "Ch. N" MIDI-channel readout aligned to
// the right edge. The middle button is wide enough for the longest
// parameter name ("MidiChannel"); the two arrow buttons stay narrow.
#define SCREEN_SUBHEADER_GAP     4
#define SCREEN_PARAMROW_Y        (SCREEN_HEADER_H + SCREEN_SUBHEADER_H + SCREEN_SUBHEADER_GAP)
#define SCREEN_PARAMROW_H        34
#define SCREEN_PARAMBTN_ARROW_W  40
#define SCREEN_PARAMBTN_NAME_W   112

// the main button grid: device.gridRows rows over device.gridHalfCols
// half-columns (from `device`, see DeviceProfile.h) filling the screen below the
// parameter row, one cell per performance button - arranged to match the
// unit's own face; see device.gridButtonIds below for exactly which id
// starts in which half-column (255 marks a half-column no cell starts in).
// Every cell is two half-columns wide, so a row of buttons that is staggered
// against the next one (offset by half a button spacing) is drawn offset by
// half a cell, and every cell stays as wide as the screen allows.
// SCREEN_BTNGRID_GAP is the margin round the grid and the gutter between
// cells; cell width/height are whatever's left over once those gaps are taken
// out, so the grid always spans the full width and reaches the bottom edge,
// however many rows/columns it has.
//
// A half-column is (SCREEN_LX - GAP) / HALF_COLS wide, gutter included.
// SCREEN_BTNGRID_X() works each edge out from the column index rather than
// stacking rounded widths, so odd pixel counts never drift across a row.
#define SCREEN_BTNGRID_GAP       4
#define SCREEN_BTNGRID_Y         (SCREEN_PARAMROW_Y + SCREEN_PARAMROW_H + SCREEN_BTNGRID_GAP)
#define SCREEN_BTNGRID_X(col)    (SCREEN_BTNGRID_GAP + ((col) * (SCREEN_LX - SCREEN_BTNGRID_GAP)) / device.gridHalfCols)
#define SCREEN_BTNGRID_CELL_W(col) (SCREEN_BTNGRID_X((col) + 2) - SCREEN_BTNGRID_GAP - SCREEN_BTNGRID_X(col))
#define SCREEN_BTNGRID_CELL_H    ((SCREEN_LY - SCREEN_BTNGRID_Y - SCREEN_BTNGRID_GAP * device.gridRows) / device.gridRows)

// a main button drawn in its colour at 20% opacity means it's off; at full
// opacity it's on. LV_OPA_20 is LVGL's own name for ~51/255.
#define SCREEN_BTNGRID_OFF_OPA   LV_OPA_20
// the same 0.20 as a fraction, for blending the off colour over the near-
// black grid background when picking a readable label colour (see
// screenBlendOver() / screenSyncMainButtons())
#define SCREEN_BTNGRID_OFF_ALPHA 0.20

// the footswitch LED behind an "off" main button is lit at this fraction of
// its full colour. Lower than the on-screen cell's 20% on purpose: a diffused
// panel LED at 20% still reads as "on" across a dark stage, so the off state
// needs to sit dimmer to stay clearly distinct from a lit button. This is a
// linear PWM duty, and the eye reads brightness roughly as duty^0.45, so the
// old 10% duty looked about 35% bright. 2% duty looks about 17%.
#define SCREEN_LED_OFF_BRIGHTNESS 0.02

// the startup screen's progress bar: a plain white 4px-tall bar, its left
// edge fixed at SCREEN_STARTUP_PROGRESS_X, growing rightwards from 0 up to
// SCREEN_STARTUP_PROGRESS_MAXW (80% of the screen width, so it's centred as
// a whole) as screenSetStartupProgress() is called through boot.
#define SCREEN_STARTUP_PROGRESS_H     4
#define SCREEN_STARTUP_PROGRESS_GAP   8
#define SCREEN_STARTUP_PROGRESS_MAXW  ((SCREEN_LX * 8) / 10)
#define SCREEN_STARTUP_PROGRESS_X     ((SCREEN_LX - SCREEN_STARTUP_PROGRESS_MAXW) / 2)

// the startup-failure screen's red error panel: inset this many pixels from
// the screen's left/right/bottom edges and from the bottom of the wordmark
#define SCREEN_STARTUP_FAILURE_INSET  20

// "lv_obj_t" is LVGL's base type for absolutely everything drawn on
// screen - labels, buttons, containers, etc. are all "lv_obj_t" under the
// hood, similar to how every UI element in many GUI frameworks (WPF's
// UIElement, or a base Widget class) ultimately shares one common base
// type. Every one of these globals is a pointer, starting out null until
// createScreenWidgets() below actually creates the real object and points
// each variable at it.

// the two coloured bars...
lv_obj_t *screenHeader = nullptr;
lv_obj_t *screenSubHeader = nullptr;

// ...and the four labels sat inside them
lv_obj_t *screenConfigLabel = nullptr;  // Header,    left  - config filename, no ".json"
lv_obj_t *screenModeLabel = nullptr;    // Header,    right - current Mode name
lv_obj_t *screenBankLabel = nullptr;    // SubHeader, left  - current Bank name
lv_obj_t *screenPatchLabel = nullptr;   // SubHeader, right - current Patch name

// the startup screen - a full-screen [85,85,85]->[0,0,0] vertical-gradient
// panel with the MIDI Toolbox wordmark and a progress bar centred on it.
// Created by showStartupMessage() before the config file has loaded, and
// deleted by the first display() call once the real UI has something to
// show. screenStartupScreen is the panel itself (it sits on top of, and
// hides, the Header / SubHeader / parameter row that createScreenWidgets()
// already built); screenStartupLabel is the wordmark label and
// screenStartupProgress the progress bar, both parented to it. All three are
// kept as globals so display(), screenSetStartupProgress() and
// showStartupFailure() can find and use them.
lv_obj_t *screenStartupScreen = nullptr;
lv_obj_t *screenStartupLabel = nullptr;
lv_obj_t *screenStartupProgress = nullptr;

// the parameter row: a transparent full-width container holding the three
// parameter buttons and the MIDI-channel label. The buttons correspond to
// the physical device.navDownId (left / value down), device.navSelectId (middle /
// cycle parameter) and device.navUpId (right / value up) footswitches; tapping
// them on the touchscreen runs the same actions (see the *Cb callbacks
// below).
lv_obj_t *screenParamRow = nullptr;
lv_obj_t *screenParamDownBtn = nullptr;    // physical id device.navDownId
lv_obj_t *screenParamNameBtn = nullptr;    // physical id device.navSelectId
lv_obj_t *screenParamUpBtn = nullptr;      // physical id device.navUpId
lv_obj_t *screenParamDownLabel = nullptr;  // holds LV_SYMBOL_LEFT
lv_obj_t *screenParamNameLabel = nullptr;  // holds the current parameter name
lv_obj_t *screenParamUpLabel = nullptr;    // holds LV_SYMBOL_RIGHT
lv_obj_t *screenChannelLabel = nullptr;    // "Ch. N", right-aligned

// LVGL timer that re-runs screenSyncParamEdit() and screenSyncButtonLeds() a
// few times a second so the pending-change preview (on screen, and on button
// A's LED) can flash (800ms on / 200ms off)
lv_timer_t *screenFlashTimer = nullptr;

// the main button grid - one flat cell and its centred label per physical
// button id, indexed by that id (so screenMainBtnCells[7] is button 7's
// cell) - including the three navigation ids, whose slots are simply never
// filled in by createScreenWidgets() (see screenWidgetsReady below for how
// the sync functions know to leave them alone). Sized MAX_BUTTONS (the most buttons a unit can have; only the first
// device.buttonCount slots are used) rather
// than a fixed 10: performance ids aren't guaranteed to be a contiguous
// 0..N-1 run once the navigation ids can sit anywhere in the numbering, so
// indexing this array directly by id (with a few always-null slots at the
// nav ids) is simpler and safer than trying to pack performance ids
// together. Built once by createScreenWidgets(); their colour, opacity,
// label text and label colour are refreshed every display() by
// screenSyncMainButtons() from the resolved Button for each id.
lv_obj_t *screenMainBtnCells[MAX_BUTTONS] = { nullptr };
lv_obj_t *screenMainBtnLabels[MAX_BUTTONS] = { nullptr };
// set true at the end of createScreenWidgets() - screenSyncMainButtons()
// uses this (rather than checking whether some particular cell pointer is
// non-null) to know the widget tree exists yet, since which slot(s) of
// screenMainBtnCells are non-null now depends on where the navigation ids
// fall, not a fixed index like 0.
bool screenWidgetsReady = false;

// converts one of this project's ColourCode objects (see ColourCode.h) into
// the "lv_color_t" type every LVGL styling call expects. lv_color_make()
// packs three 0-255 components much like ColourCode::getValue() does, just
// handing back LVGL's own colour type rather than a raw uint16_t.
static lv_color_t screenColour(const ColourCode &colour) {
  return lv_color_make(colour.red, colour.green, colour.blue);
}

// approximates the colour you actually see when `fg` is painted at `alpha`
// (0.0-1.0) opacity over `background`. An "off" main button is drawn at 20%
// opacity over the near-black bottom of the screen gradient, so its
// configured colour on its own is a poor basis for choosing a readable label
// colour - this blend is closer to what's on screen. Used only by
// screenSyncMainButtons().
static ColourCode screenBlendOver(const ColourCode &fg, const ColourCode &background, double alpha) {
  return ColourCode(
    (int)lround(fg.red   * alpha + background.red   * (1.0 - alpha)),
    (int)lround(fg.green * alpha + background.green * (1.0 - alpha)),
    (int)lround(fg.blue  * alpha + background.blue  * (1.0 - alpha)));
}

// strips a trailing ".json" (matched case-sensitively, as ConfigStore.h
// writes it) off a config filename for display - "Live Rig.json" becomes
// "Live Rig". Any other name is returned unchanged.
static String screenConfigDisplayName(const String &filename) {
  if (filename.endsWith(".json")) {
    return filename.substring(0, filename.length() - 5);
  }
  return filename;
}

// strips the border, corner radius and inner padding LVGL puts on a plain
// object by default, so it can serve as a flat, full-width colour bar. The
// trailing "0" on each "lv_obj_set_style_..." call is LVGL's state selector
// (0 = the default/normal state) - these bars never use the other states
// (pressed, focused...), so it's 0 everywhere here.
static void styleAsBar(lv_obj_t *bar) {
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
}

// paints one bar's background to `background`, then sets its two labels'
// text to pure black or pure white - whichever contrasts more with that
// background (see contrastingTextColour() in ColourCode.h): a light bar
// gets black text, a dark bar gets white text. Both the Header and the
// SubHeader need exactly this, once at build time and again in display()
// each time the config or the selected bank changes the bar's colour.
static void setBarColours(lv_obj_t *bar, lv_obj_t *leftLabel, lv_obj_t *rightLabel,
                          const ColourCode &background) {
  lv_obj_set_style_bg_color(bar, screenColour(background), 0);
  lv_color_t textColour = screenColour(contrastingTextColour(background));
  lv_obj_set_style_text_color(leftLabel, textColour, 0);
  lv_obj_set_style_text_color(rightLabel, textColour, 0);
}

// makes a plain base object behave and look like a flat button: clickable
// (base lv_obj isn't, unlike a real lv_button - which this build's lv_conf.h
// leaves disabled, along with every widget except lv_label), no rounded
// corners / border / padding, flat [39,39,39] with white text in every
// state (the parameter buttons never look "pressed" - the flashing readouts
// are the feedback instead)
static void styleAsParamButton(lv_obj_t *btn) {
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, lv_color_make(39, 39, 39), 0);
  lv_obj_set_style_bg_color(btn, lv_color_make(39, 39, 39), LV_STATE_PRESSED);
  lv_obj_set_style_text_color(btn, lv_color_make(255, 255, 255), 0);
}

// strips a main-grid cell down to a flat rectangle - no rounded corners,
// border or padding - and makes the plain lv_obj clickable (see
// styleAsParamButton above for why that flag is needed on this build). Its
// background colour, opacity and clickability are then set per-state every
// display() by screenSyncMainButtons(); the cell shows no "pressed" look of
// its own (the button turning on is the feedback).
static void styleAsMainButton(lv_obj_t *cell) {
  lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(cell, 0, 0);
  lv_obj_set_style_border_width(cell, 0, 0);
  lv_obj_set_style_pad_all(cell, 0, 0);
}

// press / release of an on-screen main button (a grid cell, for a
// non-navigation id). The physical id is passed as the event's user data
// (see createScreenWidgets()).
// These mirror the footswitch path in MIDI-Toolbox.ino's loop() exactly:
// performanceButtonPress() on press (which commits any pending navigation for
// a pure-performance button, then runs the button's actions), releaseButton()
// on release (which only does anything for a Momentary button). display() is
// called after each so the grid's on/off appearance updates at once.
static void screenMainBtnPressedCb(lv_event_t *e) {
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  performanceButtonPress(id, "on-screen");
  display();
}
static void screenMainBtnReleasedCb(lv_event_t *e) {
  int id = (int)(intptr_t)lv_event_get_user_data(e);
  logButton(id, "released", "on-screen");
  performanceButtonRelease(id);
  display();
}

// refreshes every main-grid cell from the Button currently resolved for its
// id (Patch > Bank > Mode > Config override order - see resolveButton() in
// Runtime.h):
//   - no Button for that id     -> transparent, label cleared, not clickable
//     (the cell stays in place so the grid never reflows)
//   - Button with a colourCode  -> filled with that colour; full opacity when
//     isOn, 20% when off (the 20%-over-near-black blend while off, the solid
//     colour while on).
//   - Button with no colourCode -> filled with the config's headerColourCode
//     (or a dim [30,30,30] when the config sets none) while isOn; fully
//     transparent, not just dimmed, while off - the button shows no colour at
//     all until it's next turned on.
// The label is the Button's name, drawn black or white for the best contrast
// against the colour as actually shown.
// Called from display() only - the grid doesn't flash, so the flash timer
// leaves it alone.
static void screenSyncMainButtons() {
  if (!screenWidgetsReady) {
    return; // createScreenWidgets() hasn't run yet
  }
  if (config.modes.empty()) {
    return; // no config loaded yet - resolveButton() needs a current Mode
  }

  ColourCode headerColour = config.hasHeaderColourCode ? config.headerColourCode
                                                        : ColourCode(30, 30, 30);

  for (int id = 0; id < device.buttonCount; id++) {
    // the navigation ids have no grid cell (see screenMainBtnCells above) -
    // their on-screen readout is the parameter row, not the button grid
    if (id == device.navSelectId || id == device.navDownId || id == device.navUpId) {
      continue;
    }
    lv_obj_t *cell = screenMainBtnCells[id];
    lv_obj_t *label = screenMainBtnLabels[id];
    if (cell == nullptr) {
      continue; // an id with no cell in device.gridButtonIds at all
    }
    Button *button = resolveButton(id);

    if (button == nullptr) {
      lv_label_set_text(label, "");
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
      lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
      continue;
    }
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);

    if (!button->hasColourCode) {
      lv_label_set_text(label, button->name.c_str());
      if (button->isOn) {
        lv_obj_set_style_bg_color(cell, screenColour(headerColour), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(label, screenColour(contrastingTextColour(headerColour)), 0);
      } else {
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
        // near-black background behind the transparent cell - matches the
        // screen gradient's bottom, same contrast target as the off blend
        lv_obj_set_style_text_color(label, screenColour(contrastingTextColour(ColourCode(0, 0, 0))), 0);
      }
      continue;
    }

    ColourCode base = button->colourCode;
    lv_obj_set_style_bg_color(cell, screenColour(base), 0);

    ColourCode shown;
    if (button->isOn) {
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      shown = base;
    } else {
      lv_obj_set_style_bg_opa(cell, SCREEN_BTNGRID_OFF_OPA, 0);
      // the near-black bottom of the [85,85,85]->[0,0,0] screen gradient
      shown = screenBlendOver(base, ColourCode(0, 0, 0), SCREEN_BTNGRID_OFF_ALPHA);
    }
    lv_obj_set_style_text_color(label, screenColour(contrastingTextColour(shown)), 0);
    lv_label_set_text(label, button->name.c_str());
  }
}

// ----------------------------------------------------------------------------
// RGB button LEDs
//
// Every footswitch has an RGB LED behind it, all of them wired through the
// ShiftPWM shift-register chain (see the "Shift register" section of the .ino,
// and docs/hardware-build.md for the wiring). ShiftPWM.SetRGB(ledIndex, r, g, b) takes three 0-255
// channel values and drives one LED. The LED indices line up 1:1 with this
// project's physical button ids, so LED N sits behind button id N - every id
// from 0 to device.buttonCount - 1, navigation ids included.
//
// Ordinary (non-navigation) LEDs are lit in the same colour their on-screen
// cell is drawn in. A Button with its own colourCode is shown at one of two
// brightnesses: full while the button is on, and SCREEN_LED_OFF_BRIGHTNESS
// (2% duty) while it's off. A Button with no colourCode instead shows the
// config's header-bar colour (or a dim [30,30,30] when the config sets none)
// at full brightness while on, and is dark - not dimmed - while off. A grid
// position with no Button resolved for it also goes dark.
//
// The three navigation footswitches (device.navSelectId/device.navDownId/device.navUpId)
// are the built-in navigation controls and have no configurable colour or
// on/off state, so their LEDs are dark by default and only light while the
// switch is actually held down. device.navSelectId also lights whenever a
// hierarchy / MIDI-channel change is pending (navigationDirty() - the same
// condition that arms "Set?" on the middle button), flashing 800ms on /
// 200ms off to match the on-screen preview. When lit, a navigation LED shows
// the config's header-bar colour, or a dim [30,30,30] when the config sets
// no header colour.
//
// screenSyncButtonLeds() is called from display() straight after
// screenSyncMainButtons() - from the same resolved Button for each id, so the
// LEDs and the screen can never disagree - and again from screenFlashTimerCb
// (every 50ms) so the device.navSelectId flash keeps ticking and a held
// device.navDownId/device.navUpId lights promptly. That 20Hz call rate is why
// screenLedWrite() below de-dupes: see its comment.

// scales one 0-255 colour channel to `fraction` (0.0-1.0) of its value,
// rounded and clamped back into 0-255
static uint8_t screenLedScaleChannel(int channel, double fraction) {
  long scaled = lround(channel * fraction);
  if (scaled < 0) scaled = 0;
  if (scaled > 255) scaled = 255;
  return (uint8_t)scaled;
}

// last RGB pushed to each LED - one per physical button id, navigation ids
// included, matching the LED wiring order (LED index == button id) - so
// screenLedWrite() can call ShiftPWM.SetRGB() only when the value has
// actually changed. This sync runs ~20 times a second off screenFlashTimer;
// ShiftPWM.SetRGB() writes its three channel bytes into the buffer that the
// ShiftPWM interrupt is continuously clocking out to the shift registers,
// and a rewrite landing mid-scan - even with the same values - can glitch
// the output and show as LED flicker. Re-sending only real changes keeps
// the running PWM undisturbed. Zero-initialised, but screenLedEverWritten
// (below) keeps that from being mistaken for "hardware already holds zero"
// - see there for why.
static int screenLedLastRGB[MAX_BUTTONS][3] = {};

// whether screenLedWrite() has ever actually pushed a value for each LED.
// The very first display() of a boot can already need an LED lit - e.g. a
// Bank-level defaultStatus:On button seeded via ModeBank.defaultPatchIndex
// (see Navigation.h/ConfigStore.h) - and that first call races the hardware
// coming up (ShiftPWM.Start() / the boot rainbow's ShiftPWM.SetAll(0) in the
// .ino). If that particular write doesn't actually land on the shift
// registers, screenLedLastRGB would still record it as sent, and every
// identical resync after that (screenFlashTimer runs this ~20 times a
// second) would then skip re-sending it forever - the LED stays dark even
// though the on-screen grid (screenSyncMainButtons(), which has no such
// cache and redraws unconditionally) correctly shows the button lit. Forcing
// the first write for each LED through unconditionally closes that gap,
// without weakening the de-dupe for every write after it.
static bool screenLedEverWritten[MAX_BUTTONS] = {};

// set true by setup() once ShiftPWM.Start() has run. screenFlashTimer (created
// in createScreenWidgets(), during initScreen()) starts ticking well before the
// shift-register driver is up - the Serial wait and config load in setup()
// between the two run for seconds, and every lv_timer_handler() call in that
// window fires screenFlashTimerCb -> screenSyncButtonLeds(). ShiftPWM.SetRGB()
// rejects those early writes ("number of outputs is 0") and, worse, letting
// them populate the de-dup cache below would make the first real display()
// skip re-sending an LED that never actually landed. Gating on this flag keeps
// screenLedWrite() a clean no-op until the hardware exists.
bool screenLedsReady = false;

// pushes `r`,`g`,`b` (0-255) to one LED, skipping the hardware write only
// when that LED has already been written before AND already holds that
// value (see screenLedLastRGB / screenLedEverWritten)
static void screenLedWrite(int ledIndex, int r, int g, int b) {
  if (!screenLedsReady) {
    return; // shift-register driver not started yet - see screenLedsReady
  }
  const int cacheLen = (int)(sizeof(screenLedLastRGB) / sizeof(screenLedLastRGB[0]));
  if (ledIndex >= 0 && ledIndex < cacheLen) {
    if (screenLedEverWritten[ledIndex] &&
        screenLedLastRGB[ledIndex][0] == r &&
        screenLedLastRGB[ledIndex][1] == g &&
        screenLedLastRGB[ledIndex][2] == b) {
      return; // unchanged - leave the running PWM alone
    }
    screenLedLastRGB[ledIndex][0] = r;
    screenLedLastRGB[ledIndex][1] = g;
    screenLedLastRGB[ledIndex][2] = b;
    screenLedEverWritten[ledIndex] = true;
  }
  ShiftPWM.SetRGB(ledIndex, r, g, b);
}

// drives LED `ledIndex` to `colour`, every channel scaled to `fraction`
// brightness
static void screenLedSet(int ledIndex, const ColourCode &colour, double fraction) {
  screenLedWrite(ledIndex,
                 screenLedScaleChannel(colour.red,   fraction),
                 screenLedScaleChannel(colour.green, fraction),
                 screenLedScaleChannel(colour.blue,  fraction));
}

// turns one LED fully off
static void screenLedOff(int ledIndex) {
  screenLedWrite(ledIndex, 0, 0, 0);
}

static void screenSyncButtonLeds() {
  if (config.modes.empty()) {
    return; // no config loaded yet - resolveButton() needs a current Mode
  }

  ColourCode navColour = config.hasHeaderColourCode ? config.headerColourCode
                                                    : ColourCode(30, 30, 30);

  // main grid - every non-navigation id: a Button with its own colourCode is
  // shown at full brightness when on and 2% duty when off; a Button with no
  // colourCode shows navColour (the config header colour) at full brightness
  // when on, and is dark - not dimmed - when off. Dark when the grid
  // position is empty.
  for (int id = 0; id < device.buttonCount; id++) {
    if (id == device.navSelectId || id == device.navDownId || id == device.navUpId) {
      continue; // handled below - a different, held/flash-driven behaviour
    }
    Button *button = resolveButton(id);
    if (button == nullptr) {
      screenLedOff(id);
      continue;
    }
    if (!button->hasColourCode) {
      if (button->isOn) {
        screenLedSet(id, navColour, 1.0);
      } else {
        screenLedOff(id);
      }
      continue;
    }
    screenLedSet(id, button->colourCode, button->isOn ? 1.0 : SCREEN_LED_OFF_BRIGHTNESS);
  }

  // navigation footswitches - dark unless held; Bounce::read() is LOW while
  // a switch is pressed (INPUT_PULLUP - see Pins.h). device.navSelectId also
  // lights while a change is pending, flashing 800ms on / 200ms off.
  bool selectPending = navigationDirty();
  bool flashOn = (millis() % 1000) < 800;

  bool selectLit = (!footswitches[device.navSelectId].read() || selectPending) && (!selectPending || flashOn);
  if (selectLit) {
    screenLedSet(device.navSelectId, navColour, 1.0);
  } else {
    screenLedOff(device.navSelectId);
  }

  if (!footswitches[device.navDownId].read()) {
    screenLedSet(device.navDownId, navColour, 1.0);
  } else {
    screenLedOff(device.navDownId);
  }

  if (!footswitches[device.navUpId].read()) {
    screenLedSet(device.navUpId, navColour, 1.0);
  } else {
    screenLedOff(device.navUpId);
  }
}

// ----------------------------------------------------------------------------
// Live layout (ModeBank "screenLayout": "Live") and the button-name overlay
//
// A large-type performance view for reading the screen from a distance. Instead
// of the SubHeader, the button grid and the parameter row at the top, it shows:
//
//   +----------------------------------------------------+
//   | Header    <config name>              <Mode name>   |  <- unchanged
//   +----------------------------------------------------+
//   | BANK                                               |  <- big, bank colour
//   | <Bank name>                                        |
//   +-------------------------------------------+--------+
//   | PATCH                                     | PAGE   |  <- biggest; the page
//   | <Patch name>                              | 2/3    |     box only appears
//   +-------------------------------------------+--------+     when the patch
//   [<] [ Patch ] [>]                                Ch. 1       has >1 page
//
// The navigation buttons and MIDI-channel readout are the ordinary parameter
// row moved to the bottom (screenLayoutParamRow()), so they keep working
// exactly as before - including the guaranteed on-screen navigation route. The
// Bank / Patch / Page box the arrows currently steer gets a white border.
//
// The Montserrat sizes used here must be enabled in lv_conf.h
// (LV_FONT_MONTSERRAT_48 / 36 / 24 are the ones aimed for). Each tier falls
// back to the nearest enabled size below it, and finally to the default font,
// so the screen still builds - just less large - if they aren't enabled.
//
// The overlay ("toast") is the pressed button's name in a panel over the middle
// of the screen, in either layout. When it appears and disappears is decided in
// Navigation.h (the toast* functions); this just draws it.

#define SCREEN_LIVE_GAP        4
#define SCREEN_LIVE_NAV_H      44
#define SCREEN_LIVE_ARROW_W    56
#define SCREEN_LIVE_NAME_W     104
#define SCREEN_LIVE_PAGE_W     96
#define SCREEN_LIVE_NAV_Y      (SCREEN_LY - SCREEN_LIVE_NAV_H - SCREEN_LIVE_GAP)
#define SCREEN_LIVE_BODY_Y     (SCREEN_HEADER_H + SCREEN_LIVE_GAP)
#define SCREEN_LIVE_BODY_H     (SCREEN_LIVE_NAV_Y - SCREEN_LIVE_GAP - SCREEN_LIVE_BODY_Y)
#define SCREEN_LIVE_BANK_H     ((SCREEN_LIVE_BODY_H * 2) / 5)
#define SCREEN_LIVE_PATCH_Y    (SCREEN_LIVE_BODY_Y + SCREEN_LIVE_BANK_H + SCREEN_LIVE_GAP)
#define SCREEN_LIVE_PATCH_H    (SCREEN_LIVE_BODY_Y + SCREEN_LIVE_BODY_H - SCREEN_LIVE_PATCH_Y)
#define SCREEN_LIVE_BORDER     3
// the patch box is drawn this fraction as bright as the bank colour behind it
#define SCREEN_LIVE_PATCH_DIM  0.65

#define SCREEN_TOAST_H         72
#define SCREEN_TOAST_INSET     20
#define SCREEN_TOAST_PAD       8

// three font tiers, largest first. Each picks the biggest enabled Montserrat
// size at or below its target.
#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define SCREEN_FONT_BIG (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define SCREEN_FONT_BIG (&lv_font_montserrat_40)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  #define SCREEN_FONT_BIG (&lv_font_montserrat_32)
#else
  #define SCREEN_FONT_BIG LV_FONT_DEFAULT
#endif
#if defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
  #define SCREEN_FONT_MID (&lv_font_montserrat_36)
#elif defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
  #define SCREEN_FONT_MID (&lv_font_montserrat_32)
#elif defined(LV_FONT_MONTSERRAT_28) && LV_FONT_MONTSERRAT_28
  #define SCREEN_FONT_MID (&lv_font_montserrat_28)
#else
  #define SCREEN_FONT_MID LV_FONT_DEFAULT
#endif
#if defined(LV_FONT_MONTSERRAT_24) && LV_FONT_MONTSERRAT_24
  #define SCREEN_FONT_SMALL (&lv_font_montserrat_24)
#elif defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
  #define SCREEN_FONT_SMALL (&lv_font_montserrat_20)
#else
  #define SCREEN_FONT_SMALL LV_FONT_DEFAULT
#endif

// true while the Live layout is the one on screen
bool screenLiveActive = false;

lv_obj_t *screenLiveBankBox = nullptr;
lv_obj_t *screenLiveBankCaption = nullptr;
lv_obj_t *screenLiveBankLabel = nullptr;
lv_obj_t *screenLivePatchBox = nullptr;
lv_obj_t *screenLivePatchCaption = nullptr;
lv_obj_t *screenLivePatchLabel = nullptr;
lv_obj_t *screenLivePageBox = nullptr;
lv_obj_t *screenLivePageCaption = nullptr;
lv_obj_t *screenLivePageLabel = nullptr;

lv_obj_t *screenToastBox = nullptr;
lv_obj_t *screenToastLabel = nullptr;

// puts `text` in `label` (which must be a fixed-width, LV_LABEL_LONG_MODE_DOTS
// label of width maxW) in the largest of the three fonts that fits on one line;
// if even the smallest doesn't fit, the smallest is used and the label's own
// "..." truncation takes over.
static void screenFitLabel(lv_obj_t *label, const String &text, int maxW,
                           const lv_font_t *big, const lv_font_t *mid, const lv_font_t *small) {
  const lv_font_t *fonts[3] = { big, mid, small };
  const lv_font_t *chosen = small;
  for (int i = 0; i < 3; i++) {
    lv_point_t size;
    lv_text_get_size(&size, text.c_str(), fonts[i], 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x <= maxW) {
      chosen = fonts[i];
      break;
    }
  }
  lv_obj_set_style_text_font(label, chosen, 0);
  lv_label_set_text(label, text.c_str());
}

// a flat colour panel for the Live layout, hidden until the layout is applied
static lv_obj_t * screenCreateLiveBox(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_size(box, w, h);
  lv_obj_set_pos(box, x, y);
  styleAsBar(box);
  lv_obj_set_style_border_width(box, SCREEN_LIVE_BORDER, 0);
  lv_obj_set_style_border_color(box, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_border_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
  return box;
}

// the small "BANK" / "PATCH" / "PAGE" caption in a box's top-left corner
static lv_obj_t * screenCreateLiveCaption(lv_obj_t *box, const char *text) {
  lv_obj_t *caption = lv_label_create(box);
  lv_label_set_text(caption, text);
  lv_obj_set_style_text_opa(caption, LV_OPA_70, 0);
  lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 4, 0);
  return caption;
}

// the big value label in a box, fixed width so its text truncates with "..."
static lv_obj_t * screenCreateLiveValue(lv_obj_t *box, int width) {
  lv_obj_t *label = lv_label_create(box);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(label, width);
  lv_label_set_text(label, "");
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 8);
  return label;
}

// builds every Live-layout widget and the overlay, once, from
// createScreenWidgets(). Everything starts hidden.
static void screenCreateLiveWidgets(lv_obj_t *screen) {
  const int bankInner = SCREEN_LX - 8 - SCREEN_LIVE_BORDER * 2;

  screenLiveBankBox = screenCreateLiveBox(screen, 0, SCREEN_LIVE_BODY_Y, SCREEN_LX, SCREEN_LIVE_BANK_H);
  screenLiveBankCaption = screenCreateLiveCaption(screenLiveBankBox, "BANK");
  screenLiveBankLabel = screenCreateLiveValue(screenLiveBankBox, bankInner);

  screenLivePatchBox = screenCreateLiveBox(screen, 0, SCREEN_LIVE_PATCH_Y, SCREEN_LX, SCREEN_LIVE_PATCH_H);
  screenLivePatchCaption = screenCreateLiveCaption(screenLivePatchBox, "PATCH");
  screenLivePatchLabel = screenCreateLiveValue(screenLivePatchBox, bankInner);

  screenLivePageBox = screenCreateLiveBox(screen, SCREEN_LX - SCREEN_LIVE_PAGE_W, SCREEN_LIVE_PATCH_Y,
                                          SCREEN_LIVE_PAGE_W, SCREEN_LIVE_PATCH_H);
  screenLivePageCaption = screenCreateLiveCaption(screenLivePageBox, "PAGE");
  screenLivePageLabel = screenCreateLiveValue(screenLivePageBox,
                                              SCREEN_LIVE_PAGE_W - 8 - SCREEN_LIVE_BORDER * 2);

  // the overlay: centred over the area between the Header and the bottom row,
  // created last so it sits on top, and never takes touches
  const int toastW = SCREEN_LX - SCREEN_TOAST_INSET * 2;
  const int toastY = SCREEN_LIVE_BODY_Y + (SCREEN_LIVE_BODY_H - SCREEN_TOAST_H) / 2;
  screenToastBox = lv_obj_create(screen);
  lv_obj_set_size(screenToastBox, toastW, SCREEN_TOAST_H);
  lv_obj_set_pos(screenToastBox, SCREEN_TOAST_INSET, toastY);
  styleAsBar(screenToastBox);
  lv_obj_remove_flag(screenToastBox, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(screenToastBox, LV_OBJ_FLAG_HIDDEN);
  screenToastLabel = lv_label_create(screenToastBox);
  lv_label_set_long_mode(screenToastLabel, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(screenToastLabel, toastW - SCREEN_TOAST_PAD * 2);
  lv_obj_set_style_text_align(screenToastLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(screenToastLabel);
}

// sizes and places the parameter row for the Live layout (at the bottom, with
// bigger touch targets and text) or the Grid layout (under the SubHeader)
static void screenLayoutParamRow(bool live) {
  const int rowH = live ? SCREEN_LIVE_NAV_H : SCREEN_PARAMROW_H;
  const int rowY = live ? SCREEN_LIVE_NAV_Y : SCREEN_PARAMROW_Y;
  const int arrowW = live ? SCREEN_LIVE_ARROW_W : SCREEN_PARAMBTN_ARROW_W;
  const int nameW = live ? SCREEN_LIVE_NAME_W : SCREEN_PARAMBTN_NAME_W;
  const int groupX = live ? SCREEN_LIVE_GAP : (SCREEN_LX - (arrowW * 2 + nameW)) / 2;
  const lv_font_t *font = live ? SCREEN_FONT_SMALL : LV_FONT_DEFAULT;

  lv_obj_set_size(screenParamRow, SCREEN_LX, rowH);
  lv_obj_align(screenParamRow, LV_ALIGN_TOP_LEFT, 0, rowY);

  lv_obj_set_size(screenParamDownBtn, arrowW, rowH);
  lv_obj_align(screenParamDownBtn, LV_ALIGN_LEFT_MID, groupX, 0);
  lv_obj_set_size(screenParamNameBtn, nameW, rowH);
  lv_obj_align(screenParamNameBtn, LV_ALIGN_LEFT_MID, groupX + arrowW, 0);
  lv_obj_set_size(screenParamUpBtn, arrowW, rowH);
  lv_obj_align(screenParamUpBtn, LV_ALIGN_LEFT_MID, groupX + arrowW + nameW, 0);

  lv_obj_set_style_text_font(screenParamDownLabel, font, 0);
  lv_obj_set_style_text_font(screenParamNameLabel, font, 0);
  lv_obj_set_style_text_font(screenParamUpLabel, font, 0);
  lv_obj_set_style_text_font(screenChannelLabel, font, 0);
}

// switches between the Grid and Live layouts. Cheap when nothing changes, so
// display() can call it every time; only re-lays-out on an actual change.
static void screenApplyLayout(bool live) {
  if (!screenWidgetsReady || live == screenLiveActive) {
    return;
  }
  screenLiveActive = live;

  // the SubHeader and the button grid belong to the Grid layout
  if (live) {
    lv_obj_add_flag(screenSubHeader, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(screenSubHeader, LV_OBJ_FLAG_HIDDEN);
  }
  for (int id = 0; id < device.buttonCount; id++) {
    if (screenMainBtnCells[id] == nullptr) {
      continue;
    }
    if (live) {
      lv_obj_add_flag(screenMainBtnCells[id], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(screenMainBtnCells[id], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // ...and the big readouts to the Live layout
  lv_obj_t *liveBoxes[3] = { screenLiveBankBox, screenLivePatchBox, screenLivePageBox };
  for (lv_obj_t *box : liveBoxes) {
    if (live) {
      lv_obj_remove_flag(box, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    }
  }

  screenLayoutParamRow(live);
}

// sets one Live box's colour, its text colour to contrast with it, and its
// border to show whether the arrows currently steer it
static void screenStyleLiveBox(lv_obj_t *box, lv_obj_t *caption, lv_obj_t *label,
                               const ColourCode &background, bool steered, lv_opa_t textOpa) {
  lv_obj_set_style_bg_color(box, screenColour(background), 0);
  lv_obj_set_style_border_opa(box, steered ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_color_t text = screenColour(contrastingTextColour(background));
  lv_obj_set_style_text_color(caption, text, 0);
  lv_obj_set_style_text_color(label, text, 0);
  lv_obj_set_style_text_opa(label, textOpa, 0);
}

// refreshes the Live layout's three boxes from the previewed Bank / Patch / Page,
// flashing (via flashOpa) whichever the pending navigation is changing. Called
// from screenSyncParamEdit().
static void screenSyncLive(lv_opa_t flashOpa) {
  if (!screenLiveActive) {
    return;
  }
  ModeBank *liveBank = getCurrentBank();
  ColourCode bankColour = (liveBank != nullptr) ? liveBank->subheaderColourCode : ColourCode(44, 44, 44);
  ColourCode patchColour = screenBlendOver(bankColour, ColourCode(0, 0, 0), SCREEN_LIVE_PATCH_DIM);
  Parameter steered = navSelectedParameter();

  // the page box appears only when the previewed patch has more than one page;
  // a pending different patch always opens on page 1 once activated
  Patch *previewPatch = navigationPreviewPatch();
  int pageCount = (previewPatch != nullptr) ? patchPageCount(previewPatch) : 1;
  bool showPage = pageCount > 1;
  int page = navigationPatchDirty() ? 1 : currentPage;

  const int inner = SCREEN_LX - 8 - SCREEN_LIVE_BORDER * 2;
  const int patchW = showPage ? SCREEN_LX - SCREEN_LIVE_PAGE_W - SCREEN_LIVE_GAP : SCREEN_LX;

  lv_obj_set_width(screenLivePatchBox, patchW);
  lv_obj_set_width(screenLivePatchLabel, patchW - 8 - SCREEN_LIVE_BORDER * 2);
  if (showPage) {
    lv_obj_remove_flag(screenLivePageBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(screenLivePageBox, LV_OBJ_FLAG_HIDDEN);
  }

  screenStyleLiveBox(screenLiveBankBox, screenLiveBankCaption, screenLiveBankLabel, bankColour,
                     steered == Parameter::BankParameter,
                     navigationBankDirty() ? flashOpa : LV_OPA_COVER);
  screenFitLabel(screenLiveBankLabel, navigationPreviewBankName(), inner,
                 SCREEN_FONT_MID, SCREEN_FONT_SMALL, SCREEN_FONT_SMALL);

  screenStyleLiveBox(screenLivePatchBox, screenLivePatchCaption, screenLivePatchLabel, patchColour,
                     steered == Parameter::PatchParameter,
                     navigationPatchDirty() ? flashOpa : LV_OPA_COVER);
  screenFitLabel(screenLivePatchLabel, navigationPreviewPatchBaseName(), patchW - 8 - SCREEN_LIVE_BORDER * 2,
                 SCREEN_FONT_BIG, SCREEN_FONT_MID, SCREEN_FONT_SMALL);

  if (showPage) {
    screenStyleLiveBox(screenLivePageBox, screenLivePageCaption, screenLivePageLabel, patchColour,
                       steered == Parameter::PageParameter,
                       navigationPatchDirty() ? flashOpa : LV_OPA_COVER);
    screenFitLabel(screenLivePageLabel, String(page) + "/" + String(pageCount),
                   SCREEN_LIVE_PAGE_W - 8 - SCREEN_LIVE_BORDER * 2,
                   SCREEN_FONT_BIG, SCREEN_FONT_MID, SCREEN_FONT_SMALL);
  }
}

// shows, updates or hides the button-name overlay. Hidden whenever a navigation
// is pending (the flashing preview takes priority) or its time is up. Called
// from display() and every flash-timer tick, so it appears and expires on time.
static void screenSyncToast() {
  if (screenToastBox == nullptr) {
    return;
  }
  bool show = toastVisible(millis()) && !navigationDirty();
  if (!show) {
    lv_obj_add_flag(screenToastBox, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_set_style_bg_color(screenToastBox, screenColour(toastColour), 0);
  lv_obj_set_style_text_color(screenToastLabel, screenColour(contrastingTextColour(toastColour)), 0);
  screenFitLabel(screenToastLabel, toastText, SCREEN_LX - SCREEN_TOAST_INSET * 2 - SCREEN_TOAST_PAD * 2,
                 SCREEN_FONT_BIG, SCREEN_FONT_MID, SCREEN_FONT_SMALL);
  lv_obj_center(screenToastLabel);
  lv_obj_remove_flag(screenToastBox, LV_OBJ_FLAG_HIDDEN);
}

// re-applies the pending-navigation preview to the screen (see Navigation.h):
// the middle button shows the selected parameter's name, or "Set?" while a
// change is pending; each Header/SubHeader label whose level differs from the
// live selection shows the pending value and flashes on an 800ms-on /
// 200ms-off cycle; every other readout is left solid, showing whatever
// display() last wrote.
//
// Called at the end of display() (so a state change shows at once) and
// repeatedly from screenFlashTimer (so the flash keeps ticking).
static void screenSyncParamEdit() {
  if (screenParamNameLabel == nullptr) {
    return; // createScreenWidgets() hasn't run yet
  }
  if (config.modes.empty()) {
    return; // no config loaded yet - the preview getters need a current Mode
  }

  // middle button: the selected parameter name, or "Set?" when a change is
  // pending - always solid
  lv_label_set_text(screenParamNameLabel, navigationButton10Label().c_str());
  lv_obj_set_style_text_opa(screenParamNameLabel, LV_OPA_COVER, 0);

  // 800ms visible, then 200ms hidden, out of every 1000ms
  const lv_opa_t flashOpa = ((millis() % 1000) < 800) ? LV_OPA_COVER : LV_OPA_TRANSP;

  // Each Header/SubHeader label shows its level's value and flashes while
  // that level differs from the live selection; otherwise it shows the live
  // value, solid. navigationPreview*Name() returns the LIVE value when its
  // level isn't dirty, so the text is written unconditionally - only the
  // opacity depends on dirty. That matters because a value step doesn't call
  // display(): the flash timer alone has to put a label back to the live
  // text once a round-trip clears the pending change.

  // config filename (Header, left)
  lv_label_set_text(screenConfigLabel,
                    screenConfigDisplayName(navigationPreviewConfigName()).c_str());
  lv_obj_set_style_text_opa(screenConfigLabel,
                            navigationConfigDirty() ? flashOpa : LV_OPA_COVER, 0);

  // Mode name (Header, right)
  lv_label_set_text(screenModeLabel, navigationPreviewModeName().c_str());
  lv_obj_set_style_text_opa(screenModeLabel,
                            navigationModeDirty() ? flashOpa : LV_OPA_COVER, 0);

  // Bank name (SubHeader, left)
  lv_label_set_text(screenBankLabel, navigationPreviewBankName().c_str());
  lv_obj_set_style_text_opa(screenBankLabel,
                            navigationBankDirty() ? flashOpa : LV_OPA_COVER, 0);

  // Patch name (SubHeader, right)
  lv_label_set_text(screenPatchLabel, navigationPreviewPatchName().c_str());
  lv_obj_set_style_text_opa(screenPatchLabel,
                            navigationPatchDirty() ? flashOpa : LV_OPA_COVER, 0);

  // MIDI channel readout (parameter row, right)
  lv_label_set_text(screenChannelLabel,
                    (String("Ch. ") + navigationPreviewChannel()).c_str());
  lv_obj_set_style_text_opa(screenChannelLabel,
                            navigationChannelDirty() ? flashOpa : LV_OPA_COVER, 0);

  // the right-aligned labels have just had their text (and so width)
  // changed, so re-run their alignment - see the note in display()
  lv_obj_align(screenModeLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);
  lv_obj_align(screenPatchLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);
  lv_obj_align(screenChannelLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);

  // the Live layout's big Bank / Patch / Page readouts (a no-op in the Grid
  // layout) and the button-name overlay (either layout)
  screenSyncLive(flashOpa);
  screenSyncToast();
}

// greys the left/right step arrows out when the selected parameter has 0 or 1
// possible values (e.g. "Bank" selected but the Mode has a single bank) -
// nothing to step through, so they should look disabled. Called from
// display() only, not the flash timer: the count can involve an SD scan for
// the Config parameter, and it only changes on a state change anyway.
static void screenUpdateParamArrows() {
  if (screenParamDownBtn == nullptr) {
    return;
  }
  bool enabled = navigationValueCount() > 1;

  lv_color_t textColour = enabled ? lv_color_make(255, 255, 255) : lv_color_make(90, 90, 90);
  lv_color_t bgColour   = enabled ? lv_color_make(39, 39, 39)    : lv_color_make(28, 28, 28);

  lv_obj_set_style_text_color(screenParamDownLabel, textColour, 0);
  lv_obj_set_style_text_color(screenParamUpLabel, textColour, 0);
  lv_obj_set_style_bg_color(screenParamDownBtn, bgColour, 0);
  lv_obj_set_style_bg_color(screenParamUpBtn, bgColour, 0);

  // also stop taps landing on a disabled arrow (the physical footswitches
  // are already inert for this case - navStepValue() -> pendingCycle* no-op
  // when the level has <= 1 value)
  if (enabled) {
    lv_obj_add_flag(screenParamDownBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(screenParamUpBtn, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_remove_flag(screenParamDownBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(screenParamUpBtn, LV_OBJ_FLAG_CLICKABLE);
  }
}

// screenFlashTimer's callback - re-runs the preview sync (so the on-screen
// flash advances even when nothing else has redrawn the screen) and the LED
// sync (so button A's pending-change flash keeps ticking, and a held B / C
// lights without waiting for the next display()). screenLedWrite()'s de-dupe
// means the LED sync is cheap when nothing has actually changed.
static void screenFlashTimerCb(lv_timer_t *timer) {
  (void)timer;
  screenSyncParamEdit();
  screenSyncButtonLeds();
}

// touch callbacks for the three on-screen parameter buttons - the screen
// equivalent of the physical device.navDownId/device.navSelectId/device.navUpId footswitches
// in the .ino's loop(). Unlike those footswitches (which a config may now
// override with its own Button - see buttonHasExplicitOverride() /
// builtInNavButtons() in Runtime.h), these three ALWAYS run the built-in
// hierarchy navigation role below, regardless of any override, calling
// navStepValue()/navButton10Completed() directly rather than going through
// resolveButton()/triggerButton(). This is deliberate, not an oversight: it
// guarantees there is always a way to navigate Config/Mode/Bank/Patch from
// the device screen even if a config has repurposed all three physical
// footswitches for something else. Do not "simplify" this by routing these
// through the same config-aware dispatch the footswitches use - that would
// remove the one guaranteed way back.
// A screen tap is always a quick tap - the long-press "revert" gesture on
// device.navSelectId is footswitch only for now (see docs/navigation.md
// section 6).
static void screenParamDownCb(lv_event_t *e) {
  (void)e;
  logButton(device.navDownId, "tapped", "on-screen");
  navStepValue(CycleDirection::CycleDown);
}
static void screenParamNameCb(lv_event_t *e) {
  (void)e;
  logButton(device.navSelectId, "tapped", "on-screen");
  navButton10Completed(0); // a tap: advance the selector, or activate if pending
}
static void screenParamUpCb(lv_event_t *e) {
  (void)e;
  logButton(device.navUpId, "tapped", "on-screen");
  navStepValue(CycleDirection::CycleUp);
}

// builds the widget tree once - called from initScreen()
void createScreenWidgets() {
  lv_obj_t *screen = lv_screen_active();

  // --- background: a vertical gradient, [85,85,85] at the top of the screen
  // fading to [0,0,0] at the bottom. LVGL draws a background gradient from
  // bg_color (the start) to bg_grad_color (the end); LV_GRAD_DIR_VER makes
  // it run top-to-bottom. This never changes, so it's set once here rather
  // than in display().
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screen, lv_color_make(85, 85, 85), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

  // --- Header bar: full screen width, flush into the top-left corner. Its
  // background colour (and so its text colour) is refreshed per-config in
  // display() (from config.headerColourCode); the call here just covers the
  // brief moment before the first display() call.
  screenHeader = lv_obj_create(screen);
  lv_obj_set_size(screenHeader, SCREEN_LX, SCREEN_HEADER_H);
  lv_obj_align(screenHeader, LV_ALIGN_TOP_LEFT, 0, 0);
  styleAsBar(screenHeader);

  // config filename (left) and Mode name (right)
  screenConfigLabel = lv_label_create(screenHeader);
  lv_obj_align(screenConfigLabel, LV_ALIGN_LEFT_MID, SCREEN_EDGE_PAD, 0);

  screenModeLabel = lv_label_create(screenHeader);
  lv_obj_align(screenModeLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);

  setBarColours(screenHeader, screenConfigLabel, screenModeLabel, config.headerColourCode);

  // --- SubHeader bar: full width, sat directly below the Header. Its
  // background comes from the selected bank's subheaderColourCode in
  // display(); ModeBank's own default ([44,44,44]) stands in until then.
  screenSubHeader = lv_obj_create(screen);
  lv_obj_set_size(screenSubHeader, SCREEN_LX, SCREEN_SUBHEADER_H);
  lv_obj_align(screenSubHeader, LV_ALIGN_TOP_LEFT, 0, SCREEN_HEADER_H);
  styleAsBar(screenSubHeader);

  // Bank name (left) and Patch name (right)
  screenBankLabel = lv_label_create(screenSubHeader);
  lv_obj_align(screenBankLabel, LV_ALIGN_LEFT_MID, SCREEN_EDGE_PAD, 0);

  screenPatchLabel = lv_label_create(screenSubHeader);
  lv_obj_align(screenPatchLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);

  setBarColours(screenSubHeader, screenBankLabel, screenPatchLabel, ColourCode(44, 44, 44));

  // --- Parameter row: transparent full-width strip below the SubHeader
  screenParamRow = lv_obj_create(screen);
  lv_obj_set_size(screenParamRow, SCREEN_LX, SCREEN_PARAMROW_H);
  lv_obj_align(screenParamRow, LV_ALIGN_TOP_LEFT, 0, SCREEN_PARAMROW_Y);
  lv_obj_remove_flag(screenParamRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(screenParamRow, 0, 0);
  lv_obj_set_style_radius(screenParamRow, 0, 0);
  lv_obj_set_style_pad_all(screenParamRow, 0, 0);
  lv_obj_set_style_bg_opa(screenParamRow, LV_OPA_TRANSP, 0);

  // three flat "buttons" (plain lv_obj - see styleAsParamButton) in a centred
  // group: a narrow left arrow, the wide parameter-name button, a narrow
  // right arrow. LEFT/RIGHT are LVGL's built-in glyphs (part of the default
  // Montserrat font), as asked for - the arrows are drawn by the font, not
  // by bitmaps.
  const int paramGroupW = SCREEN_PARAMBTN_ARROW_W * 2 + SCREEN_PARAMBTN_NAME_W;
  const int paramGroupX = (SCREEN_LX - paramGroupW) / 2;

  screenParamDownBtn = lv_obj_create(screenParamRow);
  lv_obj_set_size(screenParamDownBtn, SCREEN_PARAMBTN_ARROW_W, SCREEN_PARAMROW_H);
  lv_obj_align(screenParamDownBtn, LV_ALIGN_LEFT_MID, paramGroupX, 0);
  styleAsParamButton(screenParamDownBtn);
  screenParamDownLabel = lv_label_create(screenParamDownBtn);
  lv_label_set_text(screenParamDownLabel, LV_SYMBOL_LEFT);
  lv_obj_center(screenParamDownLabel);
  lv_obj_add_event_cb(screenParamDownBtn, screenParamDownCb, LV_EVENT_CLICKED, NULL);

  screenParamNameBtn = lv_obj_create(screenParamRow);
  lv_obj_set_size(screenParamNameBtn, SCREEN_PARAMBTN_NAME_W, SCREEN_PARAMROW_H);
  lv_obj_align(screenParamNameBtn, LV_ALIGN_LEFT_MID, paramGroupX + SCREEN_PARAMBTN_ARROW_W, 0);
  styleAsParamButton(screenParamNameBtn);
  screenParamNameLabel = lv_label_create(screenParamNameBtn);
  lv_label_set_text(screenParamNameLabel, "-");
  lv_obj_center(screenParamNameLabel);
  lv_obj_add_event_cb(screenParamNameBtn, screenParamNameCb, LV_EVENT_CLICKED, NULL);

  screenParamUpBtn = lv_obj_create(screenParamRow);
  lv_obj_set_size(screenParamUpBtn, SCREEN_PARAMBTN_ARROW_W, SCREEN_PARAMROW_H);
  lv_obj_align(screenParamUpBtn, LV_ALIGN_LEFT_MID,
               paramGroupX + SCREEN_PARAMBTN_ARROW_W + SCREEN_PARAMBTN_NAME_W, 0);
  styleAsParamButton(screenParamUpBtn);
  screenParamUpLabel = lv_label_create(screenParamUpBtn);
  lv_label_set_text(screenParamUpLabel, LV_SYMBOL_RIGHT);
  lv_obj_center(screenParamUpLabel);
  lv_obj_add_event_cb(screenParamUpBtn, screenParamUpCb, LV_EVENT_CLICKED, NULL);

  // MIDI channel readout, aligned to the right edge of the row
  screenChannelLabel = lv_label_create(screenParamRow);
  lv_obj_set_style_text_color(screenChannelLabel, lv_color_make(255, 255, 255), 0);
  lv_label_set_text(screenChannelLabel, "Ch. 1");
  lv_obj_align(screenChannelLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);

  // --- Main button grid: device.gridRows rows over device.gridHalfCols
  // half-columns of flat cells (plain lv_obj - see styleAsMainButton), each
  // two half-columns wide, filling the rest of the screen, laid out from
  // device.gridButtonIds (DeviceProfile.h) - that table gives the
  // physical button id of the cell starting in each half-column directly, so
  // no id arithmetic is needed here; an entry of 255 means no cell starts
  // there (it is empty, or covered by the cell to its left).
  // Each cell carries its id as the event user-data so one pair of callbacks
  // serves all of them. Colours, opacity and label text are filled in by the
  // first screenSyncMainButtons() call from display(); here they start
  // transparent with an empty label.
  for (int row = 0; row < device.gridRows; row++) {
    for (int col = 0; col < device.gridHalfCols; col++) {
      int id = device.gridButtonIds[row][col];
      if (id == 255) {
        continue; // no cell starts in this half-column
      }
      int x = SCREEN_BTNGRID_X(col);
      int cellW = SCREEN_BTNGRID_CELL_W(col);
      int y = SCREEN_BTNGRID_Y + row * (SCREEN_BTNGRID_CELL_H + SCREEN_BTNGRID_GAP);

      lv_obj_t *cell = lv_obj_create(screen);
      lv_obj_set_size(cell, cellW, SCREEN_BTNGRID_CELL_H);
      lv_obj_set_pos(cell, x, y);
      styleAsMainButton(cell);
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
      lv_obj_add_event_cb(cell, screenMainBtnPressedCb, LV_EVENT_PRESSED, (void *)(intptr_t)id);
      lv_obj_add_event_cb(cell, screenMainBtnReleasedCb, LV_EVENT_RELEASED, (void *)(intptr_t)id);
      // finger dragged off the cell before lifting - treat as a release too,
      // so a Momentary button (and its held notes) can't get stuck on
      lv_obj_add_event_cb(cell, screenMainBtnReleasedCb, LV_EVENT_PRESS_LOST, (void *)(intptr_t)id);

      lv_obj_t *label = lv_label_create(cell);
      lv_obj_set_width(label, cellW - SCREEN_BTNGRID_GAP * 2);
      lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
      lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_text(label, "");
      lv_obj_center(label);

      screenMainBtnCells[id] = cell;
      screenMainBtnLabels[id] = label;
    }
  }

  // the Live layout's widgets and the button-name overlay - created last so
  // the overlay sits on top of everything else
  screenCreateLiveWidgets(screen);

  // drives the flashing of the parameter-edit preview (see screenSyncParamEdit)
  screenFlashTimer = lv_timer_create(screenFlashTimerCb, 50, NULL);

  screenWidgetsReady = true;
}

// the MIDI Toolbox wordmark shown on the startup screen - "MIDI Toolbox" as
// figlet-style ASCII art (plain printable ASCII: spaces, _ | / \ ( ) < >).
// A C++11 raw string literal (R"DELIM( ... )DELIM") so the art can be pasted
// in verbatim, newlines and backslashes and all, without escaping. The
// single leading and trailing blank lines are part of the art and keep the
// block vertically centred once lv_obj_center() places it.
//
// This art only lines up in a MONOSPACE font, so screenStartupLabel below
// overrides the proportional Montserrat every other label uses with
// lv_font_unscii_8 (a pixel-perfect 8px fixed-width font, enabled via
// LV_FONT_UNSCII_8 in lv_conf.h). unscii_8 advances 8px/glyph; at 33 columns
// wide this art is ~264px, comfortably inside the 320px screen.
static const char *STARTUP_WORDMARK = R"WORDMARK(
  __  __ ___ ___ ___
 |  \/  |_ _|   \_ _|
 | |\/| || || |) | |
 |_|__|_|___|___/___|
 |_   _|__  ___| | |__  _____ __
   | |/ _ \/ _ \ | '_ \/ _ \ \ /
   |_|\___/\___/_|_.__/\___/_\_\

)WORDMARK";

// shows the startup screen - called before the config file has loaded, so it
// can't use display() (which needs Mode/Bank/Patch state). Builds a
// full-screen panel that covers the bars from createScreenWidgets(), paints
// it with the same [85,85,85]->[0,0,0] vertical gradient the main screen
// uses, and centres the wordmark and an (initially empty) progress bar on it
// as one block. Call screenSetStartupProgress() as boot goes on to grow the
// bar; call showStartupFailure() instead of display() if boot doesn't make
// it - see both below.
void showStartupMessage() {
  lv_obj_t *screen = lv_screen_active();

  // the panel: exactly screen-sized, flush to the top-left, opaque, and
  // stripped of the border / rounded corners / padding LVGL puts on a plain
  // object - the same treatment styleAsBar() gives the header bars, done
  // inline here because this object also needs the gradient styles below.
  screenStartupScreen = lv_obj_create(screen);
  lv_obj_set_size(screenStartupScreen, SCREEN_LX, SCREEN_LY);
  lv_obj_align(screenStartupScreen, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_remove_flag(screenStartupScreen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(screenStartupScreen, 0, 0);
  lv_obj_set_style_radius(screenStartupScreen, 0, 0);
  lv_obj_set_style_pad_all(screenStartupScreen, 0, 0);

  // background gradient: [85,85,85] at the top of the screen fading to
  // [0,0,0] at the bottom - bg_color is the start, bg_grad_color the end,
  // LV_GRAD_DIR_VER runs it top-to-bottom (mirrors createScreenWidgets()).
  lv_obj_set_style_bg_opa(screenStartupScreen, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screenStartupScreen, lv_color_make(85, 85, 85), 0);
  lv_obj_set_style_bg_grad_color(screenStartupScreen, lv_color_make(0, 0, 0), 0);
  lv_obj_set_style_bg_grad_dir(screenStartupScreen, LV_GRAD_DIR_VER, 0);

  // the wordmark, white. The label is left-aligned internally - figlet art
  // only lines up as a single left-aligned block (LV_TEXT_ALIGN_CENTER would
  // centre each line on its own width and skew the columns); it's centred as
  // a whole horizontally below, once its real (content-sized) height is
  // known.
  screenStartupLabel = lv_label_create(screenStartupScreen);
  lv_label_set_text(screenStartupLabel, STARTUP_WORDMARK);
  lv_obj_set_style_text_color(screenStartupLabel, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_font(screenStartupLabel, &lv_font_unscii_8, 0);
  lv_obj_set_style_text_align(screenStartupLabel, LV_TEXT_ALIGN_LEFT, 0);

  // the progress bar: a plain white SCREEN_STARTUP_PROGRESS_H-tall bar,
  // created at 0 width (0% progress) - screenSetStartupProgress() grows it.
  // No border/radius/padding, same treatment as the panel above.
  screenStartupProgress = lv_obj_create(screenStartupScreen);
  lv_obj_remove_flag(screenStartupProgress, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(screenStartupProgress, 0, 0);
  lv_obj_set_style_radius(screenStartupProgress, 0, 0);
  lv_obj_set_style_pad_all(screenStartupProgress, 0, 0);
  lv_obj_set_style_bg_opa(screenStartupProgress, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(screenStartupProgress, lv_color_make(255, 255, 255), 0);
  lv_obj_set_size(screenStartupProgress, 0, SCREEN_STARTUP_PROGRESS_H);

  // lv_obj_update_layout() forces LVGL to compute the wordmark label's real,
  // content-sized geometry right now rather than only after the next
  // lv_timer_handler() pass, so its height is available immediately below.
  lv_obj_update_layout(screen);
  int wordmarkH = lv_obj_get_height(screenStartupLabel);

  // the wordmark sits on top, the progress bar directly below it with a
  // small gap, and the whole two-part block is centred vertically on the
  // panel. The wordmark is centred horizontally too (lv_obj_align with
  // LV_ALIGN_TOP_MID); the progress bar's left edge is fixed at
  // SCREEN_STARTUP_PROGRESS_X (set via lv_obj_set_pos, not lv_obj_align, so
  // it stays put as screenSetStartupProgress() grows the bar rightwards
  // rather than re-centring around a moving midpoint).
  int groupH = wordmarkH + SCREEN_STARTUP_PROGRESS_GAP + SCREEN_STARTUP_PROGRESS_H;
  int groupTop = (SCREEN_LY - groupH) / 2;
  lv_obj_align(screenStartupLabel, LV_ALIGN_TOP_MID, 0, groupTop);
  lv_obj_set_pos(screenStartupProgress, SCREEN_STARTUP_PROGRESS_X,
                 groupTop + wordmarkH + SCREEN_STARTUP_PROGRESS_GAP);

  // LVGL doesn't draw anything to the physical screen the instant you
  // create/change a widget - changes just get marked as "needs redrawing"
  // internally. lv_timer_handler() is what actually does the drawing (and
  // processes touch input, animations, etc); it's normally called
  // repeatedly from loop() (see the main .ino), but this screen needs to
  // appear immediately, so it's called once here directly too.
  lv_timer_handler();
}

// grows the startup screen's progress bar to reflect how far boot has got:
// percent 0 is empty, percent 100 is full width (SCREEN_STARTUP_PROGRESS_MAXW,
// 80% of the screen). Out-of-range values are clamped. A no-op if the
// startup screen isn't currently up - either showStartupMessage() hasn't
// been called yet, or display()/showStartupFailure() has already torn it
// down - so callers don't need to guard every call themselves.
void screenSetStartupProgress(int percent) {
  if (screenStartupProgress == nullptr) {
    return;
  }
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }
  lv_obj_set_width(screenStartupProgress, (SCREEN_STARTUP_PROGRESS_MAXW * percent) / 100);
  lv_timer_handler();
}

// replaces the progress bar with a boot-failure message - call this instead
// of display() when something setup() depends on doesn't work out (SD card
// missing, no usable config file, ...), since display() needs a loaded
// config and there isn't one. Leaves the existing gradient panel and
// wordmark from showStartupMessage() in place, moves the wordmark up near
// the top of the screen, and adds a red panel below it - inset
// SCREEN_STARTUP_FAILURE_INSET px from the screen's left/right/bottom edges
// and from the bottom of the wordmark - holding the failure message. There's
// nothing more setup() can do at this point, so this is the last screen the
// player sees until they power-cycle the unit.
void showStartupFailure() {
  if (screenStartupScreen == nullptr) {
    return; // showStartupMessage() hasn't run - nothing to build this onto
  }

  // the progress bar no longer means anything once boot has failed
  if (screenStartupProgress != nullptr) {
    lv_obj_delete(screenStartupProgress);
    screenStartupProgress = nullptr;
  }

  // move the wordmark up near the top of the panel (SCREEN_EDGE_PAD in from
  // the top, matching the inset the Header/SubHeader bars use), then measure
  // where its bottom now falls so the red panel below can be inset from it
  lv_obj_align(screenStartupLabel, LV_ALIGN_TOP_MID, 0, SCREEN_EDGE_PAD);
  lv_obj_update_layout(screenStartupScreen);
  int wordmarkBottom = SCREEN_EDGE_PAD + lv_obj_get_height(screenStartupLabel);

  // the red error panel: inset SCREEN_STARTUP_FAILURE_INSET px from the
  // screen's left/right/bottom edges and from the wordmark's bottom edge
  int boxX = SCREEN_STARTUP_FAILURE_INSET;
  int boxY = wordmarkBottom + SCREEN_STARTUP_FAILURE_INSET;
  int boxW = SCREEN_LX - (SCREEN_STARTUP_FAILURE_INSET * 2);
  int boxH = SCREEN_LY - SCREEN_STARTUP_FAILURE_INSET - boxY;

  lv_obj_t *failureBox = lv_obj_create(screenStartupScreen);
  lv_obj_set_pos(failureBox, boxX, boxY);
  lv_obj_set_size(failureBox, boxW, boxH);
  lv_obj_remove_flag(failureBox, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(failureBox, 0, 0);
  lv_obj_set_style_radius(failureBox, 0, 0);
  lv_obj_set_style_pad_all(failureBox, SCREEN_EDGE_PAD, 0);
  lv_obj_set_style_bg_opa(failureBox, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(failureBox, lv_color_make(200, 0, 0), 0);

  // the message, white, wrapped and centred within the red panel
  lv_obj_t *failureLabel = lv_label_create(failureBox);
  lv_label_set_text(failureLabel, "Startup failed. Please reboot to try again.");
  lv_obj_set_style_text_color(failureLabel, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_align(failureLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(failureLabel, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_width(failureLabel, lv_pct(100));
  // resolve the wrapped label's real height before centring it in the panel
  lv_obj_update_layout(failureBox);
  lv_obj_center(failureLabel);

  lv_timer_handler();
}

// ============================================================================
// setup / update

// initialises the screen driver and LVGL, and builds the widget tree -
// called once from setup(), before the config file is loaded. Deliberately
// does NOT call display() itself: display() reads Mode/Bank/Patch state
// from `config`, which is still empty at this point (it's populated later
// by loadStartupConfig()) - call display() explicitly once that's done.
void initScreen() {
  screenDriver.output(&Serial);
  // "while (!screenDriver.begin(SCREEN_SPI_SPEED));" - a while loop with
  // an empty body (just a ";" and nothing else), which keeps calling
  // begin() over and over until it finally returns true (screenDriver's
  // begin() is written to retry/settle on its own) - a "busy wait", common
  // in embedded code for "block here until the hardware is ready", but
  // rarely seen written quite this tersely in typical C#/Python code
  while (!screenDriver.begin(SCREEN_SPI_SPEED));

  screenDriver.setFramebuffer(screenFramebuffer);
  // "&screenDiff1" - passes the address of the (stack-allocated, see
  // Pins.h) screenDiff1/screenDiff2 objects, since setDiffBuffers() expects
  // pointers to them, not the objects themselves
  screenDriver.setDiffBuffers(&screenDiff1, &screenDiff2);
  // The ILI9341_T4 driver's own docs specify rotations 1 and 3 as landscape
  // (width = SCREEN_LX, height = SCREEN_LY as MidiToolboxDevice.h gives
  // them) and 0 and 2 as portrait (the two swapped) - SCREEN_LX/SCREEN_LY
  // already come pre-swapped for whichever orientation the header selected,
  // so this only has to pick a rotation that matches, not compute the swap
  // itself. Picking 1 for landscape and 0 for portrait (rather than 3/2) is
  // an arbitrary but fixed choice - if touch ends up mirrored on your unit,
  // the other member of the matching pair is the first thing to try instead.
#if defined(SCREEN_ORIENTATION_PORTRAIT)
  screenDriver.setRotation(0);
#else
  screenDriver.setRotation(1);
#endif
  screenDriver.setDiffGap(4);
  screenDriver.setVSyncSpacing(1);
  screenDriver.setRefreshRate(100);

  // known-good calibration values for the reference unit's specific screen
  // and orientation, captured by running screenDriver.calibrateTouch() once
  // (see ILI9341Driver.h for what the 4 values mean) - passing them directly
  // here skips the on-screen calibration step on every boot. These are only
  // valid for that one screen in landscape: a different physical screen, or
  // this same one built in portrait, needs calibrateTouch() run again on the
  // real hardware and these numbers replaced with its own - see BUILD.md.
  static const int touchCalibration[4] = { 245, 3810, 3908, 359 };
  screenDriver.setTouchCalibration(touchCalibration);

  // sets up LVGL's own internal state - must run before any other lv_...
  // function is called
  lv_init();
  // registers screenTickCb (defined above) as the function LVGL calls
  // whenever it needs to know the current time
  lv_tick_set_cb(screenTickCb);

  screenDisplay = lv_display_create(SCREEN_LX, SCREEN_LY);
  // registers screenFlushCb (above) as LVGL's "push pixels to the real
  // screen" callback
  lv_display_set_flush_cb(screenDisplay, screenFlushCb);
  // "nullptr" here for the second buffer means LVGL only gets one draw
  // buffer rather than two alternating ones - simpler, at some cost to
  // rendering throughput; "sizeof(screenLvglBuffer)" asks the compiler to
  // work out that array's total size in bytes automatically, rather than
  // it being written out by hand and risking going out of sync with the
  // array's real size
  lv_display_set_buffers(screenDisplay, screenLvglBuffer, nullptr,
                          sizeof(screenLvglBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

  screenIndev = lv_indev_create();
  lv_indev_set_type(screenIndev, LV_INDEV_TYPE_POINTER);
  // registers screenTouchReadCb (above) as LVGL's "check for touch input"
  // callback
  lv_indev_set_read_cb(screenIndev, screenTouchReadCb);

  createScreenWidgets();
}

// updates the screen to reflect the current runtime state - call this
// whenever any of that state changes, same as the old direct-drawing
// display() function. For now it refreshes the Header and SubHeader bars
// (config file, Mode, Bank, Patch); the MIDI channel and selected-parameter
// readouts come back when the rest of the screen is built.
void display() {
  if (screenDisplay == nullptr) {
    return; // initScreen() hasn't run yet
  }

  // the real UI is about to be filled in - tear down the startup screen if
  // it's still up (created once by showStartupMessage()). Deleting the panel
  // takes its wordmark and progress-bar children with it and uncovers the
  // Header / SubHeader / parameter row underneath; null all three globals so
  // this only runs once and screenSetStartupProgress() cleanly no-ops
  // afterwards instead of touching a deleted object.
  if (screenStartupScreen != nullptr) {
    lv_obj_delete(screenStartupScreen);
    screenStartupScreen = nullptr;
    screenStartupLabel = nullptr;
    screenStartupProgress = nullptr;
  }

  // --- Header: config filename (minus ".json") on the left, Mode on the
  // right. The bar colour is re-applied here - and its text colour
  // re-picked to contrast with it - so a SetConfig/CycleConfig that swaps
  // in a config with a different headerColourCode takes effect.
  setBarColours(screenHeader, screenConfigLabel, screenModeLabel, config.headerColourCode);
  lv_label_set_text(screenConfigLabel, screenConfigDisplayName(currentConfigFilename).c_str());
  lv_label_set_text(screenModeLabel, getCurrentMode().name.c_str());

  // --- SubHeader: Bank on the left, Patch on the right, bar colour from the
  // selected bank's subheaderColourCode - or ModeBank's [44,44,44] default
  // when nothing is selected yet (getCurrentBank() is null - see Runtime.h),
  // in which case "-" is shown for both names.
  ModeBank *bank = getCurrentBank();
  ColourCode subheaderBg = (bank != nullptr) ? bank->subheaderColourCode : ColourCode(44, 44, 44);
  setBarColours(screenSubHeader, screenBankLabel, screenPatchLabel, subheaderBg);
  lv_label_set_text(screenBankLabel, (bank != nullptr) ? bank->name.c_str() : "-");

  // navigationPreviewPatchName() gives the live patch name (with a " / N"
  // page suffix when it has multiple pages), or the pending candidate's name
  // while the Patch level is dirty, or "-" when nothing is selected.
  // screenSyncParamEdit() below re-writes this too, but keep it correct here
  // for the path where that early-returns.
  lv_label_set_text(screenPatchLabel, navigationPreviewPatchName().c_str());

  // the two right-hand labels have just changed text (and so width), so
  // re-run their right-edge alignment - lv_obj_align() positions from the
  // size at the moment it's called, it doesn't re-follow later size changes
  lv_obj_align(screenModeLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);
  lv_obj_align(screenPatchLabel, LV_ALIGN_RIGHT_MID, -SCREEN_EDGE_PAD, 0);

  // --- Layout: the selected bank chooses between the Grid layout and the
  // large-type Live layout (a no-op unless that choice has just changed)
  screenApplyLayout(bank != nullptr && bank->screenLayout == ScreenLayout::ScreenLayoutLive);

  // --- Parameter row: the middle button's parameter name, the "Ch. N"
  // readout, and (if a parameter is armed) the flashing preview values.
  // This overwrites some of the label text set above while an edit is
  // pending, and restores it once the edit commits or times out.
  screenSyncParamEdit();
  // grey the step arrows when the current parameter has nothing to cycle
  screenUpdateParamArrows();

  // --- Main button grid: colour / opacity / label for each non-navigation
  // id, from the Button currently resolved for it and whether that Button is on
  screenSyncMainButtons();

  // --- RGB footswitch LEDs: same colour and on/off state as the grid cells
  // just drawn, mirrored onto the LED behind each switch (see
  // screenSyncButtonLeds)
  screenSyncButtonLeds();
}

// matches #ifndef/#define above
#endif
