'use strict'
// Checks a file before it is sent to the unit, so a mistake is caught on the computer instead of
// leaving the unit in safe mode. The unit makes its own, stricter checks (see DeviceStore.h); this
// catches the usual slips: choosing the Designer's design file instead of device.json, or a file
// that is not JSON at all.

const Ajv = require('ajv')
const deviceSchema = require('../schemas/device-schema.json')

const ajv = new Ajv({ allErrors: true, strict: false })
const validateDevice = ajv.compile(deviceSchema)

// Returns { ok: true } or { ok: false, problem: "plain-English reason" }.
function checkDeviceJson(text) {
  let data
  try {
    data = JSON.parse(text)
  } catch (err) {
    return { ok: false, problem: `This is not valid JSON (${err.message}).` }
  }
  if (data && typeof data === 'object' && !Array.isArray(data) && data.unit && !data.screen?.grid) {
    return {
      ok: false,
      problem:
        'This looks like a design file (<name>-design.json), which is for editing in the Designer. ' +
        'The unit needs the "device.json" file: in the Designer\'s Export step, download "device.json".',
    }
  }
  if (!validateDevice(data)) {
    const first = validateDevice.errors[0]
    const where = first.instancePath || 'the file'
    return { ok: false, problem: `This is not a valid device.json: ${where} ${first.message}.` }
  }

  // things a schema cannot say, which the unit would reject
  const pins = data.buttons.pins
  const seen = new Set()
  for (const pin of pins) {
    if (seen.has(pin)) return { ok: false, problem: `Two buttons use pin ${pin}.` }
    seen.add(pin)
  }
  const { select, down, up } = data.buttons.navigation
  if (new Set([select, down, up]).size !== 3) {
    return { ok: false, problem: 'The three navigation buttons must be different buttons.' }
  }
  for (const id of [select, down, up]) {
    if (id >= pins.length) return { ok: false, problem: `Navigation button ${id + 1} does not exist.` }
  }
  return { ok: true }
}

// A config only needs to be JSON here; the unit skips a config it cannot read.
function checkConfigJson(text) {
  try {
    const data = JSON.parse(text)
    if (data === null || typeof data !== 'object' || Array.isArray(data)) {
      return { ok: false, problem: 'A config file must be a JSON object.' }
    }
    return { ok: true }
  } catch (err) {
    return { ok: false, problem: `This is not valid JSON (${err.message}).` }
  }
}

module.exports = { checkDeviceJson, checkConfigJson }
