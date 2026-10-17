// TypeScript mirror of config/schema.json (JSON Schema draft-07).
// ajv validation against the schema is the source of truth at runtime; these
// types exist for editor ergonomics. Keep them in step with the schema.

export type Rgb = number[] // [r, g, b], 0-255 by convention

export type NavigationParameter =
  | 'Patch'
  | 'Bank'
  | 'Mode'
  | 'Config'
  | 'MidiChannel'
  | 'Page'

export type ButtonMode = 'Momentary' | 'Latched' | 'Exclusive'
export type DefaultStatus = 'Off' | 'On'
export type NoteSpread = 'NoSpread' | 'SpreadUp' | 'SpreadDown' | 'SpreadRandom'
export type CycleDirection = 'Up' | 'Down'
export type BankType = 'Default' | 'PersistentButtons'

/** How the device screen is laid out while a Bank is selected — 'Grid' (default) or the large-type 'Live' view. */
export type ScreenLayout = 'Grid' | 'Live'

export type ActionType =
  | 'ExecutePreset'
  | 'PlayNotes'
  | 'ProgramChange'
  | 'ControlChange'
  | 'SetParameter'
  | 'CycleParameter'
  | 'CycleActivateParameter'
  | 'SetParameterValue'
  | 'CycleParameterValue'
  | 'SetPatch'
  | 'CyclePatch'
  | 'SetBank'
  | 'CycleBank'
  | 'SetMode'
  | 'CycleMode'
  | 'SetConfig'
  | 'CycleConfig'
  | 'SetPage'
  | 'CyclePage'

export interface ActionParameters {
  midiChannel?: number
  noteNames?: string[]
  velocity?: number
  noteSpread?: NoteSpread
  direction?: CycleDirection
  programNumber?: number
  controllerNumber?: number
  controlValue?: number
  releaseValue?: number
  parameterName?: string
  parameterValue?: string
  patchIndex?: number
  page?: number
  bankIndex?: number
  modeIndex?: number
  configFilename?: string
  /** ExecutePreset only: the `id` of the Preset to run at this point in the button's actions. */
  presetId?: number
}

export interface Action {
  type: ActionType
  actionParameters?: ActionParameters
}

/**
 * A named, reusable list of actions, run from any button with an ExecutePreset
 * action. Presets live once, at the top of the file; buttons refer to them by
 * `id`. A preset's own actions can never be ExecutePreset (no nesting).
 */
export interface Preset {
  id: number
  name: string
  actions: Action[]
}

export interface Button {
  id: number // physical button index on the active device profile
  name: string
  buttonMode: ButtonMode
  buttonSet?: number[]
  page?: number
  defaultStatus?: DefaultStatus
  colourCode?: Rgb
  actions: Action[]
}

export interface Patch {
  name: string
  midiChannel?: number
  parameters?: NavigationParameter[]
  buttons: Button[]
}

export interface Bank {
  name: string
  midiChannel?: number
  subheaderColourCode?: Rgb
  screenLayout?: ScreenLayout
  bankType?: BankType
  maintainActiveButtonOnPatchChange?: boolean
  defaultPatchIndex?: number
  parameters?: NavigationParameter[]
  buttons?: Button[]
  patches: Patch[]
}

export interface Mode {
  name: string
  midiChannel?: number
  parameters?: NavigationParameter[]
  buttons?: Button[]
  banks: Bank[]
}

export interface DeviceConfig {
  headerColourCode?: Rgb
  midiChannel?: number
  parameters?: NavigationParameter[]
  presets?: Preset[]
  buttons?: Button[]
  modes: Mode[]
}

/** A location within the config tree, as an array of keys/indices from the root. */
export type SelectionPath = Array<string | number>
