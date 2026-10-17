# MIDI Toolbox desktop app

The Button Editor, the Device Designer and a **Device** screen in one program. The Device screen talks
to the unit over USB, so you never need the Arduino IDE.

**Everything is in the [desktop app guide](../docs/desktop-app-guide.md)**: downloading and installing
it, setting up a new unit, using the Device screen, safe mode, and how to build the app.

Quick build, from this folder (needs Node.js 24):

```bash
npm install
npm run build:web      # builds ../Editor and ../Designer into web/
npm run loader         # builds the Teensy loader from vendor/ (needs gcc)
npm test
npm start              # run it
npm run dist           # package it for your system, into dist/
```

The firmware `.hex` it installs is not committed: CI builds it for releases, or run `npm run firmware`
to build one yourself (see the guide).

The USB protocol it speaks is described in [docs/serial-protocol.md](../docs/serial-protocol.md).
