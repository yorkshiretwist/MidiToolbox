'use strict'
// Runs the host protocol code against an in-memory imitation of the firmware,
// so framing, file names with spaces and CRC checks are tested without hardware.
// Run with: npm test

const test = require('node:test')
const assert = require('node:assert/strict')
const { EventEmitter } = require('node:events')
const { Link, crc32 } = require('../electron/device')

class FakeDevice extends EventEmitter {
  constructor() {
    super()
    this.files = new Map([['/configs/Live Rig.json', Buffer.from('{"a":1}')]])
    this.inbox = Buffer.alloc(0)
    this.pendingPut = null
    this.corruptNextGet = false
  }

  reply(buf) {
    setImmediate(() => this.emit('data', Buffer.isBuffer(buf) ? buf : Buffer.from(buf)))
  }

  write(data, cb) {
    this.inbox = Buffer.concat([this.inbox, data])
    this.process()
    cb()
  }

  process() {
    for (;;) {
      if (this.pendingPut) {
        const p = this.pendingPut
        if (this.inbox.length < p.size) return
        const body = this.inbox.subarray(0, p.size)
        this.inbox = this.inbox.subarray(p.size)
        this.pendingPut = null
        if (crc32(body) !== p.crc) this.reply('ERR CRC mismatch\n')
        else {
          this.files.set(p.path, Buffer.from(body))
          this.reply('OK\n')
        }
        continue
      }
      const i = this.inbox.indexOf(0x0a)
      if (i < 0) return
      const line = this.inbox.subarray(0, i).toString()
      this.inbox = this.inbox.subarray(i + 1)
      this.handle(line)
    }
  }

  handle(line) {
    const sp = line.indexOf(' ')
    const cmd = sp < 0 ? line : line.slice(0, sp)
    const arg = sp < 0 ? '' : line.slice(sp + 1)
    // the real firmware interleaves log lines ("# ...") and the odd unprefixed driver line
    this.reply('# command received: ' + cmd + '\nsome driver noise\n')
    if (cmd === 'HELLO') this.reply('OK HELLO MidiToolbox 1 sd=1\n')
    else if (cmd === 'INFO') this.reply('OK {"firmware":"0.1.0","safeMode":false}\n')
    else if (cmd === 'LIST') {
      let out = ''
      for (const [path, data] of this.files) {
        const dir = path.slice(0, path.lastIndexOf('/')) || '/'
        if (dir === arg) out += `F ${data.length} ${path.slice(path.lastIndexOf('/') + 1)}\n`
      }
      this.reply(out + 'OK\n')
    } else if (cmd === 'GET') {
      const data = this.files.get(arg)
      if (!data) return this.reply('ERR not found\n')
      const crc = this.corruptNextGet ? 1 : crc32(data)
      this.corruptNextGet = false
      this.reply(Buffer.concat([Buffer.from(`OK ${data.length} ${crc.toString(16).toUpperCase()}\n`), data]))
    } else if (cmd === 'PUT') {
      const m = /^(\d+) ([0-9a-f]+) (.+)$/.exec(arg)
      if (!m) return this.reply('ERR usage\n')
      this.pendingPut = { size: Number(m[1]), crc: parseInt(m[2], 16), path: m[3] }
      this.reply('READY\n')
    } else if (cmd === 'DELETE') {
      this.reply(this.files.delete(arg) ? 'OK\n' : 'ERR not found\n')
    } else this.reply('ERR unknown command\n')
  }
}

test('hello identifies the device', async () => {
  const link = new Link(new FakeDevice())
  assert.deepEqual(await link.hello(), { name: 'MidiToolbox', version: 1, sd: true })
})

test('info parses the firmware JSON', async () => {
  const link = new Link(new FakeDevice())
  assert.deepEqual(await link.info(), { firmware: '0.1.0', safeMode: false })
})

test('log lines and driver noise between replies are ignored', async () => {
  const link = new Link(new FakeDevice())
  assert.deepEqual(await link.list('/configs'), [{ size: 7, name: 'Live Rig.json' }])
  await link.put('/configs/x.json', Buffer.from('{}'))
})

test('list handles file names containing spaces', async () => {
  const link = new Link(new FakeDevice())
  assert.deepEqual(await link.list('/configs'), [{ size: 7, name: 'Live Rig.json' }])
})

test('put then get round-trips arbitrary bytes', async () => {
  const dev = new FakeDevice()
  const link = new Link(dev)
  const data = Buffer.alloc(100000)
  for (let i = 0; i < data.length; i++) data[i] = (i * 31) & 0xff
  await link.put('/configs/Big File.json', data)
  assert.deepEqual(await link.get('/configs/Big File.json'), data)
})

test('get detects corruption', async () => {
  const dev = new FakeDevice()
  const link = new Link(dev)
  dev.corruptNextGet = true
  await assert.rejects(link.get('/configs/Live Rig.json'), /CRC/)
})

test('errors from the device become exceptions', async () => {
  const link = new Link(new FakeDevice())
  await assert.rejects(link.get('/nope.json'), /not found/)
  await assert.rejects(link.delete('/nope.json'), /not found/)
})

test('concurrent commands do not interleave', async () => {
  const link = new Link(new FakeDevice())
  const results = await Promise.all([link.hello(), link.list('/configs'), link.hello()])
  assert.equal(results[0].version, 1)
  assert.equal(results[1].length, 1)
})

test('a silent device times out', async () => {
  const silent = new EventEmitter()
  silent.write = (d, cb) => cb()
  const link = new Link(silent, { lineTimeoutMs: 100 })
  await assert.rejects(link.hello(), /Timed out/)
})
