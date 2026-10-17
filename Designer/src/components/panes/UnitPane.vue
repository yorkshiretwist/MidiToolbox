<script setup lang="ts">
// Step 1's sidebar pane: enclosure size, purpose and colour, file Save/Open/Revert, and the example presets.
import { computed, ref } from 'vue'
import NumField from '../NumField.vue'
import Notice from '../Notice.vue'
import PaneShell from './PaneShell.vue'
import { GRID_PITCHES, MAX_UNIT_SIZE, MIN_UNIT_SIZE } from '../../lib/geometry'
import { hexToRgb, rgbToHex } from '../../lib/colour'
import { PRESETS } from '../../lib/presets'
import { REQUIRED_SPACING } from '../../lib/spacing'
import { useDesignStore } from '../../stores/design'

const store = useDesignStore()
const design = computed(() => store.design)
const required = computed(() => REQUIRED_SPACING[design.value.purpose])
const spacing = computed(() => store.analysis.checks.find((c) => c.key === 'spacing')!)
const colourHex = computed(() => rgbToHex(design.value.colour))
const cells = computed(() => `${+(design.value.unit.width / design.value.unit.pitch).toFixed(1)} × ${+(design.value.unit.height / design.value.unit.pitch).toFixed(1)} cells`)

// the drop-down is an action, not a value: show the prompt again after choosing
const example = ref('')
async function chooseExample() {
  const id = example.value
  example.value = ''
  if (id) await store.loadPreset(id)
}
</script>

<template>
  <PaneShell :n="1" title="Unit">
    <p class="pane__hint">The physical size of the enclosure face, drawn to scale. Drag the handles on the unit, or type the sizes.</p>

    <div class="file-actions">
      <button type="button" class="btn btn--save" @click="store.saveFile()">Save</button>
      <button type="button" class="btn btn--revert" :disabled="!store.dirty" @click="store.revert()">Revert</button>
      <button type="button" class="btn btn--open" @click="store.openFile()">Open</button>
    </div>

    <div class="field field--inline">
      <label class="field__label" for="f-example">Start from</label>
      <span class="select">
        <select id="f-example" v-model="example" class="select__native" @change="chooseExample">
          <option value="">Choose an example…</option>
          <option v-for="preset in PRESETS" :key="preset.id" :value="preset.id">{{ preset.label }}</option>
        </select>
      </span>
    </div>

    <div class="field field--inline">
      <label class="field__label" for="f-name">Unit name</label>
      <input id="f-name" class="text-input" :value="design.name" @input="store.setName(($event.target as HTMLInputElement).value)" />
    </div>

    <div class="field">
      <span class="field__label">Controlled with</span>
      <div class="seg" role="radiogroup" aria-label="Controlled with">
        <button type="button" class="btn btn--toggle btn--blue" role="radio" :aria-checked="design.purpose === 'foot'" :aria-pressed="design.purpose === 'foot'" @click="store.setPurpose('foot')">Foot</button>
        <button type="button" class="btn btn--toggle btn--blue" role="radio" :aria-checked="design.purpose === 'finger'" :aria-pressed="design.purpose === 'finger'" @click="store.setPurpose('finger')">Finger</button>
      </div>
      <Notice :kind="spacing.severity === 'warn' ? 'warn' : 'info'">
        <template v-if="spacing.severity === 'warn'">{{ spacing.text }}. You can still export the design.</template>
        <template v-else>
          For {{ design.purpose }} control, keep buttons at least {{ required }} mm apart, measured centre to centre.
        </template>
      </Notice>
    </div>

    <div class="field field--inline">
      <label class="field__label">Width</label>
      <NumField label="Width" :model-value="design.unit.width" :min="MIN_UNIT_SIZE" :max="MAX_UNIT_SIZE" @update:model-value="store.setUnitSize($event, design.unit.height)" />
      <span class="unit-suffix">mm</span>
    </div>
    <div class="field field--inline">
      <label class="field__label">Height</label>
      <NumField label="Height" :model-value="design.unit.height" :min="MIN_UNIT_SIZE" :max="MAX_UNIT_SIZE" @update:model-value="store.setUnitSize(design.unit.width, $event)" />
      <span class="unit-suffix">mm</span>
    </div>
    <div class="field field--inline">
      <label class="field__label" for="f-pitch">Grid pitch</label>
      <span class="select" style="flex: 0 0 6rem">
        <select id="f-pitch" class="select__native" :value="design.unit.pitch" @change="store.setPitch(Number(($event.target as HTMLSelectElement).value))">
          <option v-for="pitch in GRID_PITCHES" :key="pitch" :value="pitch">{{ pitch }} mm</option>
        </select>
      </span>
      <span class="field__readout">{{ cells }}</span>
    </div>

    <div class="field field--inline">
      <label class="field__label" for="f-colour">Unit colour</label>
      <span class="color-swatch" :style="{ '--swatch-color': colourHex }">
        <input id="f-colour" type="color" class="color-swatch__input" :value="colourHex" aria-label="Unit colour" @input="store.setColour(hexToRgb(($event.target as HTMLInputElement).value))" />
      </span>
      <span class="field__readout">{{ colourHex }}</span>
    </div>

    <label class="toggle-field field">
      <input v-model="store.snap" class="toggle-dot" type="checkbox" />
      <span class="toggle-field__label">Snap to grid</span>
    </label>

    <p class="pane__hint">Shrinking the unit never moves anything. Anything left outside is flagged in the checks.</p>
  </PaneShell>
</template>
