# Releases

The notes for each release, newest first. A release is published only when you push a version tag
on a commit that is on `main` (see [CONTRIBUTING.md](CONTRIBUTING.md#making-a-release)); editing this
file or anything else on `main` never builds one. The section whose heading is `## <version>`
(for example `## 0.1.0 - 2026-10-15`) becomes the release notes for the tag `v<version>`.

## 0.2.0 - 2026-11-12

**Firmware.** Update the unit to 0.2.0 from the Device screen (your configs and `device.json` are not touched).

- A multi-page patch always shows **Page** in the parameter selector, even when the mode, bank or
  patch restricts its `parameters` list without it, so the player can always step pages.
- The startup screen shows the new MIDI Toolbox wordmark as a graphic instead of ASCII art.
- Startup-only code now lives in flash, freeing RAM1 so the firmware keeps room to grow.

**Desktop app.**

- A redesigned Device screen: a plain-words status panel, a firmware panel with step-by-step progress
  and a reassurance that updates never erase your files, a log sidebar and a help section.
- A **File** menu in the Editor and Designer with Open, Open recent, Save and Save as (with
  Ctrl/Cmd+O, S and Shift+S) working on real files on disk.
- A splash window while the app starts, and a new app icon.

**Editor and Designer.**

- Footswitches can be swapped by drag and drop at the File, Mode and Bank levels as well as on a Patch.
- Moving to another page of a patch keeps the same footswitch selected instead of leaving a stale
  selection from the previous page.
- New MIDI Toolbox wordmark and favicon.

**Docs and website.** Refreshed README, guides and project website with the new branding.

## 0.1.0 - 2026-10-21

First release of the desktop app: the Editor, the Designer and a Device screen that installs
`device.json`, uploads configs, restarts the unit and updates its firmware over USB.
