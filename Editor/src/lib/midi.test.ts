// Tests for lib/midi.ts using a mocked Web MIDI API.
import { beforeEach, describe, expect, it, vi } from 'vitest'
import {
  initMidi,
  midiState,
  selectOutput,
  sendControlChange,
  sendNoteOff,
  sendNoteOn,
  sendProgramChange,
} from './midi'

function fakeOutput(id: string, name: string) {
  return { id, name, manufacturer: 'Acme', send: vi.fn() }
}

function fakeAccess(outputs: ReturnType<typeof fakeOutput>[]) {
  return {
    outputs: {
      forEach(cb: (port: ReturnType<typeof fakeOutput>) => void) {
        outputs.forEach(cb)
      },
    },
    onstatechange: null,
  }
}

beforeEach(() => {
  localStorage.clear()
  midiState.permission = 'unknown'
  midiState.outputs = []
  midiState.selectedOutputId = null
  midiState.error = null
  midiState.supported = true
  vi.unstubAllGlobals()
})

describe('initMidi', () => {
  it('does nothing when the browser has no Web MIDI API', async () => {
    midiState.supported = false
    vi.stubGlobal('navigator', { requestMIDIAccess: vi.fn() })
    await initMidi()
    expect(midiState.permission).toBe('unknown')
  })

  it('grants access and lists outputs', async () => {
    const outA = fakeOutput('a', 'Synth A')
    const outB = fakeOutput('b', 'Synth B')
    const requestMIDIAccess = vi.fn().mockResolvedValue(fakeAccess([outA, outB]))
    vi.stubGlobal('navigator', { requestMIDIAccess })

    await initMidi()

    expect(midiState.permission).toBe('granted')
    expect(midiState.outputs).toEqual([
      { id: 'a', name: 'Synth A', manufacturer: 'Acme' },
      { id: 'b', name: 'Synth B', manufacturer: 'Acme' },
    ])
    expect(midiState.selectedOutputId).toBe('a')
  })

  it('records denial when the browser rejects the request', async () => {
    const requestMIDIAccess = vi.fn().mockRejectedValue(new Error('nope'))
    vi.stubGlobal('navigator', { requestMIDIAccess })

    await initMidi()

    expect(midiState.permission).toBe('denied')
    expect(midiState.error).toBe('nope')
  })

  it('is a no-op once permission is already settled', async () => {
    midiState.permission = 'granted'
    const requestMIDIAccess = vi.fn()
    vi.stubGlobal('navigator', { requestMIDIAccess })

    await initMidi()

    expect(requestMIDIAccess).not.toHaveBeenCalled()
  })
})

describe('selectOutput', () => {
  it('remembers the chosen output id across calls', () => {
    selectOutput('b')
    expect(midiState.selectedOutputId).toBe('b')
    expect(localStorage.getItem('midi-toolbox-editor:midi-output-id')).toBe('b')

    selectOutput(null)
    expect(midiState.selectedOutputId).toBeNull()
    expect(localStorage.getItem('midi-toolbox-editor:midi-output-id')).toBeNull()
  })

  it('re-selects the remembered output on the next refresh', async () => {
    localStorage.setItem('midi-toolbox-editor:midi-output-id', 'b')
    const outA = fakeOutput('a', 'Synth A')
    const outB = fakeOutput('b', 'Synth B')
    vi.stubGlobal('navigator', {
      requestMIDIAccess: vi.fn().mockResolvedValue(fakeAccess([outA, outB])),
    })

    await initMidi()

    expect(midiState.selectedOutputId).toBe('b')
  })
})

describe('sending messages', () => {
  async function withSelectedOutput() {
    const out = fakeOutput('a', 'Synth A')
    vi.stubGlobal('navigator', {
      requestMIDIAccess: vi.fn().mockResolvedValue(fakeAccess([out])),
    })
    await initMidi()
    return out
  }

  it('sends note on/off on the selected channel', async () => {
    const out = await withSelectedOutput()

    sendNoteOn(60, 100, 2)
    expect(out.send).toHaveBeenLastCalledWith([0x91, 60, 100])

    sendNoteOff(60, 2)
    expect(out.send).toHaveBeenLastCalledWith([0x81, 60, 0])
  })

  it('sends control change and program change on channel 1 by default', async () => {
    const out = await withSelectedOutput()

    sendControlChange(64, 127)
    expect(out.send).toHaveBeenLastCalledWith([0xb0, 64, 127])

    sendProgramChange(5)
    expect(out.send).toHaveBeenLastCalledWith([0xc0, 5])
  })

  it('clamps out-of-range values into the valid MIDI byte range', async () => {
    const out = await withSelectedOutput()

    sendNoteOn(200, -10, 99)
    expect(out.send).toHaveBeenLastCalledWith([0x9f, 127, 0])
  })

  it('does nothing when no output is selected', () => {
    sendNoteOn(60)
    // Nothing to assert on — there is no output port to have received a call,
    // and calling send() must not throw.
  })
})
