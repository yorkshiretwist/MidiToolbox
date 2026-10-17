'use strict'
// Electron main process: one window that shows the Editor, the Designer or the Device tab.
// The Editor and Designer are the same built files as the GitHub Pages site, served from the
// app:// protocol; the Device tab is device-ui/, which is the only page given access to the unit.

const { app, BrowserWindow, Menu, dialog, ipcMain, net, protocol, session, shell } = require('electron')
const fs = require('node:fs')
const path = require('node:path')
const { resolveAppPath, mimeTypeFor } = require('./appPaths')
const { createSettings } = require('./settings')
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

function buildMenu() {
  const isMac = process.platform === 'darwin'
  const template = [
    ...(isMac ? [{ role: 'appMenu' }] : []),
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

function createWindow() {
  win = new BrowserWindow({
    width: 1280,
    height: 860,
    minWidth: 760,
    minHeight: 520,
    title: 'MIDI Toolbox',
    backgroundColor: '#1e1e1e',
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

  show(process.env.MIDI_TOOLBOX_START_VIEW || 'editor')
  win.on('closed', () => {
    win = null
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
    serveApp()

    // The Editor and Designer save and open files with the File System Access API when it is there.
    session.defaultSession.setPermissionRequestHandler((_webContents, permission, callback, details) => {
      const fromApp = (details.requestingUrl || '').startsWith(`${ORIGIN}/`)
      callback(fromApp && ['fileSystem', 'clipboard-sanitized-write'].includes(permission))
    })

    unit = registerUnitHandlers({
      ipcMain,
      dialog,
      getWindow: () => win,
      dirs,
      settings: createSettings(path.join(app.getPath('userData'), 'settings.json')),
      app: { version: app.getVersion(), isPackaged: app.isPackaged },
    })
    buildMenu()
    createWindow()
  })

  app.on('window-all-closed', async () => {
    if (unit) await unit.disconnect()
    app.quit()
  })
}
