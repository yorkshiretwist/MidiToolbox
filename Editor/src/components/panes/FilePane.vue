<script setup lang="ts">
// Pane for the whole config file: new/open/save, the list of modes, and file-level buttons.
import { computed } from 'vue'
import { useConfigStore } from '../../stores/config'
import { modePath, presetPath } from '../../lib/selection'
import { presetName, sortedPresets } from '../../lib/presets'
import { effectiveColourCode, rgbCss } from '../../lib/buttons'
import { useDragReorder } from '../../lib/useDragReorder'
import type { Mode } from '../../types/config'

const store = useConfigStore()

// Drag a mode to a new position in the list (lib/useDragReorder.ts).
const drag = useDragReorder((from, to) => store.moveItem([], from, to), 'move')

const name = computed(() => (store.fileName ?? 'untitled').replace(/\.json$/i, ''))

const children = computed(() =>
  store.config.modes.map((m: Mode, i: number) => ({
    name: m.name || '(unnamed mode)',
    select: () => store.select(modePath(i)),
  })),
)

// Presets, alphabetically; each row opens the preset by its place in config.presets
// (which stays in creation order - only this list is sorted).
const presets = computed(() =>
  sortedPresets(store.config).map((preset) => ({
    id: preset.id,
    name: presetName(preset),
    select: () => store.select(presetPath(store.config.presets!.indexOf(preset))),
  })),
)

// File (root Config) buttons are the lowest-priority fallback in the Patch >
// Bank > Mode > Config cascade — every Mode/Bank/Patch below can override
// them, but there's nothing above the File level for it to inherit from.
const buttons = computed(() => store.buttonEditor('file'))

function pressButton(id: number) {
  store.activateButtonAtLevel('file', id)
}

function addMode() {
  store.mutate((c) =>
    c.modes.push({
      name: 'New mode',
      banks: [{ name: 'New bank', patches: [{ name: 'New patch', buttons: [] }] }],
    }),
  )
  store.select(modePath(store.config.modes.length - 1))
}

async function onNew() {
  await store.newConfig()
}

async function onOpen() {
  await store.open()
}

function onDownload() {
  store.download()
}
</script>

<template>
  <section
    :ref="drag.paneEl"
    class="pane pane--active"
    data-level="file"
    aria-label="file"
    @dragover="drag.onDragOverPane"
    @drop.prevent="drag.endDrag"
    @dragend="drag.endDrag"
  >
    <header class="pane__header">
      <span class="pane__label">File:</span>
      <span class="pane__name">{{ name }}</span>
    </header>

    <div class="file-actions">
      <button type="button" class="btn btn--blue" @click="onNew">New</button>
      <button type="button" class="btn btn--open" @click="onOpen">Open</button>
      <button type="button" class="btn btn--save" @click="onDownload">
        Download
      </button>
    </div>

    <div v-if="store.parseError" class="file-alert" role="alert">
      <p class="file-alert__title">Couldn't open file</p>
      <p class="file-alert__message">{{ store.parseError }}</p>
    </div>
    <div v-else-if="!store.isValid" class="file-alert" role="alert">
      <p class="file-alert__title">
        {{ store.validation.issues.length }}
        {{ store.validation.issues.length === 1 ? 'validation error' : 'validation errors' }}
      </p>
      <ul class="file-alert__issues">
        <li
          v-for="(issue, i) in store.validation.issues"
          :key="i"
          class="file-alert__issue"
        >
          <span class="file-alert__issue-path">{{ issue.path || '(root)' }}</span>
          {{ issue.message }}
        </li>
      </ul>
    </div>

    <hr class="pane__divider" />

    <h3 class="button-section__heading">Modes</h3>
    <ul class="pane__children child-list">
      <li v-for="(child, i) in children" :key="i">
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
      <button type="button" class="add-btn" @click="addMode">
        <span class="pane__add-label">Add new mode</span>
        <span class="icon-btn icon-btn--add" aria-hidden="true">+</span>
      </button>
    </div>

    <hr class="pane__divider" />

    <div class="button-section">
      <h3 class="button-section__heading">Presets</h3>
      <p class="button-section__note">
        A preset is a named list of actions you can run from any button with an
        “Execute preset” action. Change a preset and every button that runs it
        changes too.
      </p>

      <ul v-if="presets.length" class="child-list">
        <li v-for="preset in presets" :key="preset.id">
          <button type="button" class="pane-child" @click="preset.select()">
            <span class="pane-child__marker pane-child__marker--icon" aria-hidden="true"></span>
            <span class="pane-child__name">{{ preset.name }}</span>
          </button>
        </li>
      </ul>

      <div class="pane__add">
        <button type="button" class="add-btn" @click="store.addPreset()">
          <span class="pane__add-label">Add new preset</span>
          <span class="icon-btn icon-btn--add" aria-hidden="true">+</span>
        </button>
      </div>
    </div>

    <hr class="pane__divider" />

    <div class="button-section">
      <h3 class="button-section__heading">Buttons</h3>
      <p class="button-section__note">
        Buttons defined here apply everywhere in this file, unless a Mode,
        Bank, or Patch overrides them. Press a footswitch on the board to add
        or edit one.
      </p>

      <ol v-if="buttons.own.length" class="child-list child-list--buttons">
        <li v-for="entry in buttons.own" :key="entry.button.id">
          <button type="button" class="pane-child" @click="pressButton(entry.button.id)">
            <span class="pane-child__index">{{ entry.button.id + 1 }}.</span>
            <span
              class="pane-child__dot"
              :style="{ '--dot-color': rgbCss(effectiveColourCode(entry.button, undefined)) }"
              aria-hidden="true"
            ></span>
            <span class="pane-child__name">{{ entry.button.name || '(unnamed)' }}</span>
          </button>
        </li>
      </ol>
    </div>
  </section>
</template>
