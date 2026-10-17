<script setup lang="ts">
// The app-wide Yes/No dialog, driven by lib/confirm.ts. Esc = No, Enter = Yes.
import { nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import { confirmState, resolveConfirm } from '../lib/confirm'

// Focused when the dialog opens: the safe default for a destructive prompt.
const noButton = ref<HTMLButtonElement | null>(null)

// Esc / Enter mirror the No / Yes buttons while the dialog is open.
function onKeydown(event: KeyboardEvent) {
  if (!confirmState.open) return
  if (event.key === 'Escape') {
    event.preventDefault()
    resolveConfirm(false)
  } else if (event.key === 'Enter') {
    event.preventDefault()
    resolveConfirm(true)
  }
}

watch(
  () => confirmState.open,
  (open) => {
    if (open) nextTick(() => noButton.value?.focus())
  },
)

onMounted(() => window.addEventListener('keydown', onKeydown))
onUnmounted(() => window.removeEventListener('keydown', onKeydown))
</script>

<template>
  <Teleport to="body">
    <div v-if="confirmState.open" class="overlay overlay--confirm">
      <div class="overlay__scrim" @click="resolveConfirm(false)" />
      <div
        class="dialog"
        role="dialog"
        aria-modal="true"
        aria-labelledby="confirm-title"
      >
        <div id="confirm-title" class="dialog__titlebar">
          {{ confirmState.title }}
        </div>
        <div class="dialog__body">
          <p class="dialog__message">
            <template v-if="confirmState.message">{{ confirmState.message }}</template>
            <template v-else>
              Are you sure you want to delete {{ confirmState.target }}?
            </template>
          </p>
          <div class="dialog__actions">
            <button
              ref="noButton"
              type="button"
              class="btn btn--no"
              @click="resolveConfirm(false)"
            >
              {{ confirmState.cancelLabel }}
            </button>
            <button
              type="button"
              class="btn btn--yes"
              @click="resolveConfirm(true)"
            >
              {{ confirmState.confirmLabel }}
            </button>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>
