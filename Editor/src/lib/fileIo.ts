// Open / save a config .json purely client-side.
//
// Uses the File System Access API (Chromium) so "Save" writes back to the same
// file the user opened. Falls back to a download / hidden <input> when the API
// is missing (Firefox, Safari).

export interface OpenedFile {
  name: string
  text: string
  /** Present only when the File System Access API is available. */
  handle?: FileSystemFileHandle
}

interface FsWindow extends Window {
  showOpenFilePicker?: (opts?: unknown) => Promise<FileSystemFileHandle[]>
  showSaveFilePicker?: (opts?: unknown) => Promise<FileSystemFileHandle>
}

const jsonPickerOpts = (description: string) => ({
  types: [{ description, accept: { 'application/json': ['.json'] } }],
})

export function isFileSystemAccessSupported(): boolean {
  return typeof (window as FsWindow).showOpenFilePicker === 'function'
}

export async function openConfigFile(): Promise<OpenedFile | null> {
  return openJsonFile('JSON config')
}

/** A -design.json exported from the Designer app — see stores/device.ts. */
export async function openDeviceFile(): Promise<OpenedFile | null> {
  return openJsonFile('Device profile')
}

async function openJsonFile(description: string): Promise<OpenedFile | null> {
  const w = window as FsWindow
  if (w.showOpenFilePicker) {
    const [handle] = await w.showOpenFilePicker(jsonPickerOpts(description))
    if (!handle) return null
    const file = await handle.getFile()
    return { name: file.name, text: await file.text(), handle }
  }
  return openViaInput()
}

function openViaInput(): Promise<OpenedFile | null> {
  return new Promise((resolve) => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json,application/json'
    input.onchange = async () => {
      const file = input.files?.[0]
      if (!file) return resolve(null)
      resolve({ name: file.name, text: await file.text() })
    }
    input.click()
  })
}

/** Write to the opened handle when we have one; otherwise trigger a download. */
export async function saveConfigFile(
  file: OpenedFile,
  text: string,
): Promise<void> {
  if (file.handle && 'createWritable' in file.handle) {
    const writable = await file.handle.createWritable()
    await writable.write(text)
    await writable.close()
    return
  }
  downloadText(file.name || 'config.json', text)
}

export async function saveConfigFileAs(
  suggestedName: string,
  text: string,
): Promise<OpenedFile | null> {
  const w = window as FsWindow
  if (w.showSaveFilePicker) {
    const handle = await w.showSaveFilePicker({
      ...jsonPickerOpts,
      suggestedName,
    })
    const writable = await handle.createWritable()
    await writable.write(text)
    await writable.close()
    return { name: handle.name, text, handle }
  }
  downloadText(suggestedName, text)
  return null
}

function downloadText(filename: string, text: string): void {
  const blob = new Blob([text], { type: 'application/json' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}

/** Always triggers a real browser download, regardless of File System Access support. */
export function downloadConfigFile(filename: string, text: string): void {
  downloadText(filename, text)
}
