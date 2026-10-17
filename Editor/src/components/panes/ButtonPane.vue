<script setup lang="ts">
// Pane for editing one button: name, colour, mode, page and its list of actions.
import { computed, ref, watch } from 'vue'
import { useConfigStore } from '../../stores/config'
import { useDeviceStore } from '../../stores/device'
import { BUTTON_LEVEL_LABEL, effectiveColourCode, navButtonDefaultRole, rgbCss } from '../../lib/buttons'
import { confirmDelete } from '../../lib/confirm'
import { newPlayNotesAction } from '../../lib/actionDefaults'
import { canCreatePresetFrom, runsAnyPreset } from '../../lib/presets'
import { hexToRgb, rgbToHex } from '../../lib/colour'
import ActionCard from '../ActionCard.vue'
import type { ButtonMode } from '../../types/config'

const store = useConfigStore()
const deviceStore = useDeviceStore()

const button = computed(() => store.selectedButton)

// Which container (File/Mode/Bank/Patch) this button is actually defined on —
// only a Patch-owned button is page-filtered (see the Page field below).
const ownerLevel = computed(() => store.parsed.buttonOwnerLevel)
const isPatchOwned = computed(() => ownerLevel.value === 'patch')

// The active device's three navigation footswitches carry a built-in
// navigation role. By the time this pane is showing one of them, the
// stronger confirmation dialog in activateButtonAtLevel() (stores/config.ts)
// has already been accepted — this is a standing reminder of what it
// replaced, not a warning before the fact.
const overriddenNavRole = computed(() =>
  button.value != null ? navButtonDefaultRole(deviceStore.profile, button.value.id) : undefined,
)

const swatch = computed(() => rgbCss(effectiveColourCode(button.value, store.activeBank)))

const colourHex = computed(() => rgbToHex(effectiveColourCode(button.value, store.activeBank)))

const isExclusive = computed(() => button.value?.buttonMode === 'Exclusive')

// One entry per existing page plus one more, so the button can be moved onto a
// brand-new page. Always shown, even for a single-page patch (offers 1 and 2).
const pageOptions = computed(() =>
  Array.from({ length: store.pageCount + 1 }, (_, i) => i + 1),
)

const pageConflict = ref('')
watch(button, () => {
  pageConflict.value = ''
})

function onPage(event: Event) {
  const select = event.target as HTMLSelectElement
  const value = Number(select.value)
  const result = store.setButtonPage(value)
  if (result.ok) {
    pageConflict.value = ''
    return
  }
  // Blocked: (id, page) is already taken. Warn and put the dropdown back.
  const name = result.conflict?.name?.trim()
  pageConflict.value = result.conflict
    ? `Footswitch ${(button.value?.id ?? 0) + 1} already has a button on page ${value}` +
      (name ? ` (“${name}”)` : '') +
      '. Move or remove that one first.'
    : 'Could not move this button to that page.'
  select.value = String(button.value?.page ?? 1)
}

const activateOnLoad = computed({
  get: () => button.value?.defaultStatus === 'On',
  set: (on: boolean) =>
    store.mutate(() => {
      if (!button.value) return
      if (on) button.value.defaultStatus = 'On'
      else delete button.value.defaultStatus
    }),
})

function onName(event: Event) {
  const value = (event.target as HTMLInputElement).value
  store.mutate(() => {
    if (button.value) button.value.name = value
  })
}

function onMode(event: Event) {
  const value = (event.target as HTMLSelectElement).value as ButtonMode
  store.mutate(() => {
    if (button.value) {
      button.value.buttonMode = value
      // An Exclusive button is always a member of its own set.
      if (value === 'Exclusive' && !button.value.buttonSet?.includes(button.value.id)) {
        button.value.buttonSet = [...(button.value.buttonSet ?? []), button.value.id].sort(
          (a, b) => a - b,
        )
      }
    }
  })
  // "Button set" only applies to Exclusive; leaving it abandons any draft.
  if (value !== 'Exclusive') store.discardButtonSetDraft()
}

function onColour(event: Event) {
  const rgb = hexToRgb((event.target as HTMLInputElement).value)
  store.mutate(() => {
    if (button.value) button.value.colourCode = rgb
  })
}

// "Create preset" moves every action into a new preset and leaves one Execute preset action. A
// preset can't run another preset, so the choice isn't offered at all for a button that already
// runs one (the player has to reorganise that by hand). With no actions yet it is shown but
// disabled, since there is nothing to move.
const offerCreatePreset = computed(() => (button.value ? !runsAnyPreset(button.value) : false))
const canCreatePreset = computed(() => (button.value ? canCreatePresetFrom(button.value) : false))

const createPresetTitle = computed(() =>
  canCreatePreset.value
    ? "Move this button's actions into a new preset"
    : 'Add an action first: a preset needs at least one',
)

async function createPreset() {
  await store.createPresetFromSelectedButton()
}

function addAction() {
  store.mutate(() => {
    button.value?.actions.push(newPlayNotesAction())
  })
}

async function deleteButton() {
  const name = button.value?.name?.trim()
  const ok = await confirmDelete(name ? `the button “${name}”` : 'this button')
  if (ok) store.deleteSelectedButton()
}

async function deleteAction(index: number) {
  if (!(await confirmDelete('this action'))) return
  store.mutate(() => {
    button.value?.actions.splice(index, 1)
  })
}
</script>

<template>
  <section
    v-if="button"
    class="pane pane--active"
    data-level="button"
    aria-label="Button"
  >
    <p v-if="overriddenNavRole" class="pane__warning" role="status">
      This overrides footswitch {{ button.id + 1 }}'s built-in role ({{ overriddenNavRole }}) on the
      physical device — the on-screen parameter buttons still work as normal.
    </p>

    <header class="pane__header">
      <span class="pane__label">Button:</span>
      <input
        type="text"
        class="pane__name text-input"
        :value="button.name"
        aria-label="Button name"
        placeholder="(unnamed)"
        @input="onName"
      />
      <button
        type="button"
        class="icon-btn icon-btn--delete"
        aria-label="Delete button"
        @click="deleteButton"
      >
        &#10005;
      </button>
    </header>

    <p class="button-level-badge">
      Defined at <strong>{{ BUTTON_LEVEL_LABEL[ownerLevel!] }}</strong> level
      <span v-if="!isPatchOwned">— applies to every page, unless a Patch overrides it</span>
    </p>

    <div class="pane__fields">
      <div class="field swatch-field">
        <label class="color-swatch" :style="{ '--swatch-color': swatch }">
          <input
            type="color"
            class="color-swatch__input"
            :value="colourHex"
            aria-label="Active colour"
            @input="onColour"
          />
        </label>
        <span class="swatch-field__label">Active colour</span>
      </div>

      <div class="field toggle-field">
        <input
          id="activate-on-load"
          v-model="activateOnLoad"
          type="checkbox"
          class="toggle-dot"
        />
        <label for="activate-on-load" class="toggle-field__label">
          Activate on patch load
        </label>
      </div>

      <div class="field field--inline">
        <span class="select">
          <select
            class="select__native"
            aria-label="Button mode"
            :value="button.buttonMode"
            @change="onMode"
          >
            <option value="Momentary">Momentary</option>
            <option value="Latched">Latched</option>
            <option value="Exclusive">Exclusive</option>
          </select>
        </span>
        <button
          v-if="isExclusive"
          type="button"
          class="btn btn--toggle"
          :class="store.buttonSetEditing ? 'btn--save' : 'btn--blue'"
          :aria-pressed="store.buttonSetEditing"
          data-controls="button-board"
          @click="store.toggleButtonSetEditing()"
        >
          {{ store.buttonSetEditing ? 'Save' : 'Button set' }}
        </button>
      </div>

      <div v-if="isPatchOwned" class="field field--inline">
        <label class="field__label" for="button-page">Page</label>
        <span class="select">
          <select
            id="button-page"
            class="select__native"
            :value="button.page ?? 1"
            @change="onPage"
          >
            <option v-for="n in pageOptions" :key="n" :value="n">{{ n }}</option>
          </select>
        </span>
      </div>

      <p v-if="isPatchOwned && pageConflict" class="page-conflict" role="alert">{{ pageConflict }}</p>
    </div>

    <hr class="pane__divider" />

    <div class="pane__children action-list">
      <ActionCard
        v-for="(action, i) in button.actions"
        :key="i"
        :action="action"
        @delete="deleteAction(i)"
      />
      <p v-if="button.actions.length === 0" class="button-pane__empty">
        No actions yet — this button won't be saved until it has one.
      </p>
    </div>

    <div class="pane__add">
      <button
        v-if="offerCreatePreset"
        type="button"
        class="text-btn pane__add-start"
        :disabled="!canCreatePreset"
        :title="createPresetTitle"
        @click="createPreset"
      >
        <!-- stacked layers: several actions gathered into one preset -->
        <svg class="text-btn__icon" viewBox="0 0 24 24" aria-hidden="true">
          <path d="M12 2 2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5" />
        </svg>
        Create preset?
      </button>
      <button type="button" class="add-btn" @click="addAction">
        <span class="pane__add-label">Add new action</span>
        <span class="icon-btn icon-btn--add" aria-hidden="true">+</span>
      </button>
    </div>
  </section>
</template>

<style scoped>
.button-level-badge {
  margin: 0 0 var(--field-gap);
  font-size: var(--font-size-sm);
  color: var(--color-text-muted);
}

.pane__warning {
  margin: 0 0 var(--field-gap);
  padding: var(--space-3);
  border-left: 3px solid var(--color-accent-info);
  border-radius: var(--radius-sm);
  background: rgba(255, 255, 255, 0.06);
  color: var(--color-text);
  font-size: var(--font-size-sm);
  line-height: var(--line-height-normal);
}

.button-pane__empty {
  margin: 0;
  color: var(--color-text-muted);
  font-size: var(--font-size-sm);
}

.page-conflict {
  margin: 0;
  color: var(--color-accent-danger);
  font-size: var(--font-size-sm);
  font-weight: var(--font-weight-medium);
}
</style>
