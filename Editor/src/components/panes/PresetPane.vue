<script setup lang="ts">
// Pane for editing one preset: its name and its list of actions. A preset is a named, reusable
// list of actions that any button can run with an "Execute preset" action (see lib/presets.ts).
import { computed, ref, watch } from 'vue'
import { useConfigStore } from '../../stores/config'
import { confirmDelete } from '../../lib/confirm'
import { newPlayNotesAction } from '../../lib/actionDefaults'
import { hasDuplicateName, isPresetNameTaken } from '../../lib/presets'
import ActionCard from '../ActionCard.vue'

const store = useConfigStore()

const preset = computed(() => store.selectedPreset)

// Only a file that already had two presets with one name can show this: the editor itself no
// longer lets a name be shared (see onName).
const duplicateName = computed(() => (preset.value ? hasDuplicateName(store.config, preset.value) : false))

// What is typed in the name box. Preset names must be unique, so a name another preset has is not
// written to the config: the box keeps what was typed, an error says why, and the preset keeps its
// last good name. Leaving the box with the error still showing puts that good name back.
const nameDraft = ref('')
const nameError = ref('')
watch(
  preset,
  (p) => {
    nameDraft.value = p?.name ?? ''
    nameError.value = ''
  },
  { immediate: true },
)

function onName(event: Event) {
  const value = (event.target as HTMLInputElement).value
  nameDraft.value = value
  if (preset.value && isPresetNameTaken(store.config, value, preset.value)) {
    nameError.value = `Another preset is already called “${value.trim()}”. Preset names must be different.`
    return
  }
  nameError.value = ''
  store.mutate(() => {
    if (preset.value) preset.value.name = value
  })
}

function onNameBlur() {
  if (!nameError.value) return
  nameDraft.value = preset.value?.name ?? ''
  nameError.value = ''
}

function addAction() {
  store.mutate(() => {
    preset.value?.actions.push(newPlayNotesAction())
  })
}

async function deleteAction(index: number) {
  if (!(await confirmDelete('this action'))) return
  store.mutate(() => {
    preset.value?.actions.splice(index, 1)
  })
}

async function deletePreset() {
  if (store.parsed.presetIndex != null) await store.deletePreset(store.parsed.presetIndex)
}
</script>

<template>
  <section v-if="preset" class="pane pane--active" data-level="preset" aria-label="Preset">
    <header class="pane__header">
      <span class="pane__label">Preset:</span>
      <input
        type="text"
        class="pane__name text-input"
        :value="nameDraft"
        aria-label="Preset name"
        placeholder="(unnamed)"
        :aria-invalid="nameError !== ''"
        @input="onName"
        @blur="onNameBlur"
      />
      <button
        type="button"
        class="icon-btn icon-btn--delete"
        aria-label="Delete preset"
        @click="deletePreset"
      >
        &#10005;
      </button>
    </header>

    <p v-if="nameError" class="preset-pane__error" role="alert">{{ nameError }}</p>

    <p v-else-if="duplicateName" class="preset-pane__warning" role="status">
      Another preset already has this name — they will be hard to tell apart when choosing one.
      Rename one of them.
    </p>

    <p class="preset-pane__note">
      Any button can run this preset with an “Execute preset” action. Change it here and every
      button that runs it changes too.
    </p>

    <hr class="pane__divider" />

    <div class="pane__children action-list">
      <ActionCard
        v-for="(action, i) in preset.actions"
        :key="i"
        :action="action"
        @delete="deleteAction(i)"
      />
      <p v-if="preset.actions.length === 0" class="preset-pane__empty">
        A preset needs at least one action.
      </p>
    </div>

    <div class="pane__add">
      <button type="button" class="add-btn" @click="addAction">
        <span class="pane__add-label">Add new action</span>
        <span class="icon-btn icon-btn--add" aria-hidden="true">+</span>
      </button>
    </div>
  </section>
</template>

<style scoped>
.preset-pane__note {
  margin: 0 0 var(--field-gap);
  font-size: var(--font-size-sm);
  color: var(--color-text-muted);
}

.preset-pane__warning {
  margin: 0 0 var(--field-gap);
  padding: var(--space-3);
  border-left: 3px solid var(--color-accent-info);
  border-radius: var(--radius-sm);
  background: rgba(255, 255, 255, 0.06);
  color: var(--color-text);
  font-size: var(--font-size-sm);
  line-height: var(--line-height-normal);
}

.preset-pane__error {
  margin: 0 0 var(--field-gap);
  color: var(--color-accent-danger);
  font-size: var(--font-size-sm);
  font-weight: var(--font-weight-medium);
}

.preset-pane__empty {
  margin: 0;
  color: var(--color-accent-danger);
  font-size: var(--font-size-sm);
}
</style>
