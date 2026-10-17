// Keeps the working config in the browser's local storage so closing and
// reopening the tab restores the edits (and which device, file name and pane
// they were made in). Wired up once from main.ts via restoreWorkingState().
import type { Pinia } from 'pinia'
import { useConfigStore } from '../stores/config'
import { useDeviceStore } from '../stores/device'
import { parseDeviceFile } from './deviceValidate'
import type { DeviceConfig } from '../types/config'

const STORAGE_KEY = 'midi-toolbox-editor:working-state'
const VERSION = 1
const SAVE_DELAY_MS = 300

interface Stored {
  version: number
  config: DeviceConfig
  fileName: string | null
  selection: (string | number)[]
  page: number
  dirty: boolean
  device: unknown
}

function read(): Stored | null {
  try {
    const text = localStorage.getItem(STORAGE_KEY)
    if (!text) return null
    const data = JSON.parse(text) as Stored
    if (data?.version !== VERSION || !Array.isArray(data.config?.modes)) return null
    return data
  } catch {
    return null
  }
}

/** Loads any stored state into the stores, then keeps storage up to date as they change. */
export function restoreWorkingState(pinia: Pinia): void {
  const config = useConfigStore(pinia)
  const device = useDeviceStore(pinia)

  const stored = read()
  if (stored) {
    // The device goes first: it must be valid for the config's button ids to mean anything.
    const parsed = parseDeviceFile(JSON.stringify(stored.device))
    if (parsed.device) device.loadDevice(parsed.device)
    config.config = stored.config
    config.fileName = stored.fileName
    config.selection = Array.isArray(stored.selection) ? stored.selection : config.selection
    config.page = typeof stored.page === 'number' ? stored.page : 1
    config.dirty = stored.dirty === true
  }

  let timer: ReturnType<typeof setTimeout> | undefined
  const save = () => {
    timer = undefined
    const state: Stored = {
      version: VERSION,
      config: config.config,
      fileName: config.fileName,
      selection: config.selection,
      page: config.page,
      dirty: config.dirty,
      device: device.profile,
    }
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(state))
    } catch {
      // Storage full or unavailable (private mode): editing carries on, just unsaved.
    }
  }
  const schedule = () => {
    if (timer === undefined) timer = setTimeout(save, SAVE_DELAY_MS)
  }

  config.$subscribe(schedule, { detached: true })
  device.$subscribe(schedule, { detached: true })
  // A change made just before the tab closes mustn't be lost to the debounce.
  window.addEventListener('pagehide', () => {
    if (timer !== undefined) {
      clearTimeout(timer)
      save()
    }
  })
}
