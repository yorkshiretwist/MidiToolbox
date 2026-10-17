#!/usr/bin/env bash
#
# Builds and runs the MIDI Toolbox unit test suite. See README.md in this
# directory for what's covered and why the project is structured this way.
#
# Requirements:
#   - g++ supporting C++17
#   - the ArduinoJson library (the one you already use to build the real
#     firmware) - this script looks in the usual Arduino libraries folder,
#     or set ARDUINOJSON_DIR yourself if yours lives somewhere else:
#       ARDUINOJSON_DIR=/path/to/ArduinoJson/src ./run_tests.sh
#
# GoogleTest itself is vendored in test/vendor/googletest, so nothing needs
# to be downloaded to run this.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"
GTEST_DIR="$SCRIPT_DIR/vendor/googletest/googletest"

# find ArduinoJson if the caller didn't set ARDUINOJSON_DIR themselves
if [ -z "$ARDUINOJSON_DIR" ]; then
  for candidate in \
    "$HOME/Documents/Arduino/libraries/ArduinoJson/src" \
    "$HOME/Arduino/libraries/ArduinoJson/src" \
    "$HOME/sketchbook/libraries/ArduinoJson/src"; do
    if [ -d "$candidate" ]; then
      ARDUINOJSON_DIR="$candidate"
      break
    fi
  done
fi

if [ -z "$ARDUINOJSON_DIR" ] || [ ! -d "$ARDUINOJSON_DIR" ]; then
  echo "Could not find the ArduinoJson library."
  echo "Install it via the Arduino Library Manager (the same one the real"
  echo "firmware needs), or point this script at it directly:"
  echo "  ARDUINOJSON_DIR=/path/to/ArduinoJson/src $0"
  exit 1
fi

echo "Using ArduinoJson from: $ARDUINOJSON_DIR"
mkdir -p "$BUILD_DIR"

# Build GoogleTest itself, only if not already built (this doesn't change
# between runs, so there's no need to redo it every time)
if [ ! -f "$BUILD_DIR/gtest-all.o" ]; then
  echo "Building GoogleTest..."
  g++ -std=gnu++17 -I "$GTEST_DIR/include" -I "$GTEST_DIR" \
    -c "$GTEST_DIR/src/gtest-all.cc" -o "$BUILD_DIR/gtest-all.o"
  g++ -std=gnu++17 -I "$GTEST_DIR/include" -I "$GTEST_DIR" \
    -c "$GTEST_DIR/src/gtest_main.cc" -o "$BUILD_DIR/gtest_main.o"
fi

echo "Building the stub environment..."
g++ -std=gnu++17 -DARDUINO=1 -I "$SCRIPT_DIR/stubs" \
  -c "$SCRIPT_DIR/stubs/stub_globals.cpp" -o "$BUILD_DIR/stub_globals.o"

echo "Building the test suite..."
# MIDITOOLBOX_PROJECT_DIR lets a test locate real repo assets (e.g. a shipped
# config file under configs/) without guessing from the test binary's own
# working directory at run time, which depends on where run_tests.sh itself
# was invoked from and isn't reliable.
g++ -std=gnu++17 -DARDUINO=1 -Wall -Wextra -Wno-deprecated-declarations \
  -DMIDITOOLBOX_PROJECT_DIR="\"$PROJECT_DIR\"" \
  -I "$SCRIPT_DIR/stubs" -I "$PROJECT_DIR" -I "$ARDUINOJSON_DIR" \
  -I "$GTEST_DIR/include" -I "$SCRIPT_DIR" \
  -c "$SCRIPT_DIR/test_main.cpp" -o "$BUILD_DIR/test_main.o"

# On MSYS2 / MinGW-w64 (Windows), libmingw32's UCRT exit wrappers pull in
# __imp__Exit / __imp_quick_exit, which live in libmingwex - the g++ driver
# doesn't add it by default here, so the link fails without it. Harmless to
# omit everywhere else (the library only exists on MinGW).
EXTRA_LDLIBS=""
case "$(uname -s)" in
  MINGW*|MSYS*) EXTRA_LDLIBS="-lmingwex" ;;
esac

echo "Linking..."
g++ "$BUILD_DIR/test_main.o" "$BUILD_DIR/stub_globals.o" \
  "$BUILD_DIR/gtest-all.o" "$BUILD_DIR/gtest_main.o" \
  -std=gnu++17 -pthread -o "$BUILD_DIR/miditoolbox_tests" $EXTRA_LDLIBS

echo ""
echo "Running tests..."
echo ""
"$BUILD_DIR/miditoolbox_tests" "$@"
