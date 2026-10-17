<script setup lang="ts">
// The app-wide "type a name" dialog, driven by lib/prompt.ts. Esc = Cancel, Enter = OK.
import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import { canAcceptPrompt, promptProblem, promptState, resolvePrompt } from '../lib/prompt'

const input = ref<HTMLInputElement | null>(null)

// recomputed as the text changes (both read promptState.value)
const problem = computed(() => promptProblem())
const canAccept = computed(() => canAcceptPrompt())

// Esc / Enter mirror the Cancel / OK buttons while the dialog is open.
function onKeydown(event: KeyboardEvent) {
  if (!promptState.open) return
  if (event.key === 'Escape') {
    event.preventDefault()
    resolvePrompt(false)
  } else if (event.key === 'Enter') {
    event.preventDefault()
    if (canAcceptPrompt()) resolvePrompt(true)
  }
}

// Ready to type straight away when the dialog opens.
watch(
  () => promptState.open,
  (open) => {
    if (open) {
      nextTick(() => {
        input.value?.focus()
        input.value?.select()
      })
    }
  },
)

onMounted(() => window.addEventListener('keydown', onKeydown))
onUnmounted(() => window.removeEventListener('keydown', onKeydown))
</script>

<template>
  <Teleport to="body">
    <div v-if="promptState.open" class="overlay overlay--prompt">
      <div class="overlay__scrim" @click="resolvePrompt(false)" />
      <div class="dialog" role="dialog" aria-modal="true" aria-labelledby="prompt-title">
        <div id="prompt-title" class="dialog__titlebar">{{ promptState.title }}</div>
        <div class="dialog__body">
          <p v-if="promptState.message" class="dialog__message">{{ promptState.message }}</p>
          <ul v-if="promptState.details.length" class="dialog__details">
            <li v-for="(line, i) in promptState.details" :key="i">{{ line }}</li>
          </ul>
          <input
            ref="input"
            v-model="promptState.value"
            type="text"
            class="text-input dialog__input"
            :placeholder="promptState.placeholder"
            :aria-label="promptState.title"
            :aria-invalid="problem !== ''"
            autocomplete="off"
          />
          <p v-if="problem" class="dialog__problem" role="alert">{{ problem }}</p>
          <div class="dialog__actions">
            <button type="button" class="btn btn--no" @click="resolvePrompt(false)">
              {{ promptState.cancelLabel }}
            </button>
            <button
              type="button"
              class="btn btn--yes"
              :disabled="!canAccept"
              @click="resolvePrompt(true)"
            >
              {{ promptState.confirmLabel }}
            </button>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>
