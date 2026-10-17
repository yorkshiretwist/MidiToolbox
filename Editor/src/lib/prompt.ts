// A single app-wide "type a value" dialog, the text-entry sibling of lib/confirm.ts. Open it with
// `promptText()` and `await` the result: the trimmed text if the user accepts, or `null` if they
// cancel (Cancel, Escape or a scrim click). The modal itself is rendered once by `PromptDialog.vue`
// (mounted in `App.vue`).

import { reactive } from 'vue'

interface PromptState {
  open: boolean
  title: string
  message: string
  /** Optional lines listed under the message. */
  details: string[]
  /** What is in the box; the dialog edits this directly. */
  value: string
  placeholder: string
  confirmLabel: string
  cancelLabel: string
  /**
   * Optional: what is wrong with the current text, or "" if it is fine. While it returns something
   * the problem is shown under the box and the text cannot be accepted.
   */
  problem: ((text: string) => string) | null
}

const DEFAULTS = {
  title: 'Enter a name',
  message: '',
  details: [] as string[],
  value: '',
  placeholder: '',
  confirmLabel: 'OK',
  cancelLabel: 'Cancel',
  problem: null as ((text: string) => string) | null,
}

export const promptState = reactive<PromptState>({ open: false, ...DEFAULTS })

let resolver: ((text: string | null) => void) | null = null

/**
 * Ask the user for a line of text. The result is trimmed. An empty box cannot be accepted, nor can
 * text that `problem` objects to: the OK button stays disabled until it is fine. A second call
 * while a prompt is still open supersedes the first, resolving it `null`.
 */
export function promptText(
  message: string,
  opts: {
    title?: string
    details?: string[]
    initial?: string
    placeholder?: string
    confirmLabel?: string
    cancelLabel?: string
    problem?: (text: string) => string
  } = {},
): Promise<string | null> {
  resolver?.(null)
  Object.assign(promptState, DEFAULTS, {
    open: true,
    message,
    title: opts.title ?? DEFAULTS.title,
    details: opts.details ?? [],
    value: opts.initial ?? '',
    placeholder: opts.placeholder ?? '',
    confirmLabel: opts.confirmLabel ?? DEFAULTS.confirmLabel,
    cancelLabel: opts.cancelLabel ?? DEFAULTS.cancelLabel,
    problem: opts.problem ?? null,
  })
  return new Promise<string | null>((resolve) => {
    resolver = resolve
  })
}

/** What is wrong with the text in the box right now ("" if nothing). */
export function promptProblem(): string {
  return promptState.problem ? promptState.problem(promptState.value) : ''
}

/** Whether the box holds something that can be accepted. */
export function canAcceptPrompt(): boolean {
  return promptState.value.trim().length > 0 && promptProblem() === ''
}

/**
 * Close the dialog and settle the pending promise: with the trimmed text when `accepted`, else
 * `null`. Accepting text that cannot be accepted does nothing (the dialog stays open).
 */
export function resolvePrompt(accepted: boolean): void {
  if (accepted && !canAcceptPrompt()) return
  const text = accepted ? promptState.value.trim() : null
  Object.assign(promptState, DEFAULTS, { open: false })
  const done = resolver
  resolver = null
  done?.(text)
}
