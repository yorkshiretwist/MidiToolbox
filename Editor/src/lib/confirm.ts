// A single app-wide confirmation dialog. Every destructive action in the editor
// — deleting a mode, bank, patch, button or action — is routed through
// `confirmDelete()`, and any other "are you sure?" prompt (e.g. a device-board
// drag that would disturb a navigation footswitch) through `confirmAction()`.
// Both open the modal rendered once by `ConfirmDialog.vue` (mounted in
// `App.vue`) and resolve to `true` only when the user accepts. Callers `await`
// the promise and bail out on `false`.

import { reactive } from 'vue'

interface ConfirmState {
  open: boolean
  /** Phrase dropped into "Are you sure you want to delete ___?" (delete flow). */
  target: string
  /** Full message for a generic prompt; when set it replaces the delete text. */
  message: string
  /** Title bar text; defaults to "Are you sure?". */
  title: string
  /** Accept / cancel button labels; default to "Yes" / "No". */
  confirmLabel: string
  cancelLabel: string
  /** Optional lines listed under the message (e.g. what else a deletion affects). */
  details: string[]
}

const DEFAULTS = {
  target: '',
  message: '',
  title: 'Are you sure?',
  confirmLabel: 'Yes',
  cancelLabel: 'No',
  details: [] as string[],
}

export const confirmState = reactive<ConfirmState>({ open: false, ...DEFAULTS })

let resolver: ((confirmed: boolean) => void) | null = null

function openPrompt(patch: Partial<ConfirmState>): Promise<boolean> {
  resolver?.(false)
  Object.assign(confirmState, DEFAULTS, patch, { open: true })
  return new Promise<boolean>((resolve) => {
    resolver = resolve
  })
}

/**
 * Ask the user to confirm deleting `target` (e.g. `'this action'`, or
 * `'the mode “FX”'`). Resolves `true` for "Yes", `false` for "No", Escape, or a
 * scrim click. A second call while a prompt is still open supersedes the first,
 * resolving it `false`.
 */
export function confirmDelete(target: string): Promise<boolean> {
  return openPrompt({ target })
}

/**
 * Ask the user to confirm an arbitrary action. `message` is shown verbatim; the
 * optional fields override the title bar and button labels. Same resolution
 * rules as `confirmDelete()`.
 */
export function confirmAction(
  message: string,
  opts: {
    title?: string
    confirmLabel?: string
    cancelLabel?: string
    details?: string[]
  } = {},
): Promise<boolean> {
  return openPrompt({
    message,
    title: opts.title ?? DEFAULTS.title,
    confirmLabel: opts.confirmLabel ?? DEFAULTS.confirmLabel,
    cancelLabel: opts.cancelLabel ?? DEFAULTS.cancelLabel,
    details: opts.details ?? [],
  })
}

/** Close the dialog and settle the pending promise. */
export function resolveConfirm(confirmed: boolean): void {
  Object.assign(confirmState, DEFAULTS, { open: false })
  const done = resolver
  resolver = null
  done?.(confirmed)
}
