// Every generated file, produced once from a design.

import { strToU8, zipSync } from 'fflate'
import type { Design } from '../../types/device'
import type { Analysis } from '../checks'
import { serialiseDevice } from '../deviceFile'
import { buildDeviceProfile, serialiseDeviceProfile } from './deviceProfile'
import { drawingSvg } from './drawing'
import { buildGuide, firmwareHeader } from './firmware'
import { partsCsv, partsList, partsMarkdown, type PartRow } from './parts'
import { schematicSvg } from './schematic'

export interface Artifacts {
  /** The Designer's own file for this unit: <name>-design.json. */
  designJson: string
  /** What the firmware reads from the SD card: device.json. */
  deviceJson: string
  header: string
  buildGuide: string
  drawingSvg: string
  schematicSvg: string
  parts: PartRow[]
  partsCsv: string
  partsMarkdown: string
}

export function buildArtifacts(design: Design, analysis: Analysis): Artifacts {
  const parts = partsList(design, analysis)
  return {
    designJson: serialiseDevice(design),
    deviceJson: serialiseDeviceProfile(buildDeviceProfile(design, analysis)),
    header: firmwareHeader(design, analysis),
    buildGuide: buildGuide(design, analysis),
    drawingSvg: drawingSvg(design, analysis),
    schematicSvg: schematicSvg(design, analysis),
    parts,
    partsCsv: partsCsv(parts),
    partsMarkdown: partsMarkdown(design, parts),
  }
}

/** File name → contents for the "download everything" zip. */
export function zipEntries(artifacts: Artifacts, stem: string): Record<string, string> {
  return {
    [`${stem}-design.json`]: artifacts.designJson,
    'device.json': artifacts.deviceJson,
    'MidiToolboxDevice.h': artifacts.header,
    'BUILD.md': artifacts.buildGuide,
    'drawing.svg': artifacts.drawingSvg,
    'schematic.svg': artifacts.schematicSvg,
    'parts.csv': artifacts.partsCsv,
    'parts.md': artifacts.partsMarkdown,
  }
}

export function buildZip(entries: Record<string, string>): Uint8Array {
  const files: Record<string, Uint8Array> = {}
  for (const [name, text] of Object.entries(entries)) files[name] = strToU8(text)
  return zipSync(files)
}
