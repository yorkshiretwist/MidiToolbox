// Tests for lib/prompt.ts: the "type a value" dialog promise.
import { beforeEach, describe, expect, it } from 'vitest'
import { canAcceptPrompt, promptProblem, promptState, promptText, resolvePrompt } from './prompt'

beforeEach(() => {
  resolvePrompt(false)
  promptState.open = false
})

describe('promptText', () => {
  it('opens the dialog with the message, title, starting text and labels', () => {
    const pending = promptText('Name it', {
      title: 'New thing',
      initial: 'Start',
      placeholder: 'Type here',
      confirmLabel: 'Make',
      details: ['a note'],
    })
    expect(promptState).toMatchObject({
      open: true,
      message: 'Name it',
      title: 'New thing',
      value: 'Start',
      placeholder: 'Type here',
      confirmLabel: 'Make',
      cancelLabel: 'Cancel',
      details: ['a note'],
    })
    resolvePrompt(false)
    return expect(pending).resolves.toBeNull()
  })

  it('resolves with the trimmed text when accepted, and closes', async () => {
    const pending = promptText('Name it')
    promptState.value = '  Chorus  '
    resolvePrompt(true)
    await expect(pending).resolves.toBe('Chorus')
    expect(promptState.open).toBe(false)
    expect(promptState.value).toBe('')
  })

  it('resolves null when cancelled', async () => {
    const pending = promptText('Name it', { initial: 'x' })
    resolvePrompt(false)
    await expect(pending).resolves.toBeNull()
  })

  it('cannot be accepted while the box is empty or only spaces', async () => {
    const pending = promptText('Name it')
    expect(canAcceptPrompt()).toBe(false)
    resolvePrompt(true)
    expect(promptState.open).toBe(true) // still waiting

    promptState.value = '   '
    expect(canAcceptPrompt()).toBe(false)
    resolvePrompt(true)
    expect(promptState.open).toBe(true)

    promptState.value = 'ok'
    expect(canAcceptPrompt()).toBe(true)
    resolvePrompt(true)
    await expect(pending).resolves.toBe('ok')
  })

  it('supersedes an earlier open prompt, resolving it null', async () => {
    const first = promptText('First')
    const second = promptText('Second', { initial: 'b' })
    await expect(first).resolves.toBeNull()
    expect(promptState.message).toBe('Second')
    resolvePrompt(true)
    await expect(second).resolves.toBe('b')
  })

  it('cannot be accepted while the text has a problem, and says what it is', async () => {
    const pending = promptText('Name it', { problem: (t) => (t.trim() === 'taken' ? 'already used' : '') })

    promptState.value = ' taken '
    expect(promptProblem()).toBe('already used')
    expect(canAcceptPrompt()).toBe(false)
    resolvePrompt(true)
    expect(promptState.open).toBe(true) // still waiting

    promptState.value = 'free'
    expect(promptProblem()).toBe('')
    expect(canAcceptPrompt()).toBe(true)
    resolvePrompt(true)
    await expect(pending).resolves.toBe('free')
    expect(promptState.problem).toBeNull()
  })

  it('can still be cancelled while the text has a problem', async () => {
    const pending = promptText('Name it', { initial: 'taken', problem: () => 'always wrong' })
    expect(canAcceptPrompt()).toBe(false)
    resolvePrompt(false)
    await expect(pending).resolves.toBeNull()
  })
})
