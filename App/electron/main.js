'use strict'
// Electron main process: one window that shows the Editor, the Designer or the Device tab.
// The Editor and Designer are the same built files as the GitHub Pages site, served from the
// app:// protocol; the Device tab is device-ui/, which is the only page given access to the unit.

const { app, BrowserWindow, Menu, dialog, ipcMain, net, protocol, session, shell } = require('electron')
const fs = require('node:fs')
const path = require('node:path')
const { resolveAppPath, mimeTypeFor } = require('./appPaths')
const { createSettings } = require('./settings')
const { createFiles, kindForUrl } = require('./files')
const { registerUnitHandlers } = require('./unit')

const ORIGIN = 'app://local'

// Packaged, the big folders sit next to the app in "resources"; in development they are in App/.
const root = app.isPackaged ? process.resourcesPath : path.join(__dirname, '..')
const dirs = {
  webDir: path.join(root, 'web'),
  firmwareDir: path.join(root, 'firmware'),
  loaderDir: app.isPackaged ? path.join(process.resourcesPath, 'loader') : path.join(root, 'resources', 'loader'),
  linuxDir: app.isPackaged ? path.join(process.resourcesPath, 'linux') : path.join(root, 'resources', 'linux'),
  deviceUiDir: path.join(__dirname, '..', 'device-ui'),
}

// "standard" + "secure" make app:// behave like a normal https site (relative links, storage,
// module scripts), which the Vite-built Editor and Designer expect.
protocol.registerSchemesAsPrivileged([
  { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true } },
])

let win = null
let unit = null
let files = null

function serveApp() {
  protocol.handle('app', async (request) => {
    const { pathname } = new URL(request.url)
    const file = resolveAppPath(pathname, dirs.webDir, dirs.deviceUiDir)
    if (!file) return new Response('Not found', { status: 404 })
    try {
      // fs (not net.fetch of a file: URL) so files inside app.asar can be read too
      const body = await fs.promises.readFile(file)
      return new Response(body, { headers: { 'content-type': mimeTypeFor(file) } })
    } catch {
      return new Response('Not found', { status: 404 })
    }
  })
}

const VIEWS = {
  editor: { label: 'Editor', url: `${ORIGIN}/editor/`, key: 'CmdOrCtrl+1' },
  designer: { label: 'Designer', url: `${ORIGIN}/designer/`, key: 'CmdOrCtrl+2' },
  device: { label: 'Device', url: `${ORIGIN}/_device/`, key: 'CmdOrCtrl+3' },
}

function show(view) {
  if (win && !win.isDestroyed()) win.loadURL(VIEWS[view].url)
}

// Which page is showing decides what the File menu works on: a config in the Editor, a device
// design in the Designer. The Device tab has no files, so those items are greyed out there.
function currentKind() {
  return win && !win.isDestroyed() ? kindForUrl(win.webContents.getURL(), ORIGIN) : ''
}

function sendFileCommand(command) {
  if (win && !win.isDestroyed()) win.webContents.send('file-command', command)
}

function fileMenu() {
  const kind = currentKind()
  const noun = kind === 'designer' ? 'design' : 'config'
  const recent = kind && files ? files.recent(kind) : []
  return {
    label: 'File',
    submenu: [
      { label: `Open ${noun}...`, accelerator: 'CmdOrCtrl+O', enabled: !!kind, click: () => sendFileCommand({ type: 'open' }) },
      {
        label: 'Open recent',
        enabled: recent.length > 0,
        submenu: [
          ...recent.map((file) => ({
            label: path.basename(file),
            toolTip: file, // the full path, on hover
            click: () => sendFileCommand({ type: 'openPath', path: file }),
          })),
          { type: 'separator' },
          { label: 'Clear recent files', click: () => files && files.clearRecent(kind) },
        ],
      },
      { type: 'separator' },
      { label: 'Save', accelerator: 'CmdOrCtrl+S', enabled: !!kind, click: () => sendFileCommand({ type: 'save' }) },
      { label: 'Save as...', accelerator: 'CmdOrCtrl+Shift+S', enabled: !!kind, click: () => sendFileCommand({ type: 'saveAs' }) },
      { type: 'separator' },
      { role: process.platform === 'darwin' ? 'close' : 'quit' },
    ],
  }
}

function buildMenu() {
  const isMac = process.platform === 'darwin'
  const template = [
    ...(isMac ? [{ role: 'appMenu' }] : []),
    fileMenu(),
    {
      label: 'Go',
      submenu: Object.entries(VIEWS).map(([id, view]) => ({
        label: view.label,
        accelerator: view.key,
        click: () => show(id),
      })),
    },
    { role: 'editMenu' },
    { role: 'viewMenu' },
    { role: 'windowMenu' },
    {
      role: 'help',
      submenu: [
        {
          label: 'Project page',
          click: () => shell.openExternal('https://github.com/yorkshiretwist/MidiToolbox'),
        },
        { label: `Version ${app.getVersion()}`, enabled: false },
      ],
    },
  ]
  Menu.setApplicationMenu(Menu.buildFromTemplate(template))
}

// A small window that appears at once while the app starts up, so the player is not left staring at
// nothing. It closes when the main window is ready (see createWindow).
let splash = null
function showSplash() {
  splash = new BrowserWindow({
    width: 480,
    height: 280,
    frame: false,
    resizable: false,
    movable: true,
    minimizable: false,
    maximizable: false,
    fullscreenable: false,
    skipTaskbar: true,
    show: false,
    backgroundColor: '#1a1a1a',
    title: 'MIDI Toolbox',
    icon: path.join(app.isPackaged ? process.resourcesPath : path.join(__dirname, '..', 'resources'), 'icon.png'),
    webPreferences: { contextIsolation: true, nodeIntegration: false, sandbox: true },
  })
  splash.once('ready-to-show', () => {
    if (splash) splash.show()
  })
  splash.on('closed', () => {
    splash = null
  })
  splash.loadFile(path.join(__dirname, 'splash.html'))
}

function splashText(text) {
  if (splash && !splash.isDestroyed()) {
    splash.webContents.executeJavaScript(`document.getElementById("st").textContent=${JSON.stringify(text)}`).catch(() => {})
  }
}

function closeSplash() {
  if (splash && !splash.isDestroyed()) splash.close()
}

function createWindow() {
  win = new BrowserWindow({
    show: false,
    width: 1280,
    height: 860,
    minWidth: 760,
    minHeight: 520,
    title: 'MIDI Toolbox',
    backgroundColor: '#1e1e1e',
    icon: path.join(app.isPackaged ? process.resourcesPath : path.join(__dirname, '..', 'resources'), 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  })

  // Stay inside the app. Links to the web open in the player's own browser instead.
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (url.startsWith('https://')) shell.openExternal(url)
    return { action: 'deny' }
  })
  win.webContents.on('will-navigate', (event, url) => {
    if (!url.startsWith(`${ORIGIN}/`)) {
      event.preventDefault()
      if (url.startsWith('https://')) shell.openExternal(url)
    }
  })

  // the File menu follows the page
  win.webContents.on('did-navigate', buildMenu)

  win.webContents.on('did-fail-load', (_event, code, description, url) => {
    console.error(`[app] failed to load ${url}: ${description} (${code})`)
  })

  // Smoke test for development and CI: load the page, report, quit.
  if (process.env.MIDI_TOOLBOX_SMOKE_TEST) {
    win.webContents.on('did-finish-load', async () => {
      const title = await win.webContents.executeJavaScript('document.title')
      const root = await win.webContents.executeJavaScript('document.getElementById("app")?.children.length ?? -1')
      const unitApi = await win.webContents.executeJavaScript('typeof window.unit')
      const picker = await win.webContents.executeJavaScript('typeof window.showOpenFilePicker + "/" + typeof window.showSaveFilePicker')
      const line = `[smoke] loaded ${win.webContents.getURL()} title="${title}" appChildren=${root} unitApi=${unitApi} pickers=${picker} packaged=${app.isPackaged}`
      console.log(line)
      // a packaged Windows app has no console, so the result can also go to a file
      if (process.env.MIDI_TOOLBOX_SMOKE_OUT) fs.appendFileSync(process.env.MIDI_TOOLBOX_SMOKE_OUT, line + '\n')
      app.quit()
    })
  }

  // Show the main window once its first page has loaded, and close the splash. After 20 seconds it is
  // shown regardless, so a page that never finishes loading can't leave the app hidden.
  let revealed = false
  const reveal = () => {
    if (revealed || !win || win.isDestroyed()) return
    revealed = true
    win.show()
    win.focus()
    closeSplash()
  }
  win.webContents.once('did-finish-load', reveal)
  setTimeout(reveal, 20000)

  show(process.env.MIDI_TOOLBOX_START_VIEW || 'editor')
  win.on('closed', () => {
    win = null
    closeSplash()
  })
}

// One copy at a time: a second one would fight over the serial port.
if (!app.requestSingleInstanceLock()) {
  app.quit()
} else {
  app.on('second-instance', () => {
    if (win) {
      if (win.isMinimized()) win.restore()
      win.focus()
    }
  })

  app.whenReady().then(() => {
    if (!process.env.MIDI_TOOLBOX_SMOKE_TEST) showSplash()
    serveApp()

    // The Editor and Designer save and open files with the File System Access API when it is there.
    session.defaultSession.setPermissionRequestHandler((_webContents, permission, callback, details) => {
      const fromApp = (details.requestingUrl || '').startsWith(`${ORIGIN}/`)
      callback(fromApp && ['fileSystem', 'clipboard-sanitized-write'].includes(permission))
    })

    const settings = createSettings(path.join(app.getPath('userData'), 'settings.json'))
    files = createFiles({ ipcMain, dialog, getWindow: () => win, settings, origin: ORIGIN, onRecentChanged: buildMenu })
    unit = registerUnitHandlers({
      ipcMain,
      dialog,
      getWindow: () => win,
      dirs,
      settings,
      app: { version: app.getVersion(), isPackaged: app.isPackaged },
    })
    buildMenu()
    splashText('Loading the Editor...')
    createWindow()
  })

  app.on('window-all-closed', async () => {
    if (unit) await unit.disconnect()
    app.quit()
  })
}
