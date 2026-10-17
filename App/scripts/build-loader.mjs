// Builds teensy_loader_cli (the open-source command-line programmer for Teensy boards, GPL v3)
// from the copy of its source in App/vendor/teensy_loader_cli/ into App/resources/loader/, where the
// app looks for it. Run once per system you build the app on:
//   npm run loader
//
// Needs a C compiler (gcc or clang; set CC to choose), plus:
//   Windows  MinGW-w64 gcc (no extra libraries)
//   macOS    Xcode command-line tools
//   Linux    libusb 0.1 headers:  sudo apt install libusb-dev

import { spawnSync } from 'node:child_process'
import { existsSync, mkdirSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const here = dirname(fileURLToPath(import.meta.url))
const source = resolve(here, '../vendor/teensy_loader_cli/teensy_loader_cli.c')
const outDir = resolve(here, '../resources/loader')

if (!existsSync(source)) {
  console.error(`[loader] the loader source is missing: ${source}`)
  console.error('[loader] see App/vendor/README.md for where it comes from.')
  process.exit(1)
}

mkdirSync(outDir, { recursive: true })
const compiler = process.env.CC || 'gcc'

// the same flags as the upstream Makefile uses for each system
const builds = {
  // -I shims: a forwarding <ddk/hidsdi.h> for current MinGW-w64 (see scripts/shims/ddk/hidsdi.h)
  win32: {
    out: 'teensy_loader_cli.exe',
    args: ['-O2', '-Wall', '-s', '-DUSE_WIN32', '-I', resolve(here, 'shims'), '-lhid', '-lsetupapi', '-lwinmm'],
  },
  darwin: { out: 'teensy_loader_cli', args: ['-O2', '-Wall', '-DUSE_APPLE_IOKIT', '-framework', 'IOKit', '-framework', 'CoreFoundation'] },
  linux: { out: 'teensy_loader_cli', args: ['-O2', '-Wall', '-s', '-DUSE_LIBUSB', '-lusb'] },
}
const build = builds[process.platform]
if (!build) {
  console.error(`[loader] no build recipe for ${process.platform}`)
  process.exit(1)
}

// macOS: like upstream's Makefile, name the SDK explicitly
if (process.platform === 'darwin') {
  const sdk = spawnSync('xcrun', ['--show-sdk-path'], { encoding: 'utf8' })
  if (sdk.status === 0 && sdk.stdout.trim()) {
    build.args.push('-isysroot', sdk.stdout.trim(), `-Wl,-syslibroot,${sdk.stdout.trim()}`)
  }
}

const target = resolve(outDir, build.out)
// the source file comes first and the libraries last, so the linker can resolve them
const flags = build.args.filter((a) => !a.startsWith('-l') && a !== '-framework' && !['IOKit', 'CoreFoundation'].includes(a))
const libs = build.args.filter((a) => !flags.includes(a))
const args = [...flags, '-o', target, source, ...libs]
console.log(`[loader] ${compiler} ${args.join(' ')}`)
const result = spawnSync(compiler, args, { stdio: 'inherit' })
if (result.status !== 0) {
  console.error('[loader] the build failed')
  process.exit(result.status ?? 1)
}
console.log(`[loader] wrote ${target}`)
