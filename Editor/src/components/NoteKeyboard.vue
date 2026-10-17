<script setup lang="ts">
// A piano keyboard popup for choosing the notes of a PlayNotes action.
import { computed, onMounted, onUnmounted, ref } from 'vue'
import {
  PIANO_MAX_MIDI,
  PIANO_MIN_MIDI,
  isBlackKey,
  midiSetToNoteNames,
  midiToNoteName,
  noteNamesToMidiSet,
} from '../lib/notes'

const props = defineProps<{ notes: string[] }>()
const emit = defineEmits<{ (e: 'save', notes: string[]): void; (e: 'cancel'): void }>()

// The selection the keyboard is working on. Seeded from the notes that were in
// the textbox when it opened; "Revert" throws this away and closes.
const selected = ref(noteNamesToMidiSet(props.notes))

function isSelected(midi: number): boolean {
  return selected.value.has(midi)
}

function toggle(midi: number): void {
  // Replace the Set so the computed key classes re-evaluate.
  const next = new Set(selected.value)
  if (next.has(midi)) next.delete(midi)
  else next.add(midi)
  selected.value = next
}

// White keys carry the layout; each one hosts the black key that sits to its
// right (when there is one within range), positioned to straddle the gap.
interface WhiteKey {
  midi: number
  label: string | null
  black: number | null
}

const whiteKeys = computed<WhiteKey[]>(() => {
  const keys: WhiteKey[] = []
  for (let midi = PIANO_MIN_MIDI; midi <= PIANO_MAX_MIDI; midi++) {
    if (isBlackKey(midi)) continue
    const next = midi + 1
    keys.push({
      midi,
      // Label only the Cs, as an octave marker — keeps the keybed uncluttered.
      label: midi % 12 === 0 ? midiToNoteName(midi) : null,
      black: next <= PIANO_MAX_MIDI && isBlackKey(next) ? next : null,
    })
  }
  return keys
})

function save(): void {
  emit('save', midiSetToNoteNames(selected.value))
}

function onKeydown(event: KeyboardEvent): void {
  if (event.key === 'Escape') emit('cancel')
}

onMounted(() => window.addEventListener('keydown', onKeydown))
onUnmounted(() => window.removeEventListener('keydown', onKeydown))
</script>

<template>
  <Teleport to="body">
    <div
      class="note-keyboard"
      role="dialog"
      aria-modal="true"
      aria-label="Note selector"
    >
      <div class="note-keyboard__scrim" @click="emit('cancel')" />

      <div class="note-keyboard__panel">
        <div class="note-keyboard__keys" role="group" aria-label="Keyboard">
          <div
            v-for="key in whiteKeys"
            :key="key.midi"
            class="pkey pkey--white"
            :class="{ 'pkey--on': isSelected(key.midi) }"
            :aria-pressed="isSelected(key.midi)"
            :aria-label="midiToNoteName(key.midi)"
            role="button"
            tabindex="0"
            @click="toggle(key.midi)"
            @keydown.enter.prevent="toggle(key.midi)"
            @keydown.space.prevent="toggle(key.midi)"
          >
            <span v-if="key.label" class="pkey__label">{{ key.label }}</span>
            <span
              v-if="key.black !== null"
              class="pkey pkey--black"
              :class="{ 'pkey--on': isSelected(key.black) }"
              :aria-pressed="isSelected(key.black)"
              :aria-label="midiToNoteName(key.black)"
              role="button"
              tabindex="0"
              @click.stop="toggle(key.black)"
              @keydown.enter.stop.prevent="toggle(key.black)"
              @keydown.space.stop.prevent="toggle(key.black)"
            />
          </div>
        </div>

        <div class="note-keyboard__actions">
          <button type="button" class="btn btn--revert" @click="emit('cancel')">
            Revert
          </button>
          <button type="button" class="btn btn--save" @click="save">Save</button>
        </div>
      </div>
    </div>
  </Teleport>
</template>
