<script setup lang="ts">
// Step 2's sidebar pane: add, remove, number and quick-grid the unit's buttons and set their LED side.
import { computed, ref } from 'vue'
import CheckList from '../CheckList.vue'
import NumField from '../NumField.vue'
import Notice from '../Notice.vue'
import PaneShell from './PaneShell.vue'
import { buttonDiameter, buttonInsideUnit } from '../../lib/geometry'
import { navRoleOf, performanceButtons } from '../../lib/layout'
import { useDesignStore } from '../../stores/design'
import type { LedSide, Numbering } from '../../types/device'

const store = useDesignStore()
const design = computed(() => store.design)
const perf = computed(() => performanceButtons(design.value))
const checks = computed(() => store.analysis.checks.filter((c) => c.step === 2))
const spacing = computed(() => store.analysis.checks.find((c) => c.key === 'spacing')!)
const selected = computed(() => store.selected)
const role = computed(() => (selected.value ? navRoleOf(design.value, selected.value.uid) : undefined))
// Buttons outside the unit can sit under the sidebar or off the panel, where they
// cannot be clicked, so they are listed here to select (and then delete or move back).
const outside = computed(() =>
  design.value.buttons.filter((b) => !buttonInsideUnit(b, design.value.unit, buttonDiameter(design.value.purpose))),
)
const manual = computed(() => design.value.numbering === 'manual')

const NUMBERING: { value: Numbering; label: string }[] = [
  { value: 'bottom-left', label: 'Bottom-left → top-right' },
  { value: 'top-left', label: 'Top-left → bottom-right' },
  { value: 'manual', label: 'Manual (edit the ids)' },
]

const grid = ref({ columns: 5, rows: 2, spacing: 70 })
const addFailed = ref(false)
function add() {
  addFailed.value = store.addButton() === null
}
function generate() {
  return store.generateGrid({ ...grid.value, grow: true })
}
</script>

<template>
  <PaneShell :n="2" title="Buttons">
    <div class="summary">
      <span class="summary__big">{{ design.buttons.length }} buttons</span>
      <span class="summary__detail">{{ perf.length }} performance · 3 navigation</span>
    </div>
    <CheckList :checks="checks" />
    <Notice v-if="spacing.severity === 'warn'" kind="warn">{{ spacing.text }}.</Notice>
    <div v-if="outside.length > 0" class="field field--inline" style="flex-wrap: wrap">
      <span class="field__label" style="min-width: 0">Outside the unit</span>
      <button
        v-for="b in outside"
        :key="b.uid"
        type="button"
        class="btn btn--small"
        :class="b.uid === selected?.uid ? 'btn--blue' : 'btn--dark'"
        :aria-pressed="b.uid === selected?.uid"
        :aria-label="`Select button ${b.id + 1}`"
        @click="store.select(b.uid)"
      >{{ b.id + 1 }}</button>
    </div>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <template v-if="selected">
      <div class="pane__header" style="margin-bottom: 0">
        <span class="pane__label">Button {{ selected.id + 1 }}</span>
        <span class="field__readout" style="margin-right: auto; margin-left: var(--space-2)">{{ role ? 'Navigation' : 'Performance' }}</span>
        <button type="button" class="icon-btn icon-btn--delete" aria-label="Delete button" title="Delete this button" @click="store.deleteButton(selected.uid)">X</button>
      </div>
      <p v-if="role" class="pane__hint">Deleting a navigation button hands its role to the nearest performance button.</p>

      <div class="field field--inline">
        <label class="field__label">Button id</label>
        <NumField label="Button id" :model-value="selected.id" :min="0" :disabled="!manual" @update:model-value="store.setButtonId(selected.uid, $event)" />
        <span class="field__readout">{{ manual ? `printed “${selected.id + 1}”` : 'set by numbering' }}</span>
      </div>
      <div class="field field--inline">
        <label class="field__label">Across</label>
        <NumField label="Across (mm)" :model-value="selected.x" :min="0" @update:model-value="store.moveButton(selected.uid, $event, selected.y, true)" />
        <span class="unit-suffix">mm from the left</span>
      </div>
      <div class="field field--inline">
        <label class="field__label">Down</label>
        <NumField label="Down (mm)" :model-value="selected.y" :min="0" @update:model-value="store.moveButton(selected.uid, selected.x, $event, true)" />
        <span class="unit-suffix">mm from the top</span>
      </div>
      <div class="field field--inline">
        <label class="field__label" for="f-led">LED</label>
        <span class="select">
          <select id="f-led" class="select__native" :value="selected.led" @change="store.setLed(selected.uid, ($event.target as HTMLSelectElement).value as LedSide)">
            <option value="above">Above the button</option>
            <option value="below">Below the button</option>
          </select>
        </span>
      </div>
    </template>
    <p v-else class="pane__hint">Click a button on the unit to edit it, then press Delete to remove it.</p>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <div class="pane__add" style="margin-top: 0; justify-content: space-between">
      <span class="pane__hint">Double-click an empty spot on the unit, or</span>
      <span style="display: inline-flex; align-items: center; gap: var(--inline-gap)">
        Add button
        <button type="button" class="icon-btn icon-btn--add" aria-label="Add button" @click="add">+</button>
      </span>
    </div>
    <Notice v-if="addFailed" kind="warn">There is no free spot left on the grid. Make the unit bigger or move something.</Notice>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <span class="field__label">Quick layout</span>
    <div class="field field--inline">
      <label class="field__label" style="min-width: 0">Columns</label>
      <NumField v-model="grid.columns" label="Columns" :min="1" :max="30" style="flex-basis: 3.5rem" />
      <label class="field__label" style="min-width: 0">Rows</label>
      <NumField v-model="grid.rows" label="Rows" :min="1" :max="30" style="flex-basis: 3.5rem" />
    </div>
    <div class="field field--inline">
      <label class="field__label" style="min-width: 0">Centres</label>
      <NumField v-model="grid.spacing" label="Centre spacing (mm)" :min="10" :max="500" style="flex-basis: 4rem" />
      <span class="unit-suffix">mm apart</span>
      <button type="button" class="btn btn--blue btn--small" style="margin-left: auto" @click="generate">Generate</button>
    </div>
    <p class="pane__hint">Replaces the performance buttons with a regular grid along the bottom, and grows the unit if it needs to.</p>

    <div class="field field--inline">
      <label class="field__label" for="f-num">Numbering</label>
      <span class="select">
        <select id="f-num" class="select__native" :value="design.numbering" @change="store.setNumbering(($event.target as HTMLSelectElement).value as Numbering)">
          <option v-for="option in NUMBERING" :key="option.value" :value="option.value">{{ option.label }}</option>
        </select>
      </span>
    </div>
    <p class="pane__hint">Ids are what performance configs refer to, so renumbering buttons later changes which button each config action belongs to.</p>
  </PaneShell>
</template>
