// Embedded starter config. For now the editor boots with this "Live Rig" so
// there is something to edit; a later change swaps it for a filesystem picker
// (the store already has open()/save() plumbing for that).
import raw from './liveRig.json'
import type { DeviceConfig } from '../types/config'

export const liveRigConfig = raw as unknown as DeviceConfig
export const liveRigFileName = 'Live Rig.json'
