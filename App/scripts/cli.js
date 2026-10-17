'use strict'
// Command-line access to a unit over USB (no Electron needed): handy for testing the firmware.
// Run with: npm run cli -- <arguments>   e.g. npm run cli -- COM3 info
//   node scripts/cli.js ports
//   node scripts/cli.js <port> hello
//   node scripts/cli.js <port> info
//   node scripts/cli.js <port> list [/configs]
//   node scripts/cli.js <port> put <localfile> <devicepath>
//   node scripts/cli.js <port> get <devicepath> <localfile>
//   node scripts/cli.js <port> delete <devicepath>
//   node scripts/cli.js <port> reboot [force]      (force releases any active buttons first)
//   node scripts/cli.js <port> bootloader [force]
//   node scripts/cli.js <port> stress <devicepath> <bytes> [rounds]   (upload+download+compare)

const fs = require('node:fs')
const serial = require('../electron/serial')
const { Link } = require('../electron/device')

async function main() {
  const [first, cmd, a, b, c] = process.argv.slice(2)
  if (first === 'ports') {
    for (const p of await serial.listPorts()) console.log(p.path, p.isTeensy ? '(Teensy)' : '', p.manufacturer)
    return
  }
  const port = await serial.open(first)
  const link = new Link(port)
  try {
    if (cmd === 'hello') console.log(await link.hello())
    else if (cmd === 'info') console.log(await link.info())
    else if (cmd === 'list') for (const f of await link.list(a || '/')) console.log(String(f.size).padStart(8), f.name)
    else if (cmd === 'put') await link.put(b, fs.readFileSync(a))
    else if (cmd === 'get') fs.writeFileSync(b, await link.get(a))
    else if (cmd === 'delete') await link.delete(a)
    else if (cmd === 'reboot') await link.reboot(a === 'force')
    else if (cmd === 'bootloader') await link.bootloader(a === 'force')
    else if (cmd === 'stress') {
      const size = Number(b)
      const rounds = Number(c || 5)
      for (let i = 0; i < rounds; i++) {
        const data = require('node:crypto').randomBytes(size)
        let t = Date.now()
        await link.put(a, data)
        const up = Date.now() - t
        t = Date.now()
        const back = await link.get(a)
        const down = Date.now() - t
        const same = Buffer.compare(data, back) === 0
        console.log(`round ${i + 1}: up ${up} ms, down ${down} ms, ${same ? 'identical' : 'MISMATCH'}`)
        if (!same) process.exitCode = 1
      }
      await link.delete(a)
    } else console.log('Unknown command; see the header of src/cli.js')
  } finally {
    port.close()
  }
}

main().catch((err) => {
  console.error('Error:', err.message)
  process.exit(1)
})
