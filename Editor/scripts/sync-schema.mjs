// Copies the canonical schemas into the app bundle.
//
// The single source of truth for both is <repo>/config/. The editor imports a
// copy from src/generated/ so the build has no path dependency on files
// outside Editor/. Run automatically via the "predev" / "prebuild" / "pretest"
// npm scripts.

import { copyFileSync, mkdirSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const destDir = resolve(here, '../src/generated')
mkdirSync(destDir, { recursive: true })

for (const name of ['schema.json', 'design-schema.json']) {
  const source = resolve(here, '../../config', name)
  const dest = resolve(destDir, name)
  copyFileSync(source, dest)
  console.log(`[sync-schema] ${source} -> ${dest}`)
}

// The Designer's screen catalogue: the Editor draws the screen at its real
// physical size, so it needs each module's outer and active dimensions.
const screens = resolve(here, '../../Designer/src/data/catalog/screens.json')
copyFileSync(screens, resolve(destDir, 'screens.json'))
console.log(`[sync-schema] ${screens} -> ${resolve(destDir, 'screens.json')}`)
