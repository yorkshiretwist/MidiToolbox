'use strict'
// The firmware that ships inside the app: App/firmware/MIDI-Toolbox.ino.hex plus firmware.json
// (written by scripts/build-firmware.mjs, which the release workflow runs) saying which version it
// is. Neither is committed, so a local copy of the app may have none.

const fs = require('node:fs')
const path = require('node:path')

// "0.1.0" vs "0.10.2": compares the dot-separated numbers, not the text. Returns -1, 0 or 1.
// A version that is not made of numbers (e.g. "dev") sorts as 0.
function compareVersions(a, b) {
  const parts = (v) => String(v).split('.').map((n) => (/^\d+$/.test(n) ? Number(n) : 0))
  const left = parts(a)
  const right = parts(b)
  for (let i = 0; i < Math.max(left.length, right.length); i++) {
    const l = left[i] ?? 0
    const r = right[i] ?? 0
    if (l !== r) return l < r ? -1 : 1
  }
  return 0
}

// What is bundled, or null if no firmware has been built into this copy of the app.
function bundledFirmware(firmwareDir) {
  try {
    const meta = JSON.parse(fs.readFileSync(path.join(firmwareDir, 'firmware.json'), 'utf8'))
    const file = path.join(firmwareDir, meta.file)
    if (!fs.existsSync(file)) return null
    return { version: meta.version, file, fileName: meta.file, builtAt: meta.builtAt ?? null }
  } catch {
    return null
  }
}

// Whether the unit's firmware should be updated to the bundled one:
// 'older' (update available), 'same', 'newer' (unit is ahead of this app), or 'unknown'.
function compareInstalled(installedVersion, bundledVersion) {
  if (!installedVersion || !bundledVersion) return 'unknown'
  const c = compareVersions(installedVersion, bundledVersion)
  return c < 0 ? 'older' : c > 0 ? 'newer' : 'same'
}

module.exports = { compareVersions, bundledFirmware, compareInstalled }
