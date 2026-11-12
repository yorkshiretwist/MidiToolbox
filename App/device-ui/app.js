'use strict'
// The Device tab. Everything that touches the unit goes through window.unit (see electron/preload.js).

const $ = (id) => document.getElementById(id)

let connection = null // what the main process last told us about the unit
let busy = false // an operation is running, so buttons are disabled
let flashing = false // a firmware update is in progress
let flashStep = null // 'prepare' | 'program' | 'restart' while an update runs
let flashResult = null // { ok, message } for the last update attempt
let flashTimer = null
let connectError = false // the last Connect failed
let bundled = null // the firmware that ships with the app, if any
let loader = null

// ---- small helpers -------------------------------------------------------------------------

function log(message, isError = false) {
  const line = document.createElement('div')
  line.textContent = `${new Date().toLocaleTimeString()}  ${message}`
  if (isError) line.className = 'error'
  $('log').appendChild(line)
  $('log').scrollTop = $('log').scrollHeight
  $('clear-log').hidden = false
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

function el(tag, text, className) {
  const node = document.createElement(tag)
  if (text !== undefined) node.textContent = text
  if (className) node.className = className
  return node
}

// A modal question. `body` fills the dialog's body element. Resolves true if the user confirms.
function confirmDialog({ title, body, confirm, danger = false }) {
  const dialog = $('dialog')
  $('dialog-title').textContent = title
  const box = $('dialog-body')
  box.replaceChildren()
  body(box)
  const ok = $('dialog-ok')
  ok.textContent = confirm
  ok.className = `btn ${danger ? 'btn--danger' : 'btn--blue'}`
  return new Promise((resolve) => {
    dialog.returnValue = 'cancel'
    dialog.addEventListener('close', () => resolve(dialog.returnValue === 'ok'), { once: true })
    dialog.showModal()
    $('dialog-cancel').focus()
  })
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
  $('clear-log').hidden = $('log').childElementCount === 0

  for (const id of ['upload-device', 'upload-configs', 'reload-files', 'restart']) $(id).disabled = locked

  renderHero(info)
  renderDetails(info)
  renderFirmware(locked)
}

function renderHero(info) {
  const hero = $('hero')
  const set = (state, title, text) => {
    hero.dataset.state = state
    $('hero-title').textContent = title
    $('hero-text').textContent = text
  }
  const chips = $('chips')
  chips.replaceChildren()
  chips.hidden = true

  if (flashing) {
    set('busy', 'Updating the firmware...', 'Keep the USB cable plugged in. The steps are shown in the Firmware section below.')
  } else if (busy && !connection) {
    set('busy', 'Working...', 'Talking to the unit. This should only take a moment.')
  } else if (connection) {
    if (info?.safeMode) set('warn', 'Connected, in safe mode', 'The unit is running, but it has no valid device.json yet.')
    else set('connected', 'Unit connected', `${info ? info.device : 'MIDI Toolbox'} on ${connection.path}. Everything is working.`)
    const add = (name, value, bad = false) => {
      const wrap = el('div', undefined, bad ? 'chips__bad' : '')
      wrap.append(el('dt', name), el('dd', value))
      chips.append(wrap)
    }
    if (info) {
      add('Firmware', info.firmware)
      add('Buttons', String(info.buttons))
      add('SD card', info.sd ? 'OK' : 'Missing', !info.sd)
    } else {
      add('Firmware', 'Older')
      add('SD card', connection.sd ? 'OK' : 'Missing', !connection.sd)
    }
    chips.hidden = false
  } else if (connectError) {
    set('error', 'Could not connect', 'Check the USB cable (it must carry data, not just power) and that no other program is using the unit. Details are in the log.')
  } else if ($('ports').dataset.count === '0') {
    set('idle', 'No unit connected', 'Plug the unit in with a USB cable. It will appear on the right.')
  } else {
    set('idle', 'No unit connected', "Choose the unit's port on the right and press Connect.")
  }
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
  for (const [name, value] of rows) list.append(el('dt', name), el('dd', value))
}

function renderFirmware(locked) {
  const text = $('firmware-status')
  const badge = $('firmware-badge')
  const status = connection?.firmware?.status
  badge.hidden = true
  const mark = (label, kind) => {
    badge.textContent = label
    badge.className = `badge badge--${kind}`
    badge.hidden = false
  }
  if (flashing) {
    text.textContent = 'Installing the firmware. Follow the steps below.'
  } else if (!bundled) {
    text.textContent = 'This copy of the app has no firmware inside it. You can still install a .hex file you have, under Advanced.'
  } else if (!connection) {
    text.textContent = `This app includes firmware version ${bundled.version}. Connect a unit to see whether it needs updating.`
  } else if (status === 'older') {
    text.textContent = `A newer version is available. The unit has ${connection.info.firmware}; this app includes ${bundled.version}.`
    mark('Update available', 'update')
  } else if (status === 'same') {
    text.textContent = `The unit already has the firmware this app includes (version ${bundled.version}). There is nothing you need to do.`
    mark('Up to date', 'ok')
  } else if (status === 'newer') {
    text.textContent = `The unit has newer firmware (${connection.info.firmware}) than this app includes (${bundled.version}). You do not need to install anything.`
    mark('Newer than this app', 'warn')
  } else {
    text.textContent = `This app includes firmware version ${bundled.version}. The unit's version is not known, so installing it is a safe way to bring the unit up to date.`
  }

  const flash = $('flash-bundled')
  const upToDate = status === 'same' || status === 'newer'
  flash.textContent = !bundled
    ? 'No firmware included'
    : upToDate
      ? `Reinstall version ${bundled.version}`
      : status === 'older'
        ? `Update to version ${bundled.version}`
        : `Install version ${bundled.version}`
  flash.disabled = locked || !bundled || !loader
  flash.classList.toggle('btn--blue', !upToDate)
  flash.classList.toggle('btn--quiet', upToDate)
  flash.hidden = flashing
  $('flash-other').disabled = locked || !loader
  $('cancel-flash').hidden = !flashing

  // the step list while an update runs
  $('flash-steps').hidden = !flashing
  const order = ['prepare', 'program', 'restart']
  const now = order.indexOf(flashStep)
  for (const li of $('flash-steps').querySelectorAll('li')) {
    const at = order.indexOf(li.dataset.step)
    li.dataset.state = at < now ? 'done' : at === now ? 'active' : 'todo'
  }

  const result = $('flash-result')
  result.hidden = !flashResult || flashing
  if (flashResult) {
    result.textContent = flashResult.message
    result.className = `notice ${flashResult.ok ? 'notice--ok' : 'notice--error'}`
  }

  $('loader').textContent = loader
    ? loader.source === 'bundled'
      ? 'built into the app'
      : `${loader.path} (${loader.source === 'chosen' ? 'chosen by you' : 'found on this computer'})`
    : 'NOT FOUND - choose the teensy_loader_cli program, or use a copy of the app that includes it'
}

// ---- ports ----------------------------------------------------------------------------------

async function refreshPorts() {
  const ports = (await attempt(() => window.unit.listPorts(), { quiet: true })) || []
  const select = $('ports')
  const previous = select.value
  select.replaceChildren()
  select.dataset.count = String(ports.length)
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
  if (configs.length === 0) list.append(el('li', 'No config files yet.', 'empty'))
  for (const file of configs.sort((a, b) => a.name.localeCompare(b.name))) {
    const item = document.createElement('li')
    const download = el('button', 'Download', 'btn btn--quiet btn--small')
    download.onclick = () => working(() => window.unit.download('/configs', file.name))
    const remove = el('button', 'Delete', 'btn btn--danger btn--small')
    remove.onclick = async () => {
      const sure = await confirmDialog({
        title: `Delete ${file.name}?`,
        body: (box) => {
          box.append(el('p', `${file.name} will be removed from the unit's SD card. This cannot be undone.`))
          box.append(el('p', 'If you have a copy on your computer you can upload it again later.'))
        },
        confirm: 'Delete',
        danger: true,
      })
      if (!sure) return
      const done = await working(() => window.unit.remove('/configs', file.name))
      if (done) reloadFiles()
    }
    item.append(el('span', file.name, 'name'), el('span', formatBytes(file.size), 'size'), download, remove)
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
  connectError = false
  connection = next
  render()
  await reloadFiles()
}

async function connect() {
  const result = await working(() => window.unit.connect($('ports').value))
  connectError = !result
  if (result) {
    $('status').textContent = 'Connected.'
    await adopt(result)
  } else {
    $('status').textContent = 'Could not connect. See the log below.'
    render()
  }
}

async function disconnect() {
  await working(() => window.unit.disconnect())
  connection = null
  connectError = false
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
  const sure = await confirmDialog({
    title: source === 'bundled' ? `Install firmware ${bundled?.version ?? ''}?` : 'Install a different firmware file?',
    confirm: 'Install firmware',
    body: (box) => {
      box.append(el('p', 'This is what will happen:'))
      const steps = el('ol')
      steps.append(
        el('li', 'The unit restarts into its update mode. Its screen will go blank.'),
        el('li', source === 'bundled' ? 'The new firmware is written. This usually takes under a minute.' : 'You choose the .hex file, then it is written. This usually takes under a minute.'),
        el('li', 'The unit restarts by itself and this app reconnects to it.'),
      )
      box.append(steps)
      box.append(el('p', 'Your configs and device.json on the SD card are not changed.'))
      const warn = el('div', undefined, 'notice')
      warn.append(el('strong', 'Keep the USB cable plugged in'), ' until it is finished.')
      if (source !== 'bundled') warn.append(' Only use a .hex file built for the Teensy 4.1.')
      box.append(warn)
    },
  })
  if (!sure) return

  flashing = true
  flashStep = 'prepare'
  flashResult = null
  connection = null
  $('status').textContent = 'Updating the firmware. Keep the USB cable plugged in.'
  $('flash-hint').hidden = true
  clearTimeout(flashTimer)
  render()
  const result = await attempt(() => window.unit.flash(source))
  clearTimeout(flashTimer)
  flashing = false
  flashStep = null
  $('flash-hint').hidden = true
  if (result) {
    flashResult = {
      ok: true,
      message: `The firmware was updated${source === 'bundled' && bundled ? ` to version ${bundled.version}` : ''} and the unit is back. You can use it as normal.`,
    }
    $('status').textContent = 'Connected.'
    await adopt(result)
  } else {
    flashResult = {
      ok: false,
      message:
        'The update did not finish. Your files on the SD card are untouched. Check the log, make sure the USB cable is plugged in, then try again. If the unit looks stuck, press the PROGRAM button on the Teensy.',
    }
    $('status').textContent = 'Not connected. Press Refresh, then Connect.'
    render()
    refreshPorts()
  }
}

function describeFlashState({ state }) {
  clearTimeout(flashTimer)
  $('flash-hint').hidden = true
  if (state === 'programming') {
    flashStep = 'program'
    // if the unit does not enter update mode by itself, tell the user what to press
    flashTimer = setTimeout(() => ($('flash-hint').hidden = false), 8000)
  } else if (state === 'restarting') {
    flashStep = 'restart'
  }
  render()
}

async function refreshLoader() {
  loader = (await attempt(() => window.unit.loaderStatus(), { quiet: true })) ?? null
  if (!loader) $('advanced').open = true
  render()
}

// ---- help -----------------------------------------------------------------------------------

function renderHelp(app) {
  const body = $('help-body')
  body.replaceChildren()
  const add = (tag, text) => {
    const node = el(tag, text)
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
      if (command) item.append(el('pre', command))
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
$('clear-log').onclick = () => {
  $('log').replaceChildren()
  render()
}
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
