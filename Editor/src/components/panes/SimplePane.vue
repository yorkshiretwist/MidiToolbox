<script setup lang="ts">
// Pane shared by the Mode and Bank levels: name, colour, children list, and the buttons defined or inherited there.
import { computed } from 'vue'
import { useConfigStore } from '../../stores/config'
import { bankPath, filePath, modePath, patchPath } from '../../lib/selection'
import { BUTTON_LEVEL_LABEL, effectiveColourCode, rgbCss } from '../../lib/buttons'
import { hexToRgb, rgbToHex } from '../../lib/colour'
import { useDragReorder } from '../../lib/useDragReorder'
import type { Bank, Patch } from '../../types/config'

const props = defineProps<{ level: 'mode' | 'bank' }>()

const store = useConfigStore()

// Drag a bank (on a Mode) or a patch (on a Bank) to a new position in the list
// (lib/useDragReorder.ts). `meta.parent` is the list's owner.
const drag = useDragReorder((from, to) => {
  if (meta.value) store.moveItem(meta.value.parent, from, to)
}, 'move')

// Buttons defined directly on this Mode/Bank, plus any it inherits from a
// less-specific level (Bank inherits from Mode/Config; Mode from Config) and
// hasn't overridden itself. Clicking either opens/creates the button at this
// level — an inherited one is copied down first, after confirming (see
// activateButtonAtLevel() in the store).
const buttonSections = computed(() => store.buttonEditor(props.level))

function pressButton(id: number) {
  store.activateButtonAtLevel(props.level, id)
}

const meta = computed(() => {
  const p = store.parsed
  if (props.level === 'mode' && p.modeIndex != null) {
    const mode = store.config.modes[p.modeIndex]
    return {
      title: 'Mode:',
      name: mode.name,
      editable: true,
      childLabel: 'bank',
      childListLabel: 'Banks',
      parent: [p.modeIndex!],
      children: mode.banks.map((b: Bank, i: number) => ({
        name: b.name || '(unnamed bank)',
        select: () => store.select(bankPath(p.modeIndex!, i)),
      })),
      add: () => {
        store.mutate(() =>
          mode.banks.push({
            name: 'New bank',
            patches: [{ name: 'New patch', buttons: [] }],
          }),
        )
        store.select(bankPath(p.modeIndex!, mode.banks.length - 1))
      },
      remove: async () => {
        const name = mode.name.trim()
        // A config must keep at least one mode (schema minItems: 1). Removing
        // the last one is allowed — validation then flags the empty config.
        // Set actions pointing into the mode are listed and removed (see removeItem).
        if (!(await store.removeItem([p.modeIndex!], name ? `the mode “${name}”` : 'this mode')))
          return
        store.select(filePath())
      },
    }
  }
  if (props.level === 'bank' && p.modeIndex != null && p.bankIndex != null) {
    const mode = store.config.modes[p.modeIndex]
    const bank = mode.banks[p.bankIndex]
    return {
      title: 'Bank:',
      name: bank.name,
      editable: true,
      childLabel: 'patch',
      childListLabel: 'Patches',
      parent: [p.modeIndex!, p.bankIndex!],
      children: bank.patches.map((pt: Patch, i: number) => ({
        name: pt.name || '(unnamed patch)',
        select: () => store.select(patchPath(p.modeIndex!, p.bankIndex!, i)),
      })),
      add: () => {
        store.mutate(() => bank.patches.push({ name: 'New patch', buttons: [] }))
        store.select(
          patchPath(p.modeIndex!, p.bankIndex!, bank.patches.length - 1),
        )
      },
      remove: async () => {
        const name = bank.name.trim()
        // A mode must keep at least one bank (schema minItems: 1). Removing the
        // last one is allowed — validation then flags the empty mode.
        if (!(await store.removeItem([p.modeIndex!, p.bankIndex!], name ? `the bank “${name}”` : 'this bank')))
          return
        store.select(modePath(p.modeIndex!))
      },
    }
  }
  return null
})

// Sub-header colour is a Bank-only field — the colour buttons in that bank
// fall back to when they don't set their own `colourCode` (see effectiveColourCode).
const bank = computed(() => {
  if (props.level !== 'bank') return undefined
  const p = store.parsed
  if (p.modeIndex == null || p.bankIndex == null) return undefined
  return store.config.modes[p.modeIndex].banks[p.bankIndex]
})

const swatch = computed(() => rgbCss(bank.value?.subheaderColourCode))
const colourHex = computed(() => rgbToHex(bank.value?.subheaderColourCode))

function onColour(event: Event) {
  const rgb = hexToRgb((event.target as HTMLInputElement).value)
  store.mutate(() => {
    if (bank.value) bank.value.subheaderColourCode = rgb
  })
}

// 'Grid' is the default, so it's stored as the key being absent — existing
// files stay unchanged until a bank opts in to the Live layout.
function onScreenLayout(event: Event) {
  const value = (event.target as HTMLSelectElement).value
  store.mutate(() => {
    if (!bank.value) return
    if (value === 'Live') bank.value.screenLayout = 'Live'
    else delete bank.value.screenLayout
  })
}

function onName(event: Event) {
  const value = (event.target as HTMLInputElement).value
  const p = store.parsed
  store.mutate((c) => {
    if (props.level === 'mode' && p.modeIndex != null) {
      c.modes[p.modeIndex].name = value
    } else if (
      props.level === 'bank' &&
      p.modeIndex != null &&
      p.bankIndex != null
    ) {
      c.modes[p.modeIndex].banks[p.bankIndex].name = value
    }
  })
}
</script>

<template>
  <section
    v-if="meta"
    :ref="drag.paneEl"
    class="pane pane--active"
    :data-level="props.level"
    :aria-label="props.level"
    @dragover="drag.onDragOverPane"
    @drop.prevent="drag.endDrag"
    @dragend="drag.endDrag"
  >
    <header class="pane__header">
      <span class="pane__label">{{ meta.title }}</span>
      <input
        v-if="meta.editable"
        type="text"
        class="pane__name text-input"
        :value="meta.name"
        :aria-label="`${props.level} name`"
        @input="onName"
      />
      <span v-else class="pane__name">{{ meta.name }}</span>
      <button
        type="button"
        class="icon-btn icon-btn--delete"
        :aria-label="`Delete ${props.level}`"
        @click="meta.remove()"
      >
        &#10005;
      </button>
    </header>

    <div v-if="props.level === 'bank'" class="pane__fields">
      <div class="field swatch-field">
        <label class="color-swatch" :style="{ '--swatch-color': swatch }">
          <input
            type="color"
            class="color-swatch__input"
            :value="colourHex"
            aria-label="Sub-header colour"
            @input="onColour"
          />
        </label>
        <span class="swatch-field__label">Sub-header colour</span>
      </div>
      <div class="field field--inline">
        <label class="field__label" for="bank-screen-layout">Screen layout</label>
        <span class="select">
          <select
            id="bank-screen-layout"
            class="select__native"
            :value="bank?.screenLayout ?? 'Grid'"
            @change="onScreenLayout"
          >
            <option value="Grid">Grid: button grid</option>
            <option value="Live">Live: large Bank, Patch and Page</option>
          </select>
        </span>
      </div>
    </div>

    <hr class="pane__divider" />

    <h3 class="button-section__heading">{{ meta.childListLabel }}</h3>
    <ul class="pane__children child-list">
      <li v-for="(child, i) in meta.children" :key="i">
        <button
          type="button"
          class="pane-child"
          :class="drag.rowClasses(i)"
          draggable="true"
          @click="child.select()"
          @dragstart="drag.onDragStart(i, $event)"
          @dragover="drag.onDragOverRow(i, $event)"
          @drop="drag.onDropRow(i, $event)"
          @dragend="drag.endDrag"
        >
          <span class="pane-child__grip" aria-hidden="true">⠿</span>
          <span class="pane-child__marker pane-child__marker--icon" aria-hidden="true"></span>
          <span class="pane-child__name">{{ child.name }}</span>
        </button>
      </li>
    </ul>

    <div class="pane__add">
      <button type="button" class="add-btn" @click="meta.add()">
        <span class="pane__add-label">Add new {{ meta.childLabel }}</span>
        <span class="icon-btn icon-btn--add" aria-hidden="true">+</span>
      </button>
    </div>

    <hr class="pane__divider" />

    <div class="button-section">
      <h3 class="button-section__heading">Buttons</h3>
      <p class="button-section__note">
        Buttons defined here apply to every {{ meta.childLabel }} below, unless
        one overrides them. Press a footswitch on the board to add or edit one.
      </p>

      <ol v-if="buttonSections.own.length" class="child-list child-list--buttons">
        <li v-for="entry in buttonSections.own" :key="entry.button.id">
          <button type="button" class="pane-child" @click="pressButton(entry.button.id)">
            <span class="pane-child__index">{{ entry.button.id + 1 }}.</span>
            <span
              class="pane-child__dot"
              :style="{ '--dot-color': rgbCss(effectiveColourCode(entry.button, bank)) }"
              aria-hidden="true"
            ></span>
            <span class="pane-child__name">{{ entry.button.name || '(unnamed)' }}</span>
          </button>
        </li>
      </ol>

      <template v-if="buttonSections.inherited.length">
        <h4 class="button-section__subheading">
          Inherited (not overridden at {{ props.level }} level)
        </h4>
        <ol class="child-list child-list--buttons">
          <li v-for="entry in buttonSections.inherited" :key="entry.button.id">
            <button
              type="button"
              class="pane-child pane-child--inherited"
              :title="`Defined at ${BUTTON_LEVEL_LABEL[entry.level]} level — click to override at ${props.level} level`"
              @click="pressButton(entry.button.id)"
            >
              <span class="pane-child__index">{{ entry.button.id + 1 }}.</span>
              <span
                class="pane-child__dot"
                :style="{ '--dot-color': rgbCss(effectiveColourCode(entry.button, bank)) }"
                aria-hidden="true"
              ></span>
              <span class="pane-child__name">{{ entry.button.name || '(unnamed)' }}</span>
              <span class="pane-child__badge">{{ BUTTON_LEVEL_LABEL[entry.level] }}</span>
            </button>
          </li>
        </ol>
      </template>
    </div>
  </section>
</template>
