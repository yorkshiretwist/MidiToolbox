<script setup lang="ts">
// Root component of the Editor: header (MIDI output, Play Mode toggle), the device panel, and the accordion of editing panes.
import DevicePanel from './components/DevicePanel.vue'
import ChangeDeviceLink from './components/ChangeDeviceLink.vue'
import Accordion from './components/Accordion.vue'
import ConfirmDialog from './components/ConfirmDialog.vue'
import PromptDialog from './components/PromptDialog.vue'
import MidiOutputPicker from './components/MidiOutputPicker.vue'
import { usePlayStore } from './stores/play'
import { useConfigStore } from './stores/config'

const play = usePlayStore()
const config = useConfigStore()
const baseUrl = import.meta.env.BASE_URL

function togglePlayMode() {
  if (play.active) play.exitPlayMode()
  else play.enterPlayMode()
}
</script>

<template>
  <div class="app" :class="{ 'app--play': play.active }">
    <main class="workspace">
      <header class="workspace__header">
        <div class="app-heading">
          <h1 class="app-title">
            <span class="app-title__name">MIDI Toolbox</span>
            <span class="app-title__accent">Editor</span>
          </h1>
          <span
            v-if="config.dirty"
            class="not-downloaded"
            role="status"
            title="Your edits are saved in this browser, but you haven't downloaded the file yet"
          >
            <svg class="not-downloaded__icon" viewBox="0 0 24 24" aria-hidden="true">
              <path d="M12 3v11m0 0-4-4m4 4 4-4M5 20h14" />
            </svg>
            Changes not yet downloaded
          </span>
        </div>
        <div class="workspace__header-controls">
          <MidiOutputPicker />
          <button
            type="button"
            class="btn"
            :class="play.active ? 'btn--yes' : 'btn--blue'"
            :aria-pressed="play.active"
            @click="togglePlayMode"
          >{{ play.active ? 'Playing' : 'Play Mode' }}</button>
        </div>
      </header>

      <DevicePanel />
      <div class="workspace__footer">
        <!-- Relative to the Editor's base: the Designer is deployed at <base>designer/. -->
        <a class="app-switch-link" :href="`${baseUrl}../designer/`">Device Designer</a>
        <ChangeDeviceLink v-if="!play.active" />
      </div>
    </main>

    <aside v-if="!play.active" class="sidebar">
      <Accordion />
    </aside>

    <ConfirmDialog />
    <PromptDialog />
  </div>
</template>

<style scoped>
.app-heading {
  display: flex;
  align-items: center;
  gap: var(--space-4);
  min-width: 0;
}

/* Amber "pending" cue next to the title while edits haven't been downloaded. */
.not-downloaded {
  display: inline-flex;
  align-items: center;
  gap: var(--space-1);
  font-size: var(--font-size-sm);
  color: #f0b429;
}

.not-downloaded__icon {
  width: 1.25em;
  height: 1.25em;
  fill: none;
  stroke: currentColor;
  stroke-width: 2;
  stroke-linecap: round;
  stroke-linejoin: round;
}

.workspace__header-controls {
  display: flex;
  align-items: center;
  gap: var(--space-4);
}

/* With the sidebar hidden in Play mode, let the workspace column fill the width. */
.app--play {
  grid-template-columns: 1fr;
}
</style>
