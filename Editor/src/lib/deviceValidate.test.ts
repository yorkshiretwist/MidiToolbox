// Tests for lib/deviceValidate.ts: parsing and validating device profile files.
import { describe, expect, it } from 'vitest'
import { parseDeviceFile } from './deviceValidate'
import { liveRigDevice } from '../data/liveRigDevice'

describe('parseDeviceFile', () => {
  it('accepts the reference device file', () => {
    const result = parseDeviceFile(JSON.stringify(liveRigDevice))
    expect(result.errors).toBeUndefined()
    expect(result.device?.name).toBe('Live Rig Unit')
  })

  it('rejects invalid JSON', () => {
    const result = parseDeviceFile('{not json')
    expect(result.device).toBeUndefined()
    expect(result.errors?.[0]).toMatch(/not valid json/i)
  })

  it('rejects a file that fails the schema', () => {
    const result = parseDeviceFile(JSON.stringify({ name: 'Incomplete' }))
    expect(result.device).toBeUndefined()
    expect(result.errors?.length).toBeGreaterThan(0)
  })

  it('rejects duplicate button ids', () => {
    // Replace id 1 (an ordinary performance button, not a nav role) with a
    // second copy of id 0, so the only error is the duplicate itself.
    const bad = {
      ...liveRigDevice,
      buttons: liveRigDevice.buttons.map((b) => (b.id === 1 ? { ...liveRigDevice.buttons[0] } : b)),
    }
    const result = parseDeviceFile(JSON.stringify(bad))
    expect(result.errors).toEqual(['Two buttons share the same id'])
  })

  it('rejects navigation ids that do not reference a real button', () => {
    const bad = { ...liveRigDevice, navigation: { select: 99, down: 11, up: 12 } }
    const result = parseDeviceFile(JSON.stringify(bad))
    expect(result.errors?.[0]).toMatch(/refers to button id 99/)
  })

  it('rejects navigation roles that collide on the same button', () => {
    const bad = { ...liveRigDevice, navigation: { select: 10, down: 10, up: 12 } }
    const result = parseDeviceFile(JSON.stringify(bad))
    expect(result.errors).toContain('The three navigation roles must be three different buttons')
  })
})
