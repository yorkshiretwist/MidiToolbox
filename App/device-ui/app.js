'use strict'
// The Device tab. Everything that touches the unit goes through window.unit (see electron/preload.js).

const $ = (id) => document.getElementById(id)

let connection = null // what the main process last told us about the unit
let busy = false // an operation is running, so buttons are disabled
let flashing = false // a firmware update is in progress
let bundled = null // the firmware that ships with the app, if any
let loader = null

// ---- small helpers -------------------------------------------------------------------------

function log(message, isError = false) {
  const line = document.createElement('div')
  line.textContent = `${new Date().toLocaleTimeString()}  ${message}`
  if (isError) line.className = 'error'
  $('log').appendChild(line)
  $('log').scrollTop = $('log').scrollHeight
}

// Runs an action; shows any failure in the log instead of throwing. Returns undefined on failure.
async function attempt(action, { quiet = false } = {}) {
  try {
    return await action()
  } catch (err) {
    if (!err.cancelled && !quiet) log(err.message, true)
    return undefined
  }
}

// Runs an action with the buttons disabled while it works.
async function working(action) {
  busy = true
  render()
  try {
    return await attempt(action)
  } finally {
    busy = false
    render()
  }
}

function formatBytes(n) {
  return n < 1024 ? `${n} bytes` : `${(n / 1024).toFixed(1)} KB`
}

function describeWiring(source) {
  switch (source) {
    case 'file':
      return 'device.json on the SD card'
    case 'built-in':
      return 'built into the firmware (there is no device.json)'
    case 'none':
      return 'none (safe mode)'
    default:
      return 'unknown'
  }
}

// ---- rendering ------------------------------------------------------------------------------

function render() {
  const connected = connection !== null
  const info = connection?.info ?? null
  const locked = busy || flashing

  $('connect').hidden = connected
  $('disconnect').hidden = !connected
  $('connect').disabled = locked || !$('ports').value
  $('disconnect').disabled = locked
  $('ports').disabled = connected || locked
  $('refresh').disabled = connected || locked

  $('files').hidden = !connected
  $('actions').hidden = !connected
  $('safe-banner').hidden = !(connected && info?.safeMode)

  for (const id of ['upload-device', 'upload-configs', 'reload-files', 'restart']) $(id).disabled = locked

  renderDetails(info)
  renderFirmware(locked)
}

function renderDetails(info) {
  const list = $('details')
  list.replaceChildren()
  list.hidden = connection === null
  if (!connection) return

  const rows = [['Port', connection.path]]
  if (info) {
    rows.push(['Unit', info.device])
    rows.push(['Firmware', info.firmware])
    rows.push(['Wiring from', describeWiring(info.deviceSource)])
    rows.push(['Mode', info.safeMode ? 'Safe mode' : 'Normal'])
    rows.push(['Buttons', String(info.buttons)])
    rows.push(['SD card', info.sd ? 'OK' : 'Missing'])
    if (info.buttonsActive) rows.push(['Buttons on', 'Yes (the app will ask before turning them off)'])
  } else {
    rows.push(['Firmware', 'Older than this app can describe (it does not report a version)'])
    rows.push(['SD card', connection.sd ? 'OK' : 'Missing'])
  }
  for (const [name, value] of rows) {
    const dt = document.createElement('dt')
    dt.textContent = name
    const dd = document.createElement('dd')
    dd.textContent = value
    list.append(dt, dd)
  }
}

function renderFirmware(locked) {
  const text = $('firmware-status')
  const status = connection?.firmware?.status
  if (!bundled) {
    text.textContent = 'This copy of the app has no firmware inside it. You can still install a .hex file you have.'
  } else if (!connection) {
    text.textContent = `This app includes firmware version ${bundled.version}. Connect a unit to compare.`
  } else if (status === 'older') {
    text.textContent = `An update is available: the unit has ${connection.info.firmware}, this app includes ${bundled.version}.`
  } else if (status === 'same') {
    text.textContent = `The unit is up to date (version ${bundled.version}).`
  } else if (status === 'newer') {
    text.textContent = `The unit has newer firmware (${connection.info.firmware}) than this app includes (${bundled.version}).`
  } else {
    text.textContent = `This app includes firmware version ${bundled.version}. The unit's version is not known.`
  }

  $('flash-bundled').disabled = locked || !bundled || !loader
  $('flash-bundled').classList.toggle('btn--blue', status !== 'same')
  $('flash-other').disabled = locked || !loader
  $('cancel-flash').hidden = !flashing

  $('loader').textContent = loader
    ? loader.source === 'bundled'
      ? 'built into the app'
      : `${loader.path} (${loader.source === 'chosen' ? 'chosen by you' : 'found on this computer'})`
    : 'NOT FOUND - choose the teensy_loader_cli program, or use a copy of the app that includes it'
}

function showProgress(message) {
  const box = $('flash-progress')
  box.hidden = !message
  box.textContent = message || ''
}

// ---- ports ----------------------------------------------------------------------------------

async function refreshPorts() {
  const ports = (await attempt(() => window.unit.listPorts(), { quiet: true })) || []
  const select = $('ports')
  const previous = select.value
  select.replaceChildren()
  if (ports.length === 0) {
    const none = document.createElement('option')
    none.value = ''
    none.textContent = 'No serial ports found - is the unit plugged in?'
    select.append(none)
  }
  for (const port of ports) {
    const option = document.createElement('option')
    option.value = port.path
    option.textContent = `${port.path}${port.isTeensy ? '  (Teensy)' : ''}${port.manufacturer ? '  -  ' + port.manufacturer : ''}`
    select.append(option)
  }
  if (previous && ports.some((p) => p.path === previous)) select.value = previous
  render()
}

// ---- files ----------------------------------------------------------------------------------

async function reloadFiles() {
  if (!connection) return
  const root = (await attempt(() => window.unit.list('/'))) || []
  const configs = (await attempt(() => window.unit.list('/configs'))) || []

  const device = root.find((f) => f.name === 'device.json')
  const status = $('device-file')
  status.textContent = device ? `device.json is on the unit (${formatBytes(device.size)})` : 'There is no device.json on the unit.'
  $('download-device').disabled = !device

  const list = $('configs')
  list.replaceChildren()
  if (configs.length === 0) {
    const empty = document.createElement('li')
    empty.className = 'empty'
    empty.textContent = 'No config files yet.'
    list.append(empty)
  }
  for (const file of configs.sort((a, b) => a.name.localeCompare(b.name))) {
    const item = document.createElement('li')
    const name = document.createElement('span')
    name.className = 'name'
    name.textContent = file.name
    const size = document.createElement('span')
    size.className = 'size'
    size.textContent = formatBytes(file.size)
    const download = document.createElement('button')
    download.className = 'btn btn--small'
    download.textContent = 'Download'
    download.onclick = () => working(() => window.unit.download('/configs', file.name))
    const remove = document.createElement('button')
    remove.className = 'btn btn--small btn--red'
    remove.textContent = 'Delete'
    remove.onclick = async () => {
      const done = await working(() => window.unit.remove('/configs', file.name))
      if (done) reloadFiles()
    }
    item.append(name, size, download, remove)
    list.append(item)
  }
}

async function upload(kind) {
  const result = await working(() => window.unit.upload(kind))
  if (!result) return
  for (const skipped of result.skipped) log(`Not sent: ${skipped.name}. ${skipped.problem}`, true)
  if (result.uploaded.length > 0 && kind === 'device') {
    log('device.json is on the unit. Restart the unit for it to take effect.')
  }
  await reloadFiles()
}

// ---- connecting -----------------------------------------------------------------------------

async function adopt(next) {
  connection = next
  render()
  await reloadFiles()
}

async function connect() {
  const result = await working(() => window.unit.connect($('ports').value))
  if (result) {
    $('status').textContent = 'Connected.'
    await adopt(result)
  } else {
    $('status').textContent = 'Could not connect. See the log below.'
  }
}

async function disconnect() {
  await working(() => window.unit.disconnect())
  connection = null
  $('status').textContent = 'Disconnected.'
  render()
  refreshPorts()
}

async function restart() {
  connection = null
  $('status').textContent = 'Restarting the unit...'
  const result = await working(() => window.unit.restart())
  if (result) {
    $('status').textContent = 'Connected.'
    log('The unit is back')
    await adopt(result)
  } else {
    $('status').textContent = 'The unit was not reconnected. Press Refresh, then Connect.'
    render()
    refreshPorts()
  }
}

// ---- firmware -------------------------------------------------------------------------------

async function flash(source) {
  flashing = true
  connection = null
  showProgress(null)
  render()
  const result = await attempt(() => window.unit.flash(source))
  flashing = false
  showProgress(null)
  if (result) {
    $('status').textContent = 'Connected.'
    await adopt(result)
  } else {
    $('status').textContent = 'Not connected. Press Refresh, then Connect.'
    render()
    refreshPorts()
  }
}

function describeFlashState({ state }) {
  if (state === 'programming') {
    showProgress('Programming the unit... If nothing happens within a few seconds, press the PROGRAM button on the Teensy.')
  } else if (state === 'restarting') {
    showProgress('Programming finished. Waiting for the unit to restart...')
  } else {
    showProgress(null)
  }
}

async function refreshLoader() {
  loader = (await attempt(() => window.unit.loaderStatus(), { quiet: true })) ?? null
  render()
}

// ---- help -----------------------------------------------------------------------------------

function renderHelp(app) {
  const body = $('help-body')
  body.replaceChildren()
  const add = (tag, text) => {
    const node = document.createElement(tag)
    node.textContent = text
    body.append(node)
    return node
  }

  add('p', 'The unit must be plugged in with a data USB cable (some cables only charge) and must not be open in another program, such as the Arduino Serial Monitor or the Teensy Loader.')

  if (app.platform === 'linux') {
    add('p', 'Linux needs a one-time setup so that you may use the unit without administrator rights. In a terminal:')
    const steps = document.createElement('ol')
    for (const [text, command] of [
      ['Copy the rules file that came with this app:', `sudo cp "${app.rulesFile}" /etc/udev/rules.d/00-teensy.rules`],
      ['Load it:', 'sudo udevadm control --reload-rules && sudo udevadm trigger'],
      ['Unplug the unit and plug it in again.', null],
    ]) {
      const item = document.createElement('li')
      item.append(text)
      if (command) {
        const pre = document.createElement('pre')
        pre.textContent = command
        item.append(pre)
      }
      steps.append(item)
    }
    body.append(steps)
    add('p', 'If your account is still refused, add it to the serial group (named "dialout" on most systems, "uucp" on Arch), then log out and back in.')
  } else if (app.platform === 'darwin') {
    add('p', 'This app is not signed with an Apple developer certificate, so the first time you open it macOS may refuse. Open System Settings > Privacy & Security, scroll down, and choose "Open Anyway" next to MIDI Toolbox.')
  } else {
    add('p', 'This app is not signed with a Windows publisher certificate, so Windows may show a "Windows protected your PC" warning the first time. Choose "More info", then "Run anyway".')
  }
}

// ---- start ----------------------------------------------------------------------------------

$('refresh').onclick = refreshPorts
$('ports').onchange = render
$('connect').onclick = connect
$('disconnect').onclick = disconnect
$('upload-device').onclick = () => upload('device')
$('upload-configs').onclick = () => upload('config')
$('reload-files').onclick = () => working(reloadFiles)
$('download-device').onclick = () => working(() => window.unit.download('/', 'device.json'))
$('restart').onclick = restart
$('flash-bundled').onclick = () => flash('bundled')
$('flash-other').onclick = () => flash('choose')
$('cancel-flash').onclick = () => attempt(() => window.unit.cancelFlash())
$('choose-loader').onclick = async () => {
  const chosen = await attempt(() => window.unit.chooseLoader())
  if (chosen) await refreshLoader()
}

window.unit.onLog((message) => log(message))
window.unit.onFlashState(describeFlashState)
window.unit.onDisconnected(() => {
  if (flashing || busy) return // the app is restarting the unit on purpose
  connection = null
  $('status').textContent = 'The unit was disconnected.'
  render()
  refreshPorts()
})

;(async () => {
  bundled = (await attempt(() => window.unit.bundledFirmware(), { quiet: true })) ?? null
  await refreshLoader()
  await refreshPorts()
  const app = await attempt(() => window.unit.appInfo(), { quiet: true })
  if (app) renderHelp(app)
  // while nothing is connected, notice a unit being plugged in
  setInterval(() => {
    if (!connection && !busy && !flashing) refreshPorts()
  }, 2500)
})()
