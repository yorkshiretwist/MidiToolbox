'use strict'
const test = require('node:test')
const assert = require('node:assert/strict')
const fs = require('node:fs')
const os = require('node:os')
const path = require('node:path')
const { compareVersions, bundledFirmware, compareInstalled } = require('../electron/firmware')
const { findLoader, loaderArgs, EXECUTABLE } = require('../electron/loader')

test('versions compare by number, not by text', () => {
  assert.equal(compareVersions('0.1.0', '0.1.0'), 0)
  assert.equal(compareVersions('0.1.0', '0.10.0'), -1)
  assert.equal(compareVersions('1.0.0', '0.9.9'), 1)
  assert.equal(compareVersions('1.2', '1.2.0'), 0)
  assert.equal(compareVersions('dev', 'dev'), 0)
})

test('the installed firmware is classed against the bundled one', () => {
  assert.equal(compareInstalled('0.0.9', '0.1.0'), 'older')
  assert.equal(compareInstalled('0.1.0', '0.1.0'), 'same')
  assert.equal(compareInstalled('0.2.0', '0.1.0'), 'newer')
  assert.equal(compareInstalled(undefined, '0.1.0'), 'unknown')
  assert.equal(compareInstalled('0.1.0', undefined), 'unknown')
})

test('bundled firmware is read from firmware.json', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'fw-'))
  assert.equal(bundledFirmware(dir), null)
  fs.writeFileSync(path.join(dir, 'firmware.json'), JSON.stringify({ version: '0.1.0', file: 'a.hex' }))
  assert.equal(bundledFirmware(dir), null) // the hex itself is missing
  fs.writeFileSync(path.join(dir, 'a.hex'), ':00000001FF\n')
  const found = bundledFirmware(dir)
  assert.equal(found.version, '0.1.0')
  assert.equal(found.file, path.join(dir, 'a.hex'))
  fs.rmSync(dir, { recursive: true })
})

// The .hex is not committed: CI builds it (see .github/workflows/release.yml). If a developer has built
// one locally with "npm run firmware", check that it matches the source; otherwise there is nothing to check.
test('a locally built firmware is the version the source says it is', (t) => {
  const header = fs.readFileSync(path.join(__dirname, '..', '..', 'MIDI-Toolbox', 'SerialProtocol.h'), 'utf8')
  const source = /#define FIRMWARE_VERSION "([^"]+)"/.exec(header)[1]
  const built = bundledFirmware(path.join(__dirname, '..', 'firmware'))
  if (!built) return t.skip('no firmware built here: run npm run firmware to build one')
  assert.equal(built.version, source, 'the built hex is stale: run npm run firmware again')
})

test('the loader is found: chosen first, then bundled, then PATH', () => {
  const exists = (set) => (file) => set.includes(file)
  const bundled = path.join('/app/loader', EXECUTABLE)
  const onPath = path.join('/usr/local/bin', EXECUTABLE)
  const env = { PATH: ['/bin', '/usr/local/bin'].join(path.delimiter) }

  assert.deepEqual(findLoader({ chosenPath: '/mine', loaderDir: '/app/loader', env, exists: exists(['/mine', bundled, onPath]) }), {
    path: '/mine',
    source: 'chosen',
  })
  assert.deepEqual(findLoader({ chosenPath: '/gone', loaderDir: '/app/loader', env, exists: exists([bundled, onPath]) }), {
    path: bundled,
    source: 'bundled',
  })
  assert.deepEqual(findLoader({ loaderDir: '/app/loader', env, exists: exists([onPath]) }), { path: onPath, source: 'path' })
  assert.equal(findLoader({ loaderDir: '/app/loader', env, exists: exists([]) }), null)
})

test('the loader is run for a Teensy 4.1 and waits for the bootloader', () => {
  assert.deepEqual(loaderArgs('/x/fw.hex'), ['--mcu=TEENSY41', '-w', '-v', '/x/fw.hex'])
})
