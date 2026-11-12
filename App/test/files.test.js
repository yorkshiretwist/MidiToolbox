'use strict'
const test = require('node:test')
const assert = require('node:assert/strict')
const { addToRecent, kindForUrl, MAX_RECENT } = require('../electron/files')

test('a reopened file moves to the front without repeating', () => {
  assert.deepEqual(addToRecent(['a', 'b', 'c'], 'b'), ['b', 'a', 'c'])
})

test('the list keeps only the newest ten', () => {
  let list = []
  for (let i = 0; i < 15; i++) list = addToRecent(list, `f${i}`)
  assert.equal(list.length, MAX_RECENT)
  assert.equal(list[0], 'f14')
  assert.equal(list[9], 'f5')
})

test('the page address decides which kind of file it deals with', () => {
  const origin = 'app://local'
  assert.equal(kindForUrl('app://local/editor/', origin), 'editor')
  assert.equal(kindForUrl('app://local/designer/#x', origin), 'designer')
  assert.equal(kindForUrl('app://local/_device/', origin), '')
  assert.equal(kindForUrl('https://evil.example/editor/', origin), '')
})
