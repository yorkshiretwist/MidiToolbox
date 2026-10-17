<script setup lang="ts">
// Pane for one patch: name, parameters, pages, and its own and inherited buttons.
import { computed } from 'vue'
import { useConfigStore } from '../../stores/config'
import { bankPath } from '../../lib/selection'
import { BUTTON_LEVEL_LABEL, effectiveColourCode, inheritedButtons, rgbCss } from '../../lib/buttons'
import { useDragReorder } from '../../lib/useDragReorder'
import type { Button } from '../../types/config'

const store = useConfigStore()

// Buttons this Patch inherits from its Bank/Mode/Config and hasn't overridden
// itself, resolved for the page currently in view — Bank/Mode/Config buttons
// aren't page-filtered (they always apply), but a Patch button only shadows
// one on the specific page(s) it's defined on, so "not overridden" is itself
// a per-page question. Clicking one offers to override it, scoped to this page.
const inherited = computed(() =>
  inheritedButtons(store.config, 'patch', store.parsed, store.currentPage),
)

function pressInherited(id: number) {
  store.activateButtonAtLevel('patch', id)
}

const CHANNELS = Array.from({ length: 16 }, (_, i) => i + 1)

const patch = computed(() => store.activePatch)

const params = computed(() => {
  const p = store.parsed
  if (p.modeIndex == null || p.bankIndex == null) return []
  const mode = store.config.modes[p.modeIndex]
  const bank = mode.banks[p.bankIndex]
  return (
    patch.value?.parameters ??
    bank.parameters ??
    mode.parameters ??
    store.config.parameters ??
    []
  )
})

interface ButtonEntry {
  index: number
  button: Button
}

// Every button in the patch, grouped under its page (page 0 = "every page").
// `heading` is what's shown above each group: with only one group, a plain
// "Buttons" covers it (a "Page 1" under that would just repeat the same
// count); with more than one, the first group's heading absorbs the
// "Buttons:" label instead of a redundant "Buttons: Page N" above the whole
// list plus a "Page N" repeating it right below.
const pageGroups = computed(() => {
  const groups = new Map<number, ButtonEntry[]>()
  ;(patch.value?.buttons ?? []).forEach((button, index) => {
    const page = button.page ?? 1
    const list = groups.get(page) ?? []
    list.push({ index, button })
    groups.set(page, list)
  })
  const sorted = [...groups.entries()]
    .sort((a, z) => a[0] - z[0])
    .map(([page, entries]) => ({
      page,
      label: page === 0 ? 'Every page' : `Page ${page}`,
      entries: entries.sort((a, z) => a.button.id - z.button.id),
    }))
  if (sorted.length <= 1) {
    return sorted.map((g) => ({ ...g, heading: 'Buttons' }))
  }
  return sorted.map((g, i) => ({ ...g, heading: i === 0 ? `Buttons: ${g.label}` : g.label }))
})

function onName(event: Event) {
  const value = (event.target as HTMLInputElement).value
  store.mutate(() => {
    if (patch.value) patch.value.name = value
  })
}

function onChannel(event: Event) {
  const raw = (event.target as HTMLSelectElement).value
  store.mutate(() => {
    if (!patch.value) return
    if (raw === '') delete patch.value.midiChannel
    else patch.value.midiChannel = Number(raw)
  })
}

async function deletePatch() {
  const p = store.parsed
  if (p.modeIndex == null || p.bankIndex == null || p.patchIndex == null) return
  const name = patch.value?.name?.trim()
  // A bank must keep at least one patch (schema minItems: 1). Removing the last
  // one is allowed here — the validation panel flags the now-empty bank.
  // Set actions pointing at the patch are listed and removed (see removeItem).
  const path = [p.modeIndex, p.bankIndex, p.patchIndex]
  if (!(await store.removeItem(path, name ? `the patch “${name}”` : 'this patch'))) return
  store.select(bankPath(p.modeIndex, p.bankIndex))
}

/* ── Drag and drop: swap two buttons' physical slots ──────────────────────────
   A button carries its (id, page) slot. Dragging one button onto another swaps
   those slots between them — moving each to where the other was, across pages
   too. The gesture (grip, drop target, auto-scroll) is shared with the mode /
   bank / patch lists: see lib/useDragReorder.ts. The indices are `entry.index`,
   the position in patch.buttons. */
const drag = useDragReorder((from, to) => store.swapButtonSlots(from, to), 'swap')
</script>

<template>
  <section
    v-if="patch"
    :ref="drag.paneEl"
    class="pane pane--active"
    data-level="patch"
    aria-label="Patch"
    @dragover="drag.onDragOverPane"
    @drop.prevent="drag.endDrag"
    @dragend="drag.endDrag"
  >
    <header class="pane__header">
      <span class="pane__label">Patch:</span>
      <input
        type="text"
        class="pane__name text-input"
        :value="patch.name"
        aria-label="Patch name"
        @input="onName"
      />
      <button
        type="button"
        class="icon-btn icon-btn--delete"
        aria-label="Delete patch"
        @click="deletePatch"
      >
        &#10005;
      </button>
    </header>

    <div class="pane__fields">
      <div class="field field--inline">
        <label class="field__label">MIDI Ch</label>
        <span class="select">
          <select
            class="select__native"
            :value="patch.midiChannel ?? ''"
            @change="onChannel"
          >
            <option value="">(default)</option>
            <option v-for="c in CHANNELS" :key="c" :value="c">{{ c }}</option>
          </select>
        </span>
      </div>

      <div class="field field--inline">
        <label class="field__label">Params</label>
        <ul class="param-list">
          <li v-for="param in params" :key="param" class="param-list__item">
            {{ param }}
          </li>
        </ul>
      </div>
    </div>

    <hr class="pane__divider" />

    <div class="pane__children">
      <div v-for="group in pageGroups" :key="group.page" class="page-group">
        <h3 class="page-group__heading">{{ group.heading }}</h3>
        <ol class="child-list child-list--buttons">
          <li v-for="entry in group.entries" :key="entry.index">
            <button
              type="button"
              class="pane-child"
              :class="drag.rowClasses(entry.index)"
              :data-button-id="entry.button.id"
              draggable="true"
              @click="store.selectButtonAt(entry.index)"
              @dragstart="drag.onDragStart(entry.index, $event)"
              @dragover="drag.onDragOverRow(entry.index, $event)"
              @drop="drag.onDropRow(entry.index, $event)"
              @dragend="drag.endDrag"
            >
              <span class="pane-child__grip" aria-hidden="true">⠿</span>
              <span class="pane-child__index">{{ entry.button.id + 1 }}.</span>
              <span
                class="pane-child__dot"
                :style="{ '--dot-color': rgbCss(effectiveColourCode(entry.button, store.activeBank)) }"
                aria-hidden="true"
              ></span>
              <span class="pane-child__name">{{ entry.button.name || '(unnamed)' }}</span>
            </button>
          </li>
        </ol>
      </div>
    </div>

    <template v-if="inherited.length">
      <hr class="pane__divider" />
      <div class="button-section">
        <h3 class="button-section__subheading">
          Inherited on {{ store.pageCount > 1 ? `page ${store.currentPage}` : 'this patch' }}
          (not overridden here)
        </h3>
        <ol class="child-list child-list--buttons">
          <li v-for="entry in inherited" :key="entry.button.id">
            <button
              type="button"
              class="pane-child pane-child--inherited"
              :title="`Defined at ${BUTTON_LEVEL_LABEL[entry.level]} level — click to override at Patch level`"
              @click="pressInherited(entry.button.id)"
            >
              <span class="pane-child__index">{{ entry.button.id + 1 }}.</span>
              <span
                class="pane-child__dot"
                :style="{ '--dot-color': rgbCss(effectiveColourCode(entry.button, store.activeBank)) }"
                aria-hidden="true"
              ></span>
              <span class="pane-child__name">{{ entry.button.name || '(unnamed)' }}</span>
              <span class="pane-child__badge">{{ BUTTON_LEVEL_LABEL[entry.level] }}</span>
            </button>
          </li>
        </ol>
      </div>
    </template>
  </section>
</template>

<style scoped>
.page-group + .page-group {
  margin-top: var(--space-4);
}

.page-group__heading {
  margin: 0 0 var(--space-2);
  font-size: var(--font-size-sm);
  font-weight: var(--font-weight-bold);
  text-transform: uppercase;
  letter-spacing: var(--letter-spacing-button);
  color: var(--color-text-muted);
}
</style>
