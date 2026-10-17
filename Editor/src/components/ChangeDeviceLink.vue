<script setup lang="ts">
// Shows the active device name and lets the user switch device (after confirming that it clears all buttons).
import { ref } from 'vue'
import { useDeviceStore } from '../stores/device'
import { useConfigStore } from '../stores/config'
import { confirmAction } from '../lib/confirm'

const deviceStore = useDeviceStore()
const configStore = useConfigStore()
const busy = ref(false)

// Picking a file and confirming are two separate steps (see
// stores/device.ts): only once a file has parsed successfully do we warn
// about the reset, so a bad file never shows a scary prompt for nothing.
// The warning itself is advisory only — it never blocks the switch, even
// with unsaved changes (the user was told so when this was scoped).
async function changeDevice() {
  busy.value = true
  try {
    const device = await deviceStore.pickDeviceFile()
    if (!device) return
    const proceed = await confirmAction(
      `Switching to ${device.name} clears every button in this file. Different devices have ` +
        `nothing to do with each other — button ids, navigation placement and the screen layout ` +
        `only make sense for one specific device — so every button, at every mode, bank and ` +
        `patch, is reset. This can't be undone from within the editor. Save a copy of the ` +
        `current file first if you want to keep it.`,
      { title: 'Change device', confirmLabel: 'Reset & switch device', cancelLabel: 'Cancel' },
    )
    if (!proceed) return
    configStore.clearAllButtons()
    deviceStore.loadDevice(device)
  } finally {
    busy.value = false
  }
}
</script>

<template>
  <div class="device-bar-bottom">
    <span class="device-bar-bottom__name">
      {{ deviceStore.profile.name }} &middot; {{ deviceStore.buttonIds.length }} buttons
    </span>
    <button type="button" class="change-device-link" :disabled="busy" @click="changeDevice">
      Change device&hellip;
    </button>
    <span v-if="deviceStore.openError" class="change-device-error">{{ deviceStore.openError }}</span>
  </div>
</template>
