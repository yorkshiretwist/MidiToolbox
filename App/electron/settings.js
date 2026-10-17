'use strict'
// The few things the app remembers between runs (today just a loader the user chose), kept in a
// small JSON file in the user's app-data folder.

const fs = require('node:fs')
const path = require('node:path')

function createSettings(file) {
  function read() {
    try {
      return JSON.parse(fs.readFileSync(file, 'utf8'))
    } catch {
      return {}
    }
  }
  return {
    get: read,
    set(changes) {
      const next = { ...read(), ...changes }
      fs.mkdirSync(path.dirname(file), { recursive: true })
      fs.writeFileSync(file, JSON.stringify(next, null, 2))
      return next
    },
  }
}

module.exports = { createSettings }
