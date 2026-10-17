// Values an action always has. A note's velocity and a control change's On and Off values are never
// left empty: the editor shows these defaults, refuses to leave a box blank, and writes the values
// into every saved file.
//
// The control change defaults are what the firmware uses when a file leaves them out (see
// ControlChangeAction.h). The velocity default is 100 here; a file that has no velocity at all is
// played by the unit at its own default (99, see note.h) until the file is saved by the editor,
// which spells 100 out.

import type { Action, Button, DeviceConfig } from '../types/config'

/** The velocity of a note that has none set. */
export const DEFAULT_VELOCITY = 100
/** The CC value sent when the button turns on. */
export const CC_DEFAULT_ON = 127
/** The CC value sent when the button turns off. */
export const CC_DEFAULT_OFF = 0

/** A new, empty "Play notes" action, already carrying its default velocity. */
export function newPlayNotesAction(): Action {
  return { type: 'PlayNotes', actionParameters: { velocity: DEFAULT_VELOCITY } }
}

/**
 * Gives an action the values it must always have if it has none: a velocity for Play notes, and
 * an On and an Off value for a control change. Values that are set are never changed, and other
 * action types are left alone.
 */
export function ensureActionDefaults(action: Action): void {
  if (action.type === 'PlayNotes') {
    const params = (action.actionParameters ??= {})
    if (params.velocity === undefined) params.velocity = DEFAULT_VELOCITY
  } else if (action.type === 'ControlChange') {
    const params = (action.actionParameters ??= {})
    if (params.controlValue === undefined) params.controlValue = CC_DEFAULT_ON
    if (params.releaseValue === undefined) params.releaseValue = CC_DEFAULT_OFF
  }
}

function fillButtons(buttons: Button[] | undefined): void {
  for (const button of buttons ?? []) for (const action of button.actions ?? []) ensureActionDefaults(action)
}

/**
 * Fills in the defaults of every action in the file, including the ones inside presets, changing
 * `config` in place. Meant for the copy that is about to be saved, so a file that left them out is
 * written with them spelled out.
 */
export function fillActionDefaults(config: DeviceConfig): void {
  for (const preset of config.presets ?? []) for (const action of preset.actions) ensureActionDefaults(action)
  fillButtons(config.buttons)
  for (const mode of config.modes) {
    fillButtons(mode.buttons)
    for (const bank of mode.banks) {
      fillButtons(bank.buttons)
      for (const patch of bank.patches) fillButtons(patch.buttons)
    }
  }
}
