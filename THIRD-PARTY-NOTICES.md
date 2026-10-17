# Third-party notices

MIDI Toolbox is licensed under the [GNU General Public License v3.0 or later](LICENSE). It uses and
ships the third-party software below, each under its own licence. This list is kept by hand; if you
spot a mistake or an omission, please [open an issue](https://github.com/yorkshiretwist/MidiToolbox/issues).

## Not affiliated

MIDI Toolbox is an independent, non-commercial hobby project. It is **not affiliated with, endorsed
by or sponsored by** any company or product named in it. Names are used only to say which parts are
compatible or were used, and belong to their owners:

- **PJRC, Teensy and Teensyduino** are the work of PJRC (Paul Stoffregen).
- **Arduino** and the Arduino IDE belong to Arduino S.r.l.
- **Hammond** (Hammond Manufacturing) enclosures appear as examples in the Designer.
- **Farnell** and **Blackstar** are mentioned in the project history.
- **Hacker Shack** and **Notes and Volts** are YouTube channels linked for inspiration.
- **MIDI** is a standard of the MIDI Manufacturers Association and the Association of Musical
  Electronics Industry.
- **Windows** and **Microsoft** belong to Microsoft Corporation, **macOS** and
  **Apple** to Apple Inc., **Linux** to Linus Torvalds, **GitHub** to GitHub, Inc.
- **ILI9341** and **XPT2046** are part numbers of their respective makers, and **74HC595** is a standard part number.

## Firmware (`MIDI-Toolbox/`)

Libraries compiled into the firmware. The versions the firmware is built and tested with are listed
in [docs/hardware-build.md](docs/hardware-build.md#firmware).

| Component | Licence | Source |
| --- | --- | --- |
| ArduinoJson (Benoit Blanchon) | MIT | <https://github.com/bblanchon/ArduinoJson> |
| LVGL | MIT | <https://github.com/lvgl/lvgl> |
| ILI9341_T4 (Arvind Singh) | LGPL-2.1 | <https://github.com/vindar/ILI9341_T4> |
| ShiftPWM (Elco Jacobs), shipped with Teensyduino | LGPL-2.1-or-later | <https://github.com/elcojacobs/ShiftPWM> |
| Bounce (PJRC / Thomas Ouellet Fredericks), shipped with Teensyduino | MIT | <https://github.com/thomasfredericks/Bounce2> |
| Teensy core, SD, SPI, EEPROM and USB-MIDI (PJRC), shipped with Teensyduino | MIT and similar permissive licences | <https://github.com/PaulStoffregen/cores> |
| Montserrat and Unscii fonts, as built into LVGL | SIL Open Font Licence 1.1 and public domain | <https://github.com/lvgl/lvgl> |

`MIDI-Toolbox/lvgl/lv_conf.h` is LVGL's own configuration template (MIT) with a small set of changes.

The compiled firmware that the desktop app installs is built from this source plus the libraries
above. The LGPL libraries are linked statically, as is normal for Arduino sketches; the complete
source of MIDI Toolbox is available in this repository, so you can rebuild the firmware against
modified copies of them.

## Desktop app (`App/`)

| Component | Licence | Source |
| --- | --- | --- |
| Electron (including Chromium and Node.js) | MIT, plus the licences of its parts | <https://github.com/electron/electron> |
| serialport | MIT | <https://github.com/serialport/node-serialport> |
| Ajv | MIT | <https://github.com/ajv-validator/ajv> |
| electron-builder (build tool, not shipped) | MIT | <https://github.com/electron-userland/electron-builder> |
| **teensy_loader_cli** (PJRC), vendored in `App/vendor/teensy_loader_cli/`, compiled and shipped | GPL-3.0 | <https://github.com/PaulStoffregen/teensy_loader_cli> |

Every packaged copy of the app contains Electron's own `LICENSE` and `LICENSES.chromium.html`, which
list the licences of Chromium and its dependencies. The source for `teensy_loader_cli` is in this
repository, unmodified, as the GPL requires; see [App/vendor/README.md](App/vendor/README.md).

## Editor and Designer (`Editor/`, `Designer/`)

Bundled into the web apps and the desktop app:

| Component | Licence | Source |
| --- | --- | --- |
| Vue | MIT | <https://github.com/vuejs/core> |
| Pinia | MIT | <https://github.com/vuejs/pinia> |
| Ajv and ajv-formats | MIT | <https://github.com/ajv-validator/ajv> |
| SortableJS | MIT | <https://github.com/SortableJS/Sortable> |
| vuedraggable | MIT | <https://github.com/SortableJS/Vue.Draggable> |
| fflate | MIT | <https://github.com/101arrowz/fflate> |

Development tools that are not shipped (Vite, Vitest, TypeScript, vue-tsc, jsdom and others) are
listed in each `package.json` and `package-lock.json`, and are all under permissive licences.

## Tests

| Component | Licence | Source |
| --- | --- | --- |
| GoogleTest, vendored in `MIDI-Toolbox/test/vendor/googletest/` | BSD-3-Clause | <https://github.com/google/googletest> |

## Ideas and inspiration

The project history in the [README](README.md#project-history) mentions videos by Hacker Shack and
Notes and Volts. No code or other material from them is used.
