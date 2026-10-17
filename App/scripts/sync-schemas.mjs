// Copies the canonical schemas from <repo>/config into the app, so the Device tab can check a
// device.json before it is sent to the unit. A test fails if these copies fall out of date.
// Run with: npm run sync-schemas

import { copyFileSync, mkdirSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const destDir = resolve(here, '../schemas')
mkdirSync(destDir, { recursive: true })

for (const name of ['device-schema.json']) {
  const source = resolve(here, '../../config', name)
  copyFileSync(source, resolve(destDir, name))
  console.log(`[sync-schemas] ${source} -> ${resolve(destDir, name)}`)
}
