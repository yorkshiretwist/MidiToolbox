// Tests for the device store: loading and switching device profiles.
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useDeviceStore } from './device'
import { liveRigDevice } from '../data/liveRigDevice'
import type { DeviceFile } from '../types/device'

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('useDeviceStore', () => {
  it('boots with the embedded reference profile', () => {
    const store = useDeviceStore()
    expect(store.profile.name).toBe('Live Rig Unit')
    expect(store.profile.navigation).toEqual({ select: 10, down: 11, up: 12 })
    expect(store.buttonIds).toEqual([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
  })

  it('loadDevice replaces the profile wholesale', () => {
    const store = useDeviceStore()
    const finger: DeviceFile = {
      ...liveRigDevice,
      name: 'Finger Pad',
      buttons: liveRigDevice.buttons.slice(0, 8),
      navigation: { select: 5, down: 6, up: 7 },
    }
    store.loadDevice(finger)
    expect(store.profile.name).toBe('Finger Pad')
    expect(store.buttonIds).toEqual([0, 1, 2, 3, 4, 5, 6, 7])
  })

  it('resetToDefault restores the reference profile after a load', () => {
    const store = useDeviceStore()
    store.loadDevice({ ...liveRigDevice, name: 'Something Else' })
    store.resetToDefault()
    expect(store.profile.name).toBe('Live Rig Unit')
  })

  it('does not mutate the embedded liveRigDevice fixture', () => {
    const store = useDeviceStore()
    store.profile.name = 'Mutated'
    expect(liveRigDevice.name).toBe('Live Rig Unit')
  })
})
