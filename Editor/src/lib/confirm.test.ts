// Tests for lib/confirm.ts: the confirmation dialog promises.
import { beforeEach, describe, expect, it } from 'vitest'
import {
  confirmAction,
  confirmDelete,
  confirmState,
  resolveConfirm,
} from './confirm'

beforeEach(() => {
  resolveConfirm(false)
  confirmState.open = false
  confirmState.target = ''
})

describe('confirmDelete', () => {
  it('opens the dialog carrying the target phrase', () => {
    const pending = confirmDelete('this action')
    expect(confirmState.open).toBe(true)
    expect(confirmState.target).toBe('this action')
    resolveConfirm(false)
    return expect(pending).resolves.toBe(false)
  })

  it('resolves true and closes when confirmed', async () => {
    const pending = confirmDelete('the mode “FX”')
    resolveConfirm(true)
    await expect(pending).resolves.toBe(true)
    expect(confirmState.open).toBe(false)
    expect(confirmState.target).toBe('')
  })

  it('resolves false on cancel', async () => {
    const pending = confirmDelete('the patch “Set 1”')
    resolveConfirm(false)
    await expect(pending).resolves.toBe(false)
  })

  it('supersedes an earlier open prompt, resolving it false', async () => {
    const first = confirmDelete('the bank “A”')
    const second = confirmDelete('the bank “B”')
    await expect(first).resolves.toBe(false)
    expect(confirmState.target).toBe('the bank “B”')
    resolveConfirm(true)
    await expect(second).resolves.toBe(true)
  })
})

describe('confirmAction', () => {
  it('shows the message verbatim with overridable title and labels', () => {
    const pending = confirmAction('Move this button?', {
      title: 'Navigation footswitch',
      confirmLabel: 'Move',
      cancelLabel: 'Cancel',
    })
    expect(confirmState.open).toBe(true)
    expect(confirmState.message).toBe('Move this button?')
    expect(confirmState.title).toBe('Navigation footswitch')
    expect(confirmState.confirmLabel).toBe('Move')
    expect(confirmState.cancelLabel).toBe('Cancel')
    resolveConfirm(true)
    return expect(pending).resolves.toBe(true)
  })

  it('resets message and labels back to the delete defaults afterwards', async () => {
    const pending = confirmAction('Something?', { confirmLabel: 'Do it' })
    resolveConfirm(false)
    await pending
    expect(confirmState.message).toBe('')
    expect(confirmState.title).toBe('Are you sure?')
    expect(confirmState.confirmLabel).toBe('Yes')
    expect(confirmState.cancelLabel).toBe('No')
  })
})
