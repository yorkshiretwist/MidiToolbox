'use strict'
// Thin wrappers over the serialport package: listing, opening and the 134-baud reset.

const { SerialPort } = require('serialport')

const PJRC_VENDOR_ID = '16c0'

async function listPorts() {
  const ports = await SerialPort.list()
  return ports
    .map((p) => ({
      path: p.path,
      manufacturer: p.manufacturer || '',
      serialNumber: p.serialNumber || '',
      vendorId: p.vendorId || '',
      productId: p.productId || '',
      isTeensy: (p.vendorId || '').toLowerCase() === PJRC_VENDOR_ID,
    }))
    .sort((a, b) => Number(b.isTeensy) - Number(a.isTeensy) || a.path.localeCompare(b.path))
}

// Turns the operating system's terse "can't open port" errors into something a player can act on.
function describeOpenError(err, portPath) {
  const text = (err && err.message) || String(err)
  if (/access denied|resource busy|EBUSY|in use/i.test(text)) {
    return Object.assign(
      new Error(
        `${portPath} is in use by another program. Close the Arduino Serial Monitor, the Teensy Loader ` +
          'and anything else using the unit, then try again.',
      ),
      { code: 'IN_USE' },
    )
  }
  if (/EACCES|permission denied/i.test(text)) {
    return Object.assign(
      new Error(
        `You do not have permission to use ${portPath}. On Linux this needs a one-time setup: ` +
          'see "Help with connecting" below.',
      ),
      { code: 'PERMISSION' },
    )
  }
  return err
}

function open(path) {
  const port = new SerialPort({ path, baudRate: 115200, autoOpen: false })
  return new Promise((resolve, reject) => {
    port.open((err) => (err ? reject(describeOpenError(err, path)) : resolve(port)))
  })
}

function close(port) {
  return new Promise((resolve) => {
    if (!port || !port.isOpen) return resolve()
    port.close(() => resolve())
  })
}

// Teensyduino reboots into the bootloader when the host opens the port at 134 baud.
async function resetViaBaud(path) {
  const port = new SerialPort({ path, baudRate: 134, autoOpen: false })
  await new Promise((resolve, reject) => port.open((err) => (err ? reject(err) : resolve())))
  await new Promise((resolve) => setTimeout(resolve, 250))
  await close(port)
}

module.exports = { listPorts, open, close, resetViaBaud, describeOpenError }
