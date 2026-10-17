'use strict'
// Everything the Device tab can ask the app to do, as IPC handlers. This file owns the one
// connection to the unit. The renderer only ever sees plain results and error messages.

const fs = require('node:fs')
const path = require('node:path')
const serial = require('./serial')
const { Link } = require('./device')
const { checkDeviceJson, checkConfigJson } = require('./validate')
const { bundledFirmware, compareInstalled } = require('./firmware')
const { findLoader, runLoader } = require('./loader')

const CONFIG_DIR = '/configs'
const DEVICE_FILE = '/device.json'
// the characters the firmware accepts in a file name (see SerialProtocol.h)
const SAFE_FILE_NAME = /^[A-Za-z0-9 ._-]+$/

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms))

/**
 * @param {object} deps
 * @param {Electron.IpcMain} deps.ipcMain
 * @param {Electron.Dialog} deps.dialog
 * @param {() => Electron.BrowserWindow | null} deps.getWindow
 * @param {{ loaderDir: string, firmwareDir: string, linuxDir: string }} deps.dirs
 * @param {{ get(): object, set(changes: object): object }} deps.settings
 * @param {{ version: string, isPackaged: boolean }} deps.app
 */
function registerUnitHandlers({ ipcMain, dialog, getWindow, dirs, settings, app }) {
  let connection = null // { path, port, link, hello, info }
  let lastPath = null
  let flashJob = null

  const send = (channel, payload) => {
    const win = getWindow()
    if (win && !win.isDestroyed()) win.webContents.send(channel, payload)
  }
  const log = (message) => send('log', message)

  // Every handler returns { ok, value } or { ok: false, error } so errors cross the IPC boundary
  // as plain messages.
  function handle(channel, fn) {
    ipcMain.handle(channel, async (_event, ...args) => {
      try {
        return { ok: true, value: await fn(...args) }
      } catch (err) {
        return { ok: false, error: err && err.message ? err.message : String(err), cancelled: Boolean(err && err.cancelled) }
      }
    })
  }

  const requireLink = () => {
    if (!connection) throw new Error('Not connected to a unit')
    return connection.link
  }

  async function disconnect() {
    const old = connection
    connection = null
    if (old) await serial.close(old.port)
  }

  // ---- connecting ----

  async function readInfo(link) {
    try {
      return await link.info()
    } catch {
      return null // a unit running older firmware may not know INFO
    }
  }

  async function connect(portPath) {
    await disconnect()
    const port = await serial.open(portPath)
    const link = new Link(port)
    port.on('close', () => {
      if (connection && connection.port === port) {
        connection = null
        log('The unit was disconnected')
        send('disconnected')
      }
    })
    try {
      const hello = await link.hello()
      const info = await readInfo(link)
      connection = { path: portPath, port, link, hello, info }
      lastPath = portPath
      log(`Connected to ${portPath}`)
      return describeConnection()
    } catch (err) {
      await serial.close(port)
      throw err
    }
  }

  function describeConnection() {
    const { path: portPath, hello, info } = connection
    const bundled = bundledFirmware(dirs.firmwareDir)
    return {
      path: portPath,
      protocol: hello.version,
      sd: hello.sd,
      info,
      firmware: bundled
        ? { bundledVersion: bundled.version, status: compareInstalled(info?.firmware, bundled.version) }
        : null,
    }
  }

  // After a restart or a flash the unit reappears on a serial port (often a different one).
  // Waits for a Teensy to show up and connects to it.
  async function waitForUnit(timeoutMs, preferPath) {
    const deadline = Date.now() + timeoutMs
    await sleep(1500)
    let lastError = null
    while (Date.now() < deadline) {
      const teensys = (await serial.listPorts()).filter((p) => p.isTeensy)
      if (teensys.length > 0) {
        const pick = teensys.find((p) => p.path === preferPath) || teensys[0]
        try {
          return await connect(pick.path)
        } catch (err) {
          lastError = err // the port can appear before the unit finishes starting
        }
      }
      await sleep(600)
    }
    throw new Error(
      `The unit did not come back within ${Math.round(timeoutMs / 1000)} seconds` +
        (lastError ? ` (${lastError.message})` : '') +
        '. Check the USB cable, then press Refresh and Connect.',
    )
  }

  // ---- restarting ----

  // A restart is refused while buttons are active (it would leave their notes stuck on the
  // synth). Ask the player; if they agree, run the command again with `force`, which releases
  // the buttons first.
  async function restartCommand(run) {
    try {
      await run(false)
    } catch (err) {
      if (!err.busy) throw err
      const { response } = await dialog.showMessageBox(getWindow(), {
        type: 'question',
        buttons: ['Release and restart', 'Cancel'],
        defaultId: 1,
        cancelId: 1,
        message: 'Buttons are active on the unit',
        detail: 'Restarting now would leave their notes stuck on the synth. Release them (sending the note-offs) and restart?',
      })
      if (response !== 0) throw Object.assign(new Error('Cancelled'), { cancelled: true })
      await run(true)
    }
  }

  // ---- handlers ----

  handle('ports:list', () => serial.listPorts())

  handle('unit:connect', (portPath) => connect(portPath))

  handle('unit:disconnect', async () => {
    await disconnect()
    log('Disconnected')
  })

  handle('unit:refresh', async () => {
    requireLink()
    connection.info = await readInfo(connection.link)
    return describeConnection()
  })

  handle('files:list', (dir) => requireLink().list(dir))

  handle('files:download', async (dir, name) => {
    const devicePath = dir === '/' ? `/${name}` : `${dir}/${name}`
    const data = await requireLink().get(devicePath)
    const { canceled, filePath } = await dialog.showSaveDialog(getWindow(), { defaultPath: name })
    if (canceled || !filePath) return false
    fs.writeFileSync(filePath, data)
    log(`Saved ${name} (${data.length} bytes) to ${filePath}`)
    return true
  })

  handle('files:delete', async (dir, name) => {
    const devicePath = dir === '/' ? `/${name}` : `${dir}/${name}`
    const { response } = await dialog.showMessageBox(getWindow(), {
      type: 'warning',
      buttons: ['Delete', 'Cancel'],
      defaultId: 1,
      cancelId: 1,
      message: `Delete ${name} from the unit?`,
      detail: 'This cannot be undone.',
    })
    if (response !== 0) return false
    await requireLink().delete(devicePath)
    log(`Deleted ${devicePath}`)
    return true
  })

  // kind is 'device' (the unit's wiring file, always stored as /device.json) or 'config'
  // (any number of performance configs, stored in /configs under their own names).
  handle('files:upload', async (kind) => {
    const link = requireLink()
    const isDevice = kind === 'device'
    const { canceled, filePaths } = await dialog.showOpenDialog(getWindow(), {
      title: isDevice ? 'Choose the unit\'s device.json' : 'Choose config files',
      properties: isDevice ? ['openFile'] : ['openFile', 'multiSelections'],
      filters: [{ name: 'JSON', extensions: ['json'] }, { name: 'All files', extensions: ['*'] }],
    })
    if (canceled || filePaths.length === 0) return { uploaded: [], skipped: [] }

    const uploaded = []
    const skipped = []
    for (const file of filePaths) {
      const name = isDevice ? 'device.json' : path.basename(file)
      const shown = path.basename(file)
      const text = fs.readFileSync(file, 'utf8')
      const verdict = isDevice ? checkDeviceJson(text) : checkConfigJson(text)
      if (!verdict.ok) {
        skipped.push({ name: shown, problem: verdict.problem })
        continue
      }
      if (!SAFE_FILE_NAME.test(name) || name.length > 48) {
        skipped.push({
          name: shown,
          problem: 'The unit only accepts file names made of letters, numbers, spaces and . _ - (up to 48 characters).',
        })
        continue
      }
      const devicePath = isDevice ? DEVICE_FILE : `${CONFIG_DIR}/${name}`
      const started = Date.now()
      await link.put(devicePath, Buffer.from(text, 'utf8'))
      log(`Uploaded ${shown} to ${devicePath} (${Buffer.byteLength(text)} bytes, ${Date.now() - started} ms)`)
      uploaded.push(name)
    }
    return { uploaded, skipped }
  })

  handle('unit:restart', async () => {
    const link = requireLink()
    const wasAt = connection.path
    await restartCommand((force) => link.reboot(force))
    log('Restarting the unit...')
    await disconnect()
    send('disconnected')
    return waitForUnit(30000, wasAt)
  })

  // ---- firmware ----

  handle('firmware:bundled', () => bundledFirmware(dirs.firmwareDir))

  handle('loader:status', () => {
    const found = findLoader({ chosenPath: settings.get().loaderPath, loaderDir: dirs.loaderDir })
    return found
  })

  handle('loader:choose', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog(getWindow(), {
      title: 'Choose the teensy_loader_cli program',
      properties: ['openFile'],
    })
    if (canceled || filePaths.length === 0) return null
    settings.set({ loaderPath: filePaths[0] })
    return findLoader({ chosenPath: filePaths[0], loaderDir: dirs.loaderDir })
  })

  // source: 'bundled' (the firmware inside the app) or 'choose' (ask for a .hex)
  handle('firmware:flash', async (source) => {
    if (flashJob) throw new Error('A firmware update is already running')

    let hex
    if (source === 'bundled') {
      const bundled = bundledFirmware(dirs.firmwareDir)
      if (!bundled) throw new Error('This copy of the app has no firmware inside it. Use "Install a different .hex" instead.')
      hex = bundled.file
    } else {
      const { canceled, filePaths } = await dialog.showOpenDialog(getWindow(), {
        title: 'Choose a firmware file',
        properties: ['openFile'],
        filters: [{ name: 'Firmware (.hex)', extensions: ['hex'] }],
      })
      if (canceled || filePaths.length === 0) return null
      hex = filePaths[0]
    }

    const loader = findLoader({ chosenPath: settings.get().loaderPath, loaderDir: dirs.loaderDir })
    if (!loader) {
      throw new Error(
        'The Teensy loader program (teensy_loader_cli) was not found. This copy of the app does not include it; ' +
          'use "Choose loader..." to point to one.',
      )
    }

    const portHint = connection ? connection.path : lastPath
    flashJob = { cancel() {} }
    try {
      // 1. ask the unit to enter its bootloader, or failing that, nudge the port
      if (connection) {
        const link = connection.link
        try {
          await restartCommand((force) => link.bootloader(force))
          log('The unit is entering its bootloader')
        } catch (err) {
          if (err.cancelled) return null
          log(`The unit could not be asked to enter its bootloader (${err.message}); trying the other way`)
          try {
            await serial.resetViaBaud(connection.path)
          } catch {
            // fall through: the loader will wait for the PROGRAM button
          }
        }
        await disconnect()
        send('disconnected')
      } else if (portHint) {
        try {
          await serial.resetViaBaud(portHint)
          log(`Asked ${portHint} to restart into its bootloader`)
        } catch {
          // not available; the loader will wait for the PROGRAM button
        }
      }

      // 2. program it
      send('flash-state', { state: 'programming' })
      log(`Programming ${path.basename(hex)}. If nothing happens within a few seconds, press the PROGRAM button on the Teensy.`)
      const job = runLoader(loader.path, hex, (text) => text.trim() && log(text.trimEnd()))
      flashJob = job
      await job.done

      // 3. wait for the new firmware to start
      send('flash-state', { state: 'restarting' })
      log('Programming finished; waiting for the unit to restart...')
      const result = await waitForUnit(40000, portHint)
      log('Firmware updated')
      return result
    } finally {
      flashJob = null
      send('flash-state', { state: 'idle' })
    }
  })

  handle('firmware:cancel', () => {
    if (flashJob && flashJob.cancel) flashJob.cancel()
  })

  // ---- app ----

  handle('app:info', () => ({
    version: app.version,
    platform: process.platform,
    isPackaged: app.isPackaged,
    rulesFile: path.join(dirs.linuxDir, '00-teensy.rules'),
  }))

  return { disconnect }
}

module.exports = { registerUnitHandlers }
