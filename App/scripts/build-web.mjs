// Builds the Editor and the Designer and lays them out in App/web exactly as the GitHub Pages
// site does: the Editor under editor/, the Designer under designer/. The app serves this folder.
//
//   npm run build:web             builds with the dependencies already installed
//   npm run build:web -- --install  runs "npm ci" in both apps first (what CI does)

import { spawnSync } from 'node:child_process'
import { cpSync, existsSync, mkdirSync, rmSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const repo = resolve(here, '../..')
const web = resolve(here, '../web')
const install = process.argv.includes('--install')

function run(cwd, args, env = {}) {
  console.log(`[build-web] ${cwd}: npm ${args.join(' ')}`)
  const result = spawnSync(`npm ${args.join(' ')}`, { cwd, stdio: 'inherit', shell: true, env: { ...process.env, ...env } })
  if (result.status !== 0) {
    console.error(`[build-web] "npm ${args.join(' ')}" failed in ${cwd}`)
    process.exit(result.status ?? 1)
  }
}

function build(name, env) {
  const dir = resolve(repo, name)
  if (install || !existsSync(resolve(dir, 'node_modules'))) run(dir, ['ci'])
  run(dir, ['run', 'build'], env)
  return resolve(dir, 'dist')
}

const editorDist = build('Editor', { EDITOR_BASE: '/editor/' })
const designerDist = build('Designer', { DESIGNER_BASE: '/designer/' })

rmSync(web, { recursive: true, force: true })
mkdirSync(resolve(web, 'editor'), { recursive: true })
mkdirSync(resolve(web, 'designer'), { recursive: true })
cpSync(editorDist, resolve(web, 'editor'), { recursive: true })
cpSync(designerDist, resolve(web, 'designer'), { recursive: true })
console.log(`[build-web] done: ${web}`)
