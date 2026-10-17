// Starting points for the "Start from" list. Every example is a device file in
// Designer/examples/*.json (the same format Save writes), bundled at build time.
// To add one, save a design from the Designer and drop the file in that folder.
//
// Files are listed in filename order and the first is the default design. A
// leading "<number>-" only controls that order; the rest of the filename is the
// example's id (used by the ?preset= deep link), so "1-live-rig.json" is "live-rig".

import type { Design } from '../types/device'
import { parseDevice } from './deviceFile'

export interface Preset {
  id: string
  label: string
  build: () => Design
}

const files = import.meta.glob('../../examples/*.json', { query: '?raw', import: 'default', eager: true }) as Record<string, string>

function toPreset(path: string, text: string): Preset | null {
  const file = path.split('/').pop()!
  const id = file.replace(/\.json$/i, '').replace(/^\d+-/, '')
  const result = parseDevice(text)
  if (!result.design) {
    console.warn(`Skipping example ${file}: ${result.errors.join('; ')}`)
    return null
  }
  const { name, purpose, buttons } = result.design
  return {
    id,
    label: `${name} (${buttons.length} buttons, ${purpose})`,
    // parsed afresh each time so editing a design never changes the example
    build: () => parseDevice(text).design!,
  }
}

export const PRESETS: Preset[] = Object.entries(files)
  .sort(([a], [b]) => a.localeCompare(b, undefined, { numeric: true }))
  .map(([path, text]) => toPreset(path, text))
  .filter((preset): preset is Preset => preset !== null)

export function buildPreset(id: string): Design {
  return (PRESETS.find((p) => p.id === id) ?? PRESETS[0]!).build()
}
