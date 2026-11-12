'use strict'
// The bridge between the Device tab and the main process. It exposes nothing on any other page:
// the Editor and Designer are given no way to reach the serial port.

const { contextBridge, ipcRenderer } = require('electron')

if (location.href.startsWith('app://local/_device/')) {
  // Each call resolves with the result or rejects with the error message the main process gave.
  const call = (channel, ...args) =>
    ipcRenderer.invoke(channel, ...args).then((reply) => {
      if (!reply.ok) throw Object.assign(new Error(reply.error), { cancelled: reply.cancelled })
      return reply.value
    })

  contextBridge.exposeInMainWorld('unit', {
    listPorts: () => call('ports:list'),
    connect: (port) => call('unit:connect', port),
    disconnect: () => call('unit:disconnect'),
    refresh: () => call('unit:refresh'),
    list: (dir) => call('files:list', dir),
    download: (dir, name) => call('files:download', dir, name),
    remove: (dir, name) => call('files:delete', dir, name),
    upload: (kind) => call('files:upload', kind),
    restart: () => call('unit:restart'),
    bundledFirmware: () => call('firmware:bundled'),
    flash: (source) => call('firmware:flash', source),
    cancelFlash: () => call('firmware:cancel'),
    loaderStatus: () => call('loader:status'),
    chooseLoader: () => call('loader:choose'),
    appInfo: () => call('app:info'),
    onLog: (fn) => ipcRenderer.on('log', (_event, message) => fn(message)),
    onDisconnected: (fn) => ipcRenderer.on('disconnected', () => fn()),
    onFlashState: (fn) => ipcRenderer.on('flash-state', (_event, state) => fn(state)),
  })
}

// The Editor and the Designer open and save files through the app, so the File menu and the recent
// files list work on the same files as the page's own buttons.
if (location.href.startsWith('app://local/editor/') || location.href.startsWith('app://local/designer/')) {
  const call = (channel, ...args) =>
    ipcRenderer.invoke(channel, ...args).then((reply) => {
      if (!reply.ok) throw new Error(reply.error)
      return reply.value
    })

  contextBridge.exposeInMainWorld('appFile', {
    /** Show the open dialog: { path, name, text }, or null when cancelled. */
    open: () => call('file:open'),
    /** Open a file from the recent list. */
    read: (file) => call('file:read', file),
    /** Save to `file`, or ask where when it is null: { path, name }, or null when cancelled. */
    write: (file, suggestedName, text) => call('file:write', file, suggestedName, text),
    /** File menu commands: { type: 'open' | 'save' | 'saveAs' } or { type: 'openPath', path }. */
    onCommand: (fn) => {
      ipcRenderer.removeAllListeners('file-command')
      ipcRenderer.on('file-command', (_event, command) => fn(command))
    },
  })
}
