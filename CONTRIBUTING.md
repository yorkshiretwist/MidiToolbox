# Contributing to MIDI Toolbox

Thank you for thinking of helping! This is a small hobby project, so please be patient with replies.
By taking part you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to help

- **Report a bug** or ask a question: [open an issue](https://github.com/yorkshiretwist/MidiToolbox/issues/new/choose).
  Say what you did, what you expected and what happened, and include your operating system, the app
  version and (for hardware problems) your firmware version and how your unit is built.
- **Suggest a feature.** Explain the problem you want solved, not just the solution.
- **Improve the documentation**, especially the build guides. If you built a unit, photos and notes
  about what was hard are very welcome.
- **Send a pull request.** For anything bigger than a small fix, open an issue first so we can agree
  the approach before you spend time on it.

Safety matters here: building a unit involves drilling, soldering and mains-powered tools. Please do
not remove or weaken the warnings about measuring parts and verifying wiring, and mention any wiring
change in your pull request so it can be checked.

## Licensing of contributions

MIDI Toolbox is licensed under the [GNU General Public License v3.0 or later](LICENSE). By submitting
a contribution you confirm that you wrote it (or have the right to share it) and that you license it
under the same terms (GPL-3.0-or-later). There is no separate contributor agreement. If you add code
or files from somewhere else, say where from and under which licence, and add it to
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md); it must be compatible with the GPL.

## What is where

| Folder | What it is | Needs |
| --- | --- | --- |
| `MIDI-Toolbox/` | Firmware (Arduino C++ for a Teensy 4.1) and its unit tests | Arduino IDE + Teensyduino to build; g++ for the tests |
| `Editor/` | Web app: configure what the buttons do (Vue 3, TypeScript, Vite) | Node.js 24 |
| `Designer/` | Web app: design your own unit | Node.js 24 |
| `App/` | Desktop app (Electron) wrapping both, plus USB tools | Node.js 24, gcc (for the Teensy loader) |
| `config/` | JSON schemas and example files shared by all of them | |
| `docs/` | The guides | |

The schemas in `config/` are the source of truth; `npm run sync-schema` (run automatically by the
Editor and Designer) copies them into each app.

## Building and testing

Run the tests for the part you changed. Pull requests are checked by [CI](.github/workflows/ci.yml),
which runs all of these.

```bash
# Editor and Designer (the same commands in each folder)
cd Editor
npm ci
npm run typecheck
npm run test:run
npm run build
npm run dev          # run it locally

# Desktop app
cd App
npm ci
npm test
npm run build:web    # builds ../Editor and ../Designer into web/
npm run loader       # builds the Teensy loader from vendor/ (needs gcc)
npm start

# Firmware unit tests (they run on your computer, not on a Teensy)
bash MIDI-Toolbox/test/run_tests.sh
```

See [the firmware tests README](MIDI-Toolbox/test/README.md) and the
[desktop app guide](docs/desktop-app-guide.md#building-the-app-yourself). To build the firmware itself,
follow [docs/hardware-build.md](docs/hardware-build.md#firmware).

If you change what a button, action or file can contain, change all of these together: the schema in
`config/`, the firmware, the Editor (including its Play mode, which mirrors the firmware) and the docs.

## Pull requests

- Keep each pull request to one change, and describe what it does and why.
- Add or update tests, and make sure the tests pass.
- Match the style of the surrounding code and comments.
- Update the docs if behaviour changes.
- Do not commit compiled files (`.hex`, `.o`, `.exe`) or `node_modules`/`dist` folders.

## Making a release

Only the maintainer does this. A release is built and published **only when a version tag is pushed**
on a commit that is on `main`. Pushing to `main` (including docs) never creates a release.

1. Add a section for the new version at the top of [`release.md`](release.md), headed
   `## <major>.<minor>.<patch> - <date>`; its text becomes the release notes.
2. Make sure the version in `MIDI-Toolbox/SerialProtocol.h` (`FIRMWARE_VERSION`) and the
   `MIDI-Toolbox.ino` `version` string is the version you intend to ship.
3. Merge to `main`, then tag and push:

   ```bash
   git checkout main && git pull
   git tag v0.1.0
   git push origin v0.1.0
   ```

4. The [release workflow](.github/workflows/release.yml) builds the firmware and the Windows, Linux
   and macOS apps and attaches them to a new GitHub release. If a tag was a mistake, delete the
   release and the tag (`git push origin --delete v0.1.0`) and tag again.
