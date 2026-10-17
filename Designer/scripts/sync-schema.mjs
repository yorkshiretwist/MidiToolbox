// Copies the canonical schemas into the app bundle.
//
// The single source of truth is <repo>/config/: design-schema.json (the design
// file the Designer saves and the Editor opens) and device-schema.json (the
// device.json the firmware reads). The Designer imports copies from
// src/generated/ so the build has no path dependency on files outside Designer/.
// Run automatically via the "predev" / "prebuild" / "pretest" npm scripts.

import { copyFileSync, mkdirSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const destDir = resolve(here, '../src/generated')
mkdirSync(destDir, { recursive: true })

for (const name of ['design-schema.json', 'device-schema.json']) {
  const source = resolve(here, '../../config', name)
  const dest = resolve(destDir, name)
  copyFileSync(source, dest)
  console.log(`[sync-schema] ${source} -> ${dest}`)
}
