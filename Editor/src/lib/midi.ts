// Thin wrapper around the Web MIDI API for the play-mode simulator: requests
// access once, tracks the live set of output ports, and lets a caller pick one
// and send NoteOn/Off, ControlChange and ProgramChange messages to it.
//
// `midiState` is reactive and is what components read. The raw `MIDIOutput`
// objects are kept out of Vue's reactivity, in a plain Map keyed by port id —
// wrapping host objects in a reactive proxy can break their native methods.

import { reactive } from 'vue'

const STORAGE_KEY = 'midi-toolbox-editor:midi-output-id'

interface MidiOutputInfo {
  id: string
  name: string
  manufacturer: string
}

interface MidiState {
  /** False on a browser with no Web MIDI API at all (e.g. Safari). */
  supported: boolean
  /** Set once requestMIDIAccess() has resolved or rejected. */
  permission: 'unknown' | 'granted' | 'denied'
  outputs: MidiOutputInfo[]
  selectedOutputId: string | null
  error: string | null
}

export const midiState = reactive<MidiState>({
  supported: typeof navigator !== 'undefined' && 'requestMIDIAccess' in navigator,
  permission: 'unknown',
  outputs: [],
  selectedOutputId: null,
  error: null,
})

let access: MIDIAccess | null = null
const outputPorts = new Map<string, MIDIOutput>()

function readRememberedId(): string | null {
  try {
    return localStorage.getItem(STORAGE_KEY)
  } catch {
    return null
  }
}

function writeRememberedId(id: string | null): void {
  try {
    if (id) localStorage.setItem(STORAGE_KEY, id)
    else localStorage.removeItem(STORAGE_KEY)
  } catch {
    // Storage unavailable (private browsing, disabled cookies) — not fatal.
  }
}

function refreshOutputs(): void {
  outputPorts.clear()
  const infos: MidiOutputInfo[] = []
  access?.outputs.forEach((port) => {
    outputPorts.set(port.id, port)
    infos.push({
      id: port.id,
      name: port.name ?? 'Unnamed output',
      manufacturer: port.manufacturer ?? '',
    })
  })
  midiState.outputs = infos

  if (midiState.selectedOutputId && !outputPorts.has(midiState.selectedOutputId)) {
    midiState.selectedOutputId = null
  }
  if (!midiState.selectedOutputId && infos.length > 0) {
    const remembered = readRememberedId()
    selectOutput(remembered && outputPorts.has(remembered) ? remembered : infos[0].id)
  }
}

/**
 * Requests MIDI access from the browser (triggers its permission prompt on
 * first call) and starts tracking output ports as they connect/disconnect.
 * A no-op once permission has already been granted or denied — call
 * `selectOutput`/`send*` freely after that instead of calling this again.
 */
export async function initMidi(): Promise<void> {
  if (!midiState.supported || midiState.permission !== 'unknown') return
  try {
    access = await navigator.requestMIDIAccess()
    midiState.permission = 'granted'
    access.onstatechange = refreshOutputs
    refreshOutputs()
  } catch (err) {
    midiState.permission = 'denied'
    midiState.error = err instanceof Error ? err.message : String(err)
  }
}

/** Selects the output port that `send*` targets, and remembers it for next time. */
export function selectOutput(id: string | null): void {
  midiState.selectedOutputId = id
  writeRememberedId(id)
}

function currentOutput(): MIDIOutput | null {
  return midiState.selectedOutputId ? outputPorts.get(midiState.selectedOutputId) ?? null : null
}

function clampChannel(channel: number): number {
  return Math.min(Math.max(Math.trunc(channel), 1), 16) - 1
}

function clampByte(value: number): number {
  return Math.min(Math.max(Math.trunc(value), 0), 127)
}

export function sendNoteOn(note: number, velocity = 127, channel = 1): void {
  currentOutput()?.send([0x90 | clampChannel(channel), clampByte(note), clampByte(velocity)])
}

export function sendNoteOff(note: number, channel = 1): void {
  currentOutput()?.send([0x80 | clampChannel(channel), clampByte(note), 0])
}

export function sendControlChange(controller: number, value: number, channel = 1): void {
  currentOutput()?.send([0xb0 | clampChannel(channel), clampByte(controller), clampByte(value)])
}

export function sendProgramChange(program: number, channel = 1): void {
  currentOutput()?.send([0xc0 | clampChannel(channel), clampByte(program)])
}
