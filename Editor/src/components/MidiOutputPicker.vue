<script setup lang="ts">
// Header control to enable Web MIDI and choose the output port that Play mode sends to.
import { initMidi, midiState, selectOutput } from '../lib/midi'

function onChange(event: Event) {
  const value = (event.target as HTMLSelectElement).value
  selectOutput(value || null)
}
</script>

<template>
  <div class="midi-output" aria-label="MIDI output">
    <span v-if="!midiState.supported" class="midi-output__status">
      MIDI not supported in this browser.
    </span>

    <button
      v-else-if="midiState.permission === 'unknown'"
      type="button"
      class="btn btn--blue"
      @click="initMidi"
    >
      Enable MIDI
    </button>

    <template v-else-if="midiState.permission === 'granted'">
      <span v-if="midiState.outputs.length === 0" class="midi-output__status">
        No MIDI outputs found.
      </span>
      <span v-else class="field field--inline midi-output__picker">
        <label class="field__label" for="midi-output-select">MIDI out</label>
        <span class="select">
          <select
            id="midi-output-select"
            class="select__native"
            :value="midiState.selectedOutputId ?? ''"
            @change="onChange"
          >
            <option v-for="output in midiState.outputs" :key="output.id" :value="output.id">
              {{ output.name }}
            </option>
          </select>
        </span>
      </span>
    </template>

    <span v-else class="midi-output__status midi-output__status--error">
      MIDI access denied.
    </span>
  </div>
</template>

<style scoped>
.midi-output {
  display: flex;
  align-items: center;
  gap: var(--space-3);
}

.midi-output__picker {
  gap: var(--inline-gap);
}

.midi-output__picker .field__label {
  min-width: 0;
}

.midi-output__status {
  font-size: var(--font-size-sm);
  color: var(--color-text-muted);
}

.midi-output__status--error {
  color: var(--color-accent-danger);
}
</style>
