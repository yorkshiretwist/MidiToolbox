'use strict'
// Finding and running teensy_loader_cli, the open-source command-line programmer for Teensy boards.
// (Source: App/vendor/teensy_loader_cli, built per system by scripts/build-loader.mjs.)

const fs = require('node:fs')
const path = require('node:path')
const { spawn } = require('node:child_process')

const EXECUTABLE = process.platform === 'win32' ? 'teensy_loader_cli.exe' : 'teensy_loader_cli'

// Where the loader is, in order of preference: a path the user chose, the one built into the app,
// then whatever is on the PATH. Returns { path, source } or null when none can be found.
function findLoader({ chosenPath, loaderDir, env = process.env, exists = fs.existsSync } = {}) {
  if (chosenPath && exists(chosenPath)) return { path: chosenPath, source: 'chosen' }
  if (loaderDir) {
    const bundled = path.join(loaderDir, EXECUTABLE)
    if (exists(bundled)) return { path: bundled, source: 'bundled' }
  }
  for (const dir of (env.PATH || '').split(path.delimiter)) {
    if (dir && exists(path.join(dir, EXECUTABLE))) return { path: path.join(dir, EXECUTABLE), source: 'path' }
  }
  return null
}

// -w waits for the board to enter its bootloader (so the player can also press PROGRAM),
// -v is verbose. The board restarts itself when programming finishes.
function loaderArgs(hexPath) {
  return ['--mcu=TEENSY41', '-w', '-v', hexPath]
}

/**
 * Runs the loader. Returns { done, cancel }: `done` resolves when it exits 0 and rejects otherwise;
 * `onOutput(text)` is called with everything it prints.
 */
function runLoader(loaderPath, hexPath, onOutput) {
  const child = spawn(loaderPath, loaderArgs(hexPath))
  let cancelled = false
  const done = new Promise((resolve, reject) => {
    child.stdout.on('data', (d) => onOutput(d.toString()))
    child.stderr.on('data', (d) => onOutput(d.toString()))
    child.on('error', (err) => reject(new Error(`Could not run the Teensy loader (${loaderPath}): ${err.message}`)))
    child.on('close', (code) => {
      if (cancelled) reject(Object.assign(new Error('Cancelled'), { cancelled: true }))
      else if (code === 0) resolve()
      else reject(new Error(`The Teensy loader stopped with code ${code}`))
    })
  })
  return {
    done,
    cancel() {
      cancelled = true
      child.kill()
    },
  }
}

module.exports = { EXECUTABLE, findLoader, loaderArgs, runLoader }
