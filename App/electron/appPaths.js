'use strict'
// Which URL path maps to which file, for the app:// protocol. Pure functions (no Electron), so
// they can be tested, and the one place that decides what the app will ever serve.

const path = require('node:path')

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.woff2': 'font/woff2',
  '.woff': 'font/woff',
  '.txt': 'text/plain; charset=utf-8',
  '.map': 'application/json; charset=utf-8',
}

function mimeTypeFor(file) {
  return MIME_TYPES[path.extname(file).toLowerCase()] || 'application/octet-stream'
}

// The Device tab lives under this prefix; everything else is the built Editor/Designer site.
const DEVICE_PREFIX = '/_device/'

/**
 * Maps a URL pathname (e.g. "/designer/" or "/assets/app.js") to a file inside `webDir` or
 * `deviceUiDir`, or null if it would leave them. A path that ends in "/" (or is a folder) gets
 * its index.html, which is what makes "/designer/" work.
 */
function resolveAppPath(pathname, webDir, deviceUiDir) {
  let decoded
  try {
    decoded = decodeURIComponent(pathname)
  } catch {
    return null
  }
  if (decoded.includes('\0') || decoded.includes('\\')) return null

  let base = webDir
  let relative = decoded
  if (decoded.startsWith(DEVICE_PREFIX)) {
    base = deviceUiDir
    relative = decoded.slice(DEVICE_PREFIX.length - 1) // keep the leading "/"
  }
  if (relative.endsWith('/')) relative += 'index.html'

  const resolved = path.resolve(base, '.' + relative)
  const inside = path.relative(base, resolved)
  if (inside.startsWith('..') || path.isAbsolute(inside)) return null
  return resolved
}

module.exports = { MIME_TYPES, DEVICE_PREFIX, mimeTypeFor, resolveAppPath }
