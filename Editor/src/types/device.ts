// The on-disk shape of a device profile, defined by config/design-schema.json
// and written by the Designer app. The Editor reads one to know which
// physical buttons exist on the active unit and which three play the built-in
// navigation roles; it has no reason to care about the Designer-only fields
// (screen/microcontroller catalogue lookups, pin planning). Keep in step with
// design-schema.json.

export type Purpose = 'foot' | 'finger'
export type LedSide = 'above' | 'below'
export type Orientation = 'landscape' | 'portrait'
export type Numbering = 'bottom-left' | 'top-left' | 'manual'
export type NavRole = 'select' | 'down' | 'up'
export type LedPin = 'latch' | 'data' | 'clock'

export interface DeviceButton {
  /** Physical button index that performance configs refer to. */
  id: number
  /** Centre, mm from the unit's left edge. */
  xMm: number
  /** Centre, mm from the unit's top edge. */
  yMm: number
  led: LedSide
}

export interface DeviceFile {
  schemaVersion: 1
  name: string
  purpose: Purpose
  unit: {
    widthMm: number
    heightMm: number
    gridPitchMm: number
    /** Colour of the unit's face as [r, g, b]; very dark grey when absent. */
    colourCode?: number[]
  }
  numbering: Numbering
  buttons: DeviceButton[]
  /** Button ids that play the three built-in navigation roles. */
  navigation: Record<NavRole, number>
  screen: { catalogId: string; orientation: Orientation; xMm: number; yMm: number }
  microcontroller: string
  pins?: {
    buttons?: { id: number; pin: number }[]
    screen?: Record<string, number>
    ledChain?: Partial<Record<LedPin, number>>
  }
}
