<script setup lang="ts">
// Root component of the Designer: the to-scale unit stage (or the export preview on step 6) beside the sidebar step stack.
import ConfirmDialog from './components/ConfirmDialog.vue'
import ExportPreview from './components/ExportPreview.vue'
import ScreenZoom from './components/ScreenZoom.vue'
import StepStack from './components/StepStack.vue'
import UnitStage from './components/UnitStage.vue'
import { onMounted } from 'vue'
import { appFileBridge } from './lib/fileIo'
import { useDesignStore } from './stores/design'

const store = useDesignStore()
const baseUrl = import.meta.env.BASE_URL

// The desktop app's File menu (Open, Open recent, Save, Save as) acts on the open design.
onMounted(() => {
  appFileBridge()?.onCommand((command) => {
    if (command.type === 'open') void store.openFile()
    else if (command.type === 'openPath' && command.path) void store.openFile(command.path)
    else if (command.type === 'save') void store.saveFile()
    else if (command.type === 'saveAs') void store.saveFile(true)
  })
})
</script>

<template>
  <div class="app" :data-step="store.step" :data-blocked="store.analysis.blocked">
    <main class="workspace">
      <header class="workspace__header">
        <h1 class="app-title">
          <img class="app-title__logo" :src="`${baseUrl}wordmark.svg`" alt="MIDI Toolbox" />
          <span class="app-title__accent">Designer</span>
        </h1>
      </header>

      <ExportPreview v-if="store.step === 6" />
      <UnitStage v-else />

      <!-- The Editor is deployed one level above the Designer's base (<base>../). -->
      <a class="app-switch-link" :href="`${baseUrl}../editor/`">Button Editor</a>
    </main>

    <aside class="sidebar">
      <StepStack />
    </aside>

    <ScreenZoom />
    <ConfirmDialog />
  </div>
</template>
