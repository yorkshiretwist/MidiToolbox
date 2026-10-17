// Builds the firmware into App/firmware/ so the app can ship it: MIDI-Toolbox.ino.hex plus
// firmware.json (its version, read from FIRMWARE_VERSION in MIDI-Toolbox/SerialProtocol.h).
//
// Needs arduino-cli with the Teensy core and the libraries the sketch uses (ArduinoJson, lvgl with
// the project's lv_conf.h, ILI9341_T4) installed - the same setup the Arduino IDE uses. It looks for
// arduino-cli in: the ARDUINO_CLI environment variable, your PATH, then the copy that comes with
// the Arduino IDE.
//
// The result is not committed (App/firmware/ is git-ignored): the release workflow builds it, and you
// can build it yourself to try the desktop app's firmware update with:
//   npm run firmware

import { spawnSync } from 'node:child_process'
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { homedir, tmpdir } from 'node:os'
import { delimiter, dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const repo = resolve(here, '../..')
const sketch = resolve(repo, 'MIDI-Toolbox')
const outDir = resolve(here, '../firmware')

// Teensy 4.1, USB Serial + MIDI (the serial protocol needs Serial; the unit is also a MIDI device),
// 600 MHz, "Faster" optimisation - the Teensyduino defaults.
const FQBN = 'teensy:avr:teensy41:usb=serialmidi,speed=600,opt=o2std,keys=en-us'
const HEX = 'MIDI-Toolbox.ino.hex'

function findArduinoCli() {
  if (process.env.ARDUINO_CLI && existsSync(process.env.ARDUINO_CLI)) return process.env.ARDUINO_CLI
  const exe = process.platform === 'win32' ? 'arduino-cli.exe' : 'arduino-cli'
  for (const dir of (process.env.PATH || '').split(delimiter)) {
    if (dir && existsSync(join(dir, exe))) return join(dir, exe)
  }
  const ide = {
    win32: [join(process.env.LOCALAPPDATA || '', 'Programs', 'Arduino IDE', 'resources', 'app', 'lib', 'backend', 'resources', exe)],
    darwin: ['/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'],
    linux: [join(homedir(), 'arduino-ide', 'resources', 'app', 'lib', 'backend', 'resources', exe)],
  }[process.platform] ?? []
  return ide.find((p) => existsSync(p)) ?? null
}

const cli = findArduinoCli()
if (!cli) {
  console.error('[firmware] arduino-cli was not found. Install it, or set ARDUINO_CLI to its path.')
  process.exit(1)
}

const header = readFileSync(resolve(sketch, 'SerialProtocol.h'), 'utf8')
const version = /#define FIRMWARE_VERSION "([^"]+)"/.exec(header)?.[1]
if (!version) {
  console.error('[firmware] could not read FIRMWARE_VERSION from MIDI-Toolbox/SerialProtocol.h')
  process.exit(1)
}

const buildPath = mkdtempSync(join(tmpdir(), 'miditoolbox-fw-'))
console.log(`[firmware] compiling ${sketch} for ${FQBN} (this takes a few minutes)`)
const result = spawnSync(cli, ['compile', '--fqbn', FQBN, '--build-path', buildPath, sketch], { stdio: 'inherit' })
if (result.status !== 0) {
  console.error('[firmware] the compile failed')
  rmSync(buildPath, { recursive: true, force: true })
  process.exit(result.status ?? 1)
}

mkdirSync(outDir, { recursive: true })
copyFileSync(join(buildPath, HEX), join(outDir, HEX))
writeFileSync(join(outDir, 'firmware.json'), JSON.stringify({ version, file: HEX, fqbn: FQBN }, null, 2) + '\n')
rmSync(buildPath, { recursive: true, force: true })
console.log(`[firmware] wrote ${join(outDir, HEX)} (version ${version})`)
