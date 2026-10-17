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
