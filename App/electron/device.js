'use strict'
// Host side of the ProtoSerial protocol (see firmware/ProtoSerial/ProtoSerial.ino).
// It only needs an object with `write(buffer, callback)` and 'data' events, so it
// works with a real serialport and with the in-memory fake used by the tests.

const CRC_TABLE = (() => {
  const t = new Uint32Array(256)
  for (let n = 0; n < 256; n++) {
    let c = n
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1
    t[n] = c >>> 0
  }
  return t
})()

function crc32(buf) {
  let c = 0xffffffff
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8)
  return (c ^ 0xffffffff) >>> 0
}

class DeviceError extends Error {
  // true when the unit refused a restart because buttons are active (see REBOOT force)
  get busy() {
    return this.message.startsWith('busy')
  }
}

class Link {
  constructor(port, { lineTimeoutMs = 3000 } = {}) {
    this.port = port
    this.lineTimeoutMs = lineTimeoutMs
    this.buf = Buffer.alloc(0)
    this._notify = null
    this._queue = Promise.resolve()
    port.on('data', (chunk) => {
      this.buf = Buffer.concat([this.buf, chunk])
      if (this._notify) this._notify()
    })
  }

  // Commands must not interleave: each runs after the previous one finishes.
  _exclusive(fn) {
    const run = this._queue.then(fn, fn)
    this._queue = run.catch(() => {})
    return run
  }

  _write(data) {
    return new Promise((resolve, reject) => {
      this.port.write(data, (err) => (err ? reject(err) : resolve()))
    })
  }

  _wait(deadline) {
    return new Promise((resolve, reject) => {
      const ms = deadline - Date.now()
      if (ms <= 0) return reject(new DeviceError('Timed out waiting for the device'))
      const timer = setTimeout(() => {
        this._notify = null
        reject(new DeviceError('Timed out waiting for the device'))
      }, ms)
      this._notify = () => {
        clearTimeout(timer)
        this._notify = null
        resolve()
      }
    })
  }

  async _readLine(timeoutMs = this.lineTimeoutMs) {
    const deadline = Date.now() + timeoutMs
    for (;;) {
      const i = this.buf.indexOf(0x0a)
      if (i >= 0) {
        const line = this.buf.subarray(0, i).toString('utf8').replace(/\r$/, '')
        this.buf = this.buf.subarray(i + 1)
        return line
      }
      await this._wait(deadline)
    }
  }

  async _readBytes(n, timeoutMs) {
    const deadline = Date.now() + timeoutMs
    while (this.buf.length < n) await this._wait(deadline)
    const out = Buffer.from(this.buf.subarray(0, n))
    this.buf = this.buf.subarray(n)
    return out
  }

  // Reads lines until one starts with one of `prefixes`, ignoring the rest. The
  // firmware's log lines start with "#", and its screen driver prints a few
  // unprefixed lines at boot; none of that is part of a reply.
  async _readReply(prefixes, timeoutMs = this.lineTimeoutMs) {
    const deadline = Date.now() + timeoutMs
    for (;;) {
      const line = await this._readLine(Math.max(deadline - Date.now(), 1))
      if (!line.startsWith('#') && prefixes.some((p) => line.startsWith(p))) return line
    }
  }

  // Sends one command line; returns the first reply line, throwing on "ERR ...".
  async _command(line, timeoutMs) {
    this.buf = Buffer.alloc(0) // discard any stale output (e.g. boot messages)
    await this._write(Buffer.from(line + '\n', 'utf8'))
    return this._expectOk(await this._readReply(['OK', 'ERR', 'READY'], timeoutMs))
  }

  _expectOk(reply) {
    if (reply.startsWith('ERR')) throw new DeviceError(reply.slice(4) || 'device error')
    return reply
  }

  hello() {
    return this._exclusive(async () => {
      const reply = await this._command('HELLO')
      const m = /^OK HELLO (\S+) (\d+)(?: sd=(\d))?/.exec(reply)
      if (!m) throw new DeviceError(`Not a MidiToolbox device (replied "${reply}")`)
      return { name: m[1], version: Number(m[2]), sd: m[3] === '1' }
    })
  }

  // Firmware version, which wiring file the unit is using, whether it is in safe mode...
  info() {
    return this._exclusive(async () => {
      const reply = await this._command('INFO')
      try {
        return JSON.parse(reply.slice(3))
      } catch {
        throw new DeviceError(`Unreadable INFO reply "${reply}"`)
      }
    })
  }

  list(dir = '/') {
    return this._exclusive(async () => {
      this.buf = Buffer.alloc(0)
      await this._write(Buffer.from(`LIST ${dir}\n`))
      const files = []
      for (;;) {
        const line = await this._readReply(['F ', 'OK', 'ERR'])
        if (line.startsWith('ERR')) throw new DeviceError(line.slice(4))
        if (line.startsWith('OK')) return files
        const m = /^F (\d+) (.*)$/.exec(line)
        if (m) files.push({ size: Number(m[1]), name: m[2] })
      }
    })
  }

  get(path) {
    return this._exclusive(async () => {
      const reply = await this._command(`GET ${path}`)
      const m = /^OK (\d+) ([0-9A-Fa-f]+)$/.exec(reply)
      if (!m) throw new DeviceError(`Unexpected reply "${reply}"`)
      const size = Number(m[1])
      const data = await this._readBytes(size, 5000 + size / 10)
      if (crc32(data) !== parseInt(m[2], 16)) throw new DeviceError('Download failed CRC check')
      return data
    })
  }

  put(path, data) {
    return this._exclusive(async () => {
      const crc = crc32(data).toString(16)
      const reply = await this._command(`PUT ${data.length} ${crc} ${path}`)
      if (reply !== 'READY') throw new DeviceError(`Unexpected reply "${reply}"`)
      await this._write(data)
      // The device answers once it has written and verified the file.
      this._expectOk(await this._readReply(['OK', 'ERR'], 10000 + data.length / 10))
    })
  }

  delete(path) {
    return this._exclusive(async () => {
      await this._command(`DELETE ${path}`)
    })
  }

  // The device restarts straight after replying, so the port will drop.
  // With buttons active the unit refuses (a restart would leave notes stuck on the
  // synth). `force` makes it release them first, sending the note-offs.
  reboot(force = false) {
    return this._exclusive(async () => {
      await this._command(force ? 'REBOOT force' : 'REBOOT')
    })
  }

  bootloader(force = false) {
    return this._exclusive(async () => {
      await this._command(force ? 'BOOTLOADER force' : 'BOOTLOADER')
    })
  }
}

module.exports = { Link, DeviceError, crc32 }
