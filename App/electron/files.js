'use strict'
// Opening and saving files for the Editor and the Designer, and the "recent files" lists behind the
// File menu. The pages never touch the disk themselves: they ask over IPC, and each page can only
// ask about its own kind of file ("editor" files are configs, "designer" files are device designs).

const fs = require('node:fs')
const path = require('node:path')

const MAX_RECENT = 10

const KINDS = {
  editor: { label: 'MIDI Toolbox config', defaultName: 'config.json' },
  designer: { label: 'MIDI Toolbox device design', defaultName: 'design.json' },
}

/** Which kind of file a page deals with, from its address ('' for the Device tab or anything else). */
function kindForUrl(url, origin) {
  if (typeof url !== 'string') return ''
  if (url.startsWith(`${origin}/editor/`)) return 'editor'
  if (url.startsWith(`${origin}/designer/`)) return 'designer'
  return ''
}

/** A recents list with `file` moved to the front, no repeats, at most MAX_RECENT long. */
function addToRecent(list, file) {
  return [file, ...list.filter((f) => f !== file)].slice(0, MAX_RECENT)
}

function createFiles({ ipcMain, dialog, getWindow, settings, origin, onRecentChanged }) {
  const recent = (kind) => {
    const all = settings.get().recent || {}
    return Array.isArray(all[kind]) ? all[kind].filter((p) => typeof p === 'string') : []
  }
  const setRecent = (kind, list) => {
    settings.set({ recent: { ...(settings.get().recent || {}), [kind]: list } })
    onRecentChanged()
  }

  function kindOf(event) {
    const kind = kindForUrl(event.senderFrame ? event.senderFrame.url : event.sender.getURL(), origin)
    if (!kind) throw new Error('Files can only be opened from the Editor or the Designer')
    return kind
  }

  const filters = (kind) => [
    { name: KINDS[kind].label, extensions: ['json'] },
    { name: 'All files', extensions: ['*'] },
  ]

  async function readInto(kind, file) {
    let text
    try {
      text = await fs.promises.readFile(file, 'utf8')
    } catch (err) {
      setRecent(kind, recent(kind).filter((f) => f !== file))
      throw new Error(`Could not open ${path.basename(file)}: ${err.code === 'ENOENT' ? 'the file no longer exists' : err.message}`)
    }
    setRecent(kind, addToRecent(recent(kind), file))
    return { path: file, name: path.basename(file), text }
  }

  // Every handler answers { ok, value } or { ok: false, error }, like the unit handlers.
  function handle(channel, fn) {
    ipcMain.handle(channel, async (event, ...args) => {
      try {
        return { ok: true, value: await fn(kindOf(event), ...args) }
      } catch (err) {
        return { ok: false, error: err && err.message ? err.message : String(err) }
      }
    })
  }

  // Show the open dialog; null when the user cancels.
  handle('file:open', async (kind) => {
    const result = await dialog.showOpenDialog(getWindow(), { filters: filters(kind), properties: ['openFile'] })
    if (result.canceled || !result.filePaths[0]) return null
    return readInto(kind, result.filePaths[0])
  })

  // Open a file from the recent list (or anywhere the page was told about).
  handle('file:read', async (kind, file) => {
    if (typeof file !== 'string' || !recent(kind).includes(file)) throw new Error('That file is not in the recent files list')
    return readInto(kind, file)
  })

  // Write to `file`, or ask where when it is null; null back when the user cancels the dialog.
  handle('file:write', async (kind, file, suggestedName, text) => {
    if (typeof text !== 'string') throw new Error('Nothing to save')
    let target = typeof file === 'string' ? file : null
    if (target && !recent(kind).includes(target)) target = null // only ever overwrite files this page opened or saved
    if (!target) {
      const name = typeof suggestedName === 'string' && suggestedName ? path.basename(suggestedName) : KINDS[kind].defaultName
      const lastDir = recent(kind)[0] ? path.dirname(recent(kind)[0]) : undefined
      const result = await dialog.showSaveDialog(getWindow(), {
        defaultPath: lastDir ? path.join(lastDir, name) : name,
        filters: filters(kind),
      })
      if (result.canceled || !result.filePath) return null
      target = result.filePath
    }
    await fs.promises.writeFile(target, text, 'utf8')
    setRecent(kind, addToRecent(recent(kind), target))
    return { path: target, name: path.basename(target) }
  })

  return {
    recent,
    clearRecent: (kind) => setRecent(kind, []),
  }
}

module.exports = { createFiles, kindForUrl, addToRecent, MAX_RECENT }
