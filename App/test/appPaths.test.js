'use strict'
const test = require('node:test')
const assert = require('node:assert/strict')
const path = require('node:path')
const { resolveAppPath, mimeTypeFor } = require('../electron/appPaths')

const web = path.resolve('/srv/web')
const ui = path.resolve('/srv/ui')
const at = (pathname) => resolveAppPath(pathname, web, ui)

test('the root and folders get their index.html', () => {
  assert.equal(at('/'), path.join(web, 'index.html'))
  assert.equal(at('/designer/'), path.join(web, 'designer', 'index.html'))
})

test('files resolve inside the web folder', () => {
  assert.equal(at('/assets/app.js'), path.join(web, 'assets', 'app.js'))
  assert.equal(at('/designer/assets/x.css'), path.join(web, 'designer', 'assets', 'x.css'))
})

test('the Device tab is served from its own folder', () => {
  assert.equal(at('/_device/'), path.join(ui, 'index.html'))
  assert.equal(at('/_device/app.js'), path.join(ui, 'app.js'))
  assert.equal(at('/_device/style.css'), path.join(ui, 'style.css'))
})

test('nothing outside the two folders can be reached', () => {
  assert.equal(at('/../secret.txt'), null)
  assert.equal(at('/assets/../../secret.txt'), null)
  assert.equal(at('/%2e%2e/secret.txt'), null)
  assert.equal(at('/_device/../secret.txt'), null)
  assert.equal(at('/_device/%2e%2e/%2e%2e/secret'), null)
  assert.equal(at('/a\\..\\b'), null)
  assert.equal(at('/bad%00name'), null)
  assert.equal(at('/%E0%A4%A'), null) // malformed escape
})

test('mime types', () => {
  assert.match(mimeTypeFor('a.html'), /text\/html/)
  assert.match(mimeTypeFor('a.js'), /javascript/)
  assert.match(mimeTypeFor('A.CSS'), /text\/css/)
  assert.equal(mimeTypeFor('a.svg'), 'image/svg+xml')
  assert.equal(mimeTypeFor('a.unknown'), 'application/octet-stream')
})
