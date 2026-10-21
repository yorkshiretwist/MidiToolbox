# The desktop app

One download that holds the **Editor**, the **Designer** and a **Device** screen. The Device screen
talks to your unit over USB, so you never need the Arduino IDE: it installs the unit's wiring
(`device.json`), uploads your configs, restarts the unit, recovers it from safe mode and updates
its firmware.

> ⚠️ **Build at your own risk.** Updating firmware and installing a `device.json` change what your unit does with its pins. Check that the `device.json` matches how your unit is really wired, and verify your wiring with a multimeter before you apply power (see the [hardware build](hardware-build.md)). Everything is provided "as is", with no warranty.

It runs on Windows, Linux and macOS. The Editor and Designer inside it are exactly the ones on the
web site, and work offline; nothing is uploaded anywhere.

## Installing it

Plug the unit in with a USB **data** cable (some cables only charge), then download the file for your
system from the [latest release](https://github.com/yorkshiretwist/MidiToolbox/releases/latest):

| System | File | First run |
| --- | --- | --- |
| Windows | `MIDI-Toolbox-<version>-windows-portable.exe` | Double-click. Windows may warn you: see [Windows and macOS warnings](#windows-and-macos-warnings). |
| Linux | `MIDI-Toolbox-<version>-linux.AppImage` | Make it executable (`chmod +x`) and run it. Do the one-time setup below. |
| macOS | `MIDI-Toolbox-<version>-macos.zip` | Unzip, then open the app. macOS may refuse: see [Windows and macOS warnings](#windows-and-macos-warnings). |

Windows and Linux are tested by the author; macOS only occasionally.

### Windows and macOS warnings

The app is **not code-signed**. Signing means paying a company (Microsoft or Apple, or a certificate
authority) to vouch for who published a program, and this is a free hobby project. An unsigned app
is not necessarily unsafe, but Windows and macOS cannot tell who made it, so they warn you the first
time you run it. You are trusting this project, so only download it from the
[releases page](https://github.com/yorkshiretwist/MidiToolbox/releases) of this repository. The
source is all here to read, and the release is built by the public
[release workflow](../.github/workflows/release.yml).

**Windows.** SmartScreen shows "Windows protected your PC" or "Unknown publisher".

1. Click **More info**.
2. Click **Run anyway**.

If the file was blocked when it downloaded, right-click it, choose **Properties**, tick **Unblock**
at the bottom of the General tab, and click **OK**. Your antivirus may also scan it the first
time; that can take a moment.

**macOS.** You may see "MIDI Toolbox can't be opened because Apple cannot check it for malicious
software", or that it is "damaged".

1. Unzip the download and move **MIDI Toolbox** to your **Applications** folder.
2. Try to open it once, then click **Done** (not "Move to Bin") on the warning.
3. Open **System Settings > Privacy & Security**, scroll down to the message about MIDI Toolbox, and
   click **Open Anyway**. Confirm with your password.

If macOS still says it is "damaged", it is only the download quarantine flag. Remove it in
Terminal, then open the app again:

```bash
xattr -dr com.apple.quarantine "/Applications/MIDI Toolbox.app"
```

Linux does not show a warning.

### Checking your download

Every release lists a SHA-256 checksum for each file (also in `SHA256SUMS.txt` on the release page).
Check that your download matches:

```bash
sha256sum MIDI-Toolbox-0.1.0-linux.AppImage      # Linux
shasum -a 256 MIDI-Toolbox-0.1.0-macos.zip        # macOS
```

```powershell
Get-FileHash .\MIDI-Toolbox-0.1.0-windows-portable.exe -Algorithm SHA256   # Windows
```

The files also carry a signed record (an "attestation") that GitHub's release workflow built them
from this repository. With the [GitHub CLI](https://cli.github.com/) installed you can verify it:

```bash
gh attestation verify MIDI-Toolbox-0.1.0-linux.AppImage --repo yorkshiretwist/MidiToolbox
```

This is not the same as code signing, so Windows and macOS will still warn you, but it proves the
file was not swapped or tampered with after it was built.

Switch between the three screens with the **Go** menu, or **Ctrl/Cmd + 1** (Editor), **2**
(Designer), **3** (Device).

### Linux: one-time setup

So that you can use the unit without administrator rights, install the rules file that is inside
the app. The Device screen's **Help with connecting** shows these commands with the right path
filled in:

```bash
sudo cp 00-teensy.rules /etc/udev/rules.d/00-teensy.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug the unit and plug it in again. Updating firmware also needs `libusb` 0.1 on the
computer (`sudo apt install libusb-0.1-4` on Debian/Ubuntu). If the window does not open from the
AppImage, run it once with `--no-sandbox`.

## Setting up a brand-new unit

1. In the **Designer**, finish your design and, in the Export step, download `device.json`.
2. Open the **Device** screen and **Connect** (pick the unit from the list; Teensy ports are marked).
3. If the Firmware panel says the unit has no firmware, or an older version than the app has, press
   **Install the firmware that came with this app**. The unit restarts and the app reconnects.
4. **Upload device.json...**, then **Restart the unit**.
5. In the **Editor**, save your config, then back on the Device screen **Upload configs...**.

For the original 13-button unit use [`config/live-rig-device.json`](../config/live-rig-device.json)
instead of step 1.

## The Device screen

- **Unit** - choose the port, **Connect**. It shows the firmware version, the unit's name, where its
  wiring came from, whether it is in safe mode and whether any buttons are currently on.
- **Unit wiring (`device.json`)** - whether the unit has one; upload a new one or download the
  current one.
- **Configs (`/configs`)** - list, upload (several at once), download and delete the config files on
  the SD card. The unit remembers which one was last selected.
- **Restart** - restarts the unit. If any buttons are on, the app asks first: a restart would leave
  notes stuck on, so it releases the buttons (sending the note-offs) and then restarts.
- **Firmware** - installed version against the version bundled with the app. You can also choose any
  other `.hex` file. While flashing, the unit disappears from the port list for a few seconds; the
  app waits for it and reconnects. **Cancel** is available while it waits.
- **Log** - what the app has done, useful when reporting a problem.

Common first-run trouble is explained inside the app: on Linux the serial permissions (the rules
file above), and the unit being held by another program (the Arduino Serial Monitor or the Teensy
Loader). Close the other program and press **Connect** again.

## Safe mode

A unit with no valid `device.json` on its SD card is in *safe mode*. It drives no buttons, screen or
LEDs at all, but it still answers over USB so the app can fix it. There is
deliberately no fall-back to the 13-button wiring, because on different wiring it could drive a pin
that is wired to a button and damage something. The Device screen shows a banner explaining this
whenever a unit is in safe mode. Upload a correct `device.json` and restart.

(If you build the firmware yourself you can change this: set `DEVICE_JSON_REQUIRED` to 0 in
`DeviceStore.h` and a missing file falls back to the wiring compiled in from `MidiToolboxDevice.h`.
A file that is present but invalid always means safe mode.)

## The files on a unit

| File | Written by | Read by | What it is |
| --- | --- | --- | --- |
| `device.json` | Designer | Firmware | The resolved wiring: the pin for every button, the navigation buttons, the screen, the grid, the LED chain length. Schema: [`config/device-schema.json`](../config/device-schema.json) |
| `/configs/*.json` | Editor | Firmware | What the buttons do. Schema: [`config/schema.json`](../config/schema.json) |
| `touch.json` | Firmware | Firmware | Touch-screen calibration, saved by the unit itself |
| `<name>-design.json` | Designer | Designer, Editor | Your design (millimetre layout, pin overrides). It stays on your computer; the unit never needs it |
| `MidiToolboxDevice.h` | Designer | Compiler | The compile-time equivalent of `device.json`, only for people who build the firmware themselves |

Only one `device.json` exists on a unit, and it is read once at start-up, so restart after
uploading it. The LED chain pins are fixed at 28 (latch), 29 (data) and 30 (clock) because the
ShiftPWM library needs them at compile time; the Designer reports an error if a design moves them.
Uploaded configs are read when the unit selects them.

## Current limitations

- **Screens.** The firmware can only drive the screen it was built for (the ILI9341 in landscape).
  A `device.json` that describes a different screen is refused. Choosing the screen at run time is
  planned but not done: it means turning the screen code's compile-time sizes into run-time ones,
  and every screen needs checking on hardware.
- The Designer and Editor have no "send to unit" button; use the Device screen's file pickers.
- No auto-update, installers or custom icon.
- Not decided yet: a version handshake between app, firmware and `device.json`'s `schemaVersion`,
  and whether an uploaded config should take effect immediately rather than on the next boot or
  config switch.
- Building the firmware is not part of the automatic build (it needs the Teensy toolchain), so the
  firmware bundled in the app is whatever was last committed.

## Building the app yourself

You only need this if you are changing the app. It needs Node.js 24. From the `App/` folder:

```bash
npm install
npm run build:web      # builds ../Editor and ../Designer into web/
npm run loader         # builds the Teensy loader from vendor/ (needs gcc; Linux also needs libusb-dev)
npm test
npm start              # run it
npm run dist           # package it for the system you are on, into dist/
```

`npm run firmware` builds the firmware that ships inside the app into `App/firmware/`. It needs the
Arduino toolchain (see the top of `App/scripts/build-firmware.mjs`). The `.hex` is **not committed**
(`App/firmware/` is git-ignored): the release workflow builds it, and you only need to run this to
try the app's firmware update yourself. Without it the app still works, but **Install bundled
firmware** reports that none is inside it (use **Install a different .hex** instead). If you have
built one, a test fails when it is for a different version than `MIDI-Toolbox/SerialProtocol.h` says.

GitHub Actions builds and tests everything:

- [`ci.yml`](../.github/workflows/ci.yml) runs the Editor, Designer, app and firmware tests on every
  pull request.
- [`release.yml`](../.github/workflows/release.yml) builds the firmware, the web apps, the loader
  from the vendored source and the packaged app for all three systems, and publishes a GitHub
  release. It runs **only when you push a version tag** (such as `v0.1.0`) on a commit on `main`; see
  [CONTRIBUTING.md](../CONTRIBUTING.md#making-a-release).

### Talking to a unit from the command line

For testing the firmware without the app's window:

```bash
npm run cli -- ports
npm run cli -- COM3 info
```

The top of `App/scripts/cli.js` lists the other commands (`list`, `get`, `put`, `delete`, `reboot`,
`bootloader`, `stress`). The commands behind them are described in the
[USB serial protocol](serial-protocol.md).

## How it is put together

| Folder | What is in it |
| --- | --- |
| `App/electron/` | The main process: window, menu, the `app://` server, the serial link, flashing |
| `App/device-ui/` | The Device screen (plain HTML and JavaScript) |
| `App/web/` | The built Editor and Designer (generated, not committed) |
| `App/firmware/` | The bundled firmware `.hex` and its version |
| `App/vendor/` | Source of the open-source Teensy loader |
| `App/resources/` | The built loader (generated) and the Linux rules file |
| `App/schemas/` | Copy of `config/device-schema.json`, for checking a file before it is sent |
| `App/scripts/` | Build scripts and the command-line tool |

Why it is built this way:

- **Electron**, with plain JavaScript for the app's own code. The Editor and Designer stay Vue and
  TypeScript, and are served unchanged, laid out as on GitHub Pages (Editor at `/editor/`, Designer at
  `/designer/`), so neither needed any change for the app.
- **One window, three screens.** `app://local/` is a custom protocol that serves the built web apps
  and the Device screen, and refuses any path outside those folders. Pages run with context
  isolation and no Node access.
- **Only the Device screen can reach the serial port.** The preload script checks the page address
  before exposing anything, so the Editor and Designer pages cannot.
- **Flashing uses the open-source `teensy_loader_cli`** (GPL v3, like this project). It has no
  official Windows binary, so it is built from source for each system (MinGW on Windows) and shipped
  inside the app. Its source is vendored in `App/vendor/` so builds need no network and the source is
  offered alongside the binary.
- **The firmware is bundled** in release builds (built by CI into `App/firmware/`), so flashing works offline;
  "Install a different .hex" stays available for advanced users.
- **The serial link** frames each command as a line, checks every file with a CRC-32, ignores the
  firmware's `# ` log lines and never restarts a unit with active buttons without the player's
  agreement. See the [protocol](serial-protocol.md).
