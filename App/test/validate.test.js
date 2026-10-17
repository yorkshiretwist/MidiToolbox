'use strict'
const test = require('node:test')
const assert = require('node:assert/strict')
const fs = require('node:fs')
const path = require('node:path')
const { checkDeviceJson, checkConfigJson } = require('../electron/validate')

const repo = path.resolve(__dirname, '..', '..')
const read = (...parts) => fs.readFileSync(path.join(repo, ...parts), 'utf8')

test('the bundled schema copy matches config/device-schema.json (run npm run sync-schemas if not)', () => {
  assert.equal(
    fs.readFileSync(path.join(__dirname, '..', 'schemas', 'device-schema.json'), 'utf8').replace(/\r\n/g, '\n'),
    read('config', 'device-schema.json').replace(/\r\n/g, '\n'),
  )
})

test('the reference device.json for the original unit is accepted', () => {
  assert.deepEqual(checkDeviceJson(read('config', 'live-rig-device.json')), { ok: true })
})

test('a design file is recognised and explained', () => {
  const verdict = checkDeviceJson(read('Designer', 'examples', '1-live-rig.json'))
  assert.equal(verdict.ok, false)
  assert.match(verdict.problem, /design file/)
  assert.match(verdict.problem, /device\.json/)
})

test('text that is not JSON is refused', () => {
  const verdict = checkDeviceJson('{ nope')
  assert.equal(verdict.ok, false)
  assert.match(verdict.problem, /not valid JSON/)
})

function edited(change) {
  const data = JSON.parse(read('config', 'live-rig-device.json'))
  change(data)
  return checkDeviceJson(JSON.stringify(data))
}

test('a duplicate pin is refused', () => {
  const verdict = edited((d) => (d.buttons.pins[1] = d.buttons.pins[0]))
  assert.equal(verdict.ok, false)
  assert.match(verdict.problem, /pin 33/)
})

test('navigation buttons must differ and exist', () => {
  assert.equal(edited((d) => (d.buttons.navigation.up = 10)).ok, false)
  assert.equal(edited((d) => (d.buttons.navigation.up = 13)).ok, false)
})

test('schema violations are described', () => {
  const verdict = edited((d) => (d.leds.registers = 99))
  assert.equal(verdict.ok, false)
  assert.match(verdict.problem, /registers/)
  assert.equal(edited((d) => (d.schemaVersion = 2)).ok, false)
  assert.equal(edited((d) => delete d.screen).ok, false)
})

test('a config only has to be a JSON object', () => {
  assert.deepEqual(checkConfigJson(read('config', 'small-example.json')), { ok: true })
  assert.equal(checkConfigJson('[1,2]').ok, false)
  assert.equal(checkConfigJson('null').ok, false)
  assert.equal(checkConfigJson('{ oops').ok, false)
})
