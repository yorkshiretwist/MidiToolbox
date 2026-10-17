// Embedded default device profile: the reference 13-button unit, matching
// MIDI-Toolbox/MidiToolboxDevice.h exactly. Generated from the Designer's own
// 'live-rig' preset (toDeviceFile(buildPreset('live-rig'))) rather than
// hand-transcribed, so it can't drift from what the Designer itself would
// produce. The editor boots with this profile active; "Open Device" (see
// stores/device.ts) replaces it with one loaded from a -design.json file.
import raw from './liveRigDevice.json'
import type { DeviceFile } from '../types/device'

export const liveRigDevice = raw as unknown as DeviceFile
