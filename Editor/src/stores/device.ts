// The active device profile: which physical buttons exist on this unit and
// which three play the built-in navigation roles. Parallel to the config
// store, but much smaller — nothing here is edited in place, only replaced
// wholesale by opening a different -design.json.
//
// Boots with the embedded reference profile (liveRigDevice), matching
// MIDI-Toolbox/MidiToolboxDevice.h, so the editor keeps working with no setup
// — the same role liveRig.ts plays for the config store's starting config.
//
// Picking a file (pickDeviceFile) and committing it (loadDevice) are
// deliberately separate: switching device is destructive to the open config
// (see ChangeDeviceLink.vue, which clears every button before committing),
// so the caller needs the parsed file in hand before deciding whether to go
// through with it — pickDeviceFile never touches `profile` itself.

import { defineStore } from 'pinia'
import { openDeviceFile as pickDeviceFileFromDisk } from '../lib/fileIo'
import { parseDeviceFile } from '../lib/deviceValidate'
import { liveRigDevice } from '../data/liveRigDevice'
import { rgbToHex } from '../lib/colour'
import type { DeviceFile } from '../types/device'

/** Colour of the unit's face when the device file doesn't give one: very dark grey. */
export const DEFAULT_UNIT_COLOUR = [38, 38, 38]

function startingProfile(): DeviceFile {
  return structuredClone(liveRigDevice)
}

export const useDeviceStore = defineStore('device', {
  state: () => ({
    profile: startingProfile(),
    /** Set only when a pick failed (bad JSON/schema) — distinct from the user cancelling the picker, which clears it. */
    openError: null as string | null,
  }),

  getters: {
    buttonIds(state): number[] {
      return state.profile.buttons.map((b) => b.id)
    },
    /** The unit's face colour as "#rrggbb", for the device panel's background. */
    unitColour(state): string {
      return rgbToHex(state.profile.unit.colourCode ?? DEFAULT_UNIT_COLOUR)
    },
  },

  actions: {
    /** Replace the active profile outright. */
    loadDevice(device: DeviceFile) {
      this.profile = device
      this.openError = null
    },

    resetToDefault() {
      this.profile = startingProfile()
      this.openError = null
    },

    /**
     * Prompts for a -design.json and parses it WITHOUT committing — returns
     * the parsed profile, or null if the user cancelled the picker or the
     * file was invalid (see openError for why, in the latter case).
     */
    async pickDeviceFile(): Promise<DeviceFile | null> {
      const file = await pickDeviceFileFromDisk()
      if (!file) {
        this.openError = null
        return null
      }
      const result = parseDeviceFile(file.text)
      if (result.errors) {
        this.openError = `${file.name}: ${result.errors.join('; ')}`
        return null
      }
      this.openError = null
      return result.device
    },
  },
})
