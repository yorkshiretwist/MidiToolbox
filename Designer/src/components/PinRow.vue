<script setup lang="ts">
// One connection and the Teensy pin it uses. Typing a pin sets it; the ↺ button
// (shown only when the pin differs from its default) puts it back.
import NumField from './NumField.vue'

defineProps<{
  label: string
  /** The pin in effect; undefined when the board ran out of pins. */
  pin: number | undefined
  /** The user has set this pin (as opposed to it being the default). */
  edited: boolean
  /** Highest pin number the board has. */
  max: number
  /** Another connection is on this pin. */
  conflict?: boolean
  /** A conflict here is two buttons on the same pin, not just a shared bus signal — shown in red. */
  severe?: boolean
}>()
const emit = defineEmits<{ set: [pin: number | null] }>()

</script>

<template>
  <div class="pin-row" :class="{ 'pin-row--conflict': conflict, 'pin-row--severe': conflict && severe, 'pin-row--edited': edited }">
    <span class="pin-row__label" :title="label">{{ label }}</span>
    <NumField
      v-if="pin !== undefined"
      :label="`${label} pin`"
      :model-value="pin"
      :min="0"
      :max="max"
      @update:model-value="emit('set', $event)"
    />
    <span v-else class="pin-row__none" title="Every pin is taken. Free one up or remove a button.">none left</span>
    <button
      v-if="edited"
      type="button"
      class="pin-row__reset"
      :aria-label="`Reset ${label} to its default pin`"
      title="Put back to the default pin"
      @click="emit('set', null)"
    >
      ↺
    </button>
    <span v-else class="pin-row__reset pin-row__reset--none" />
  </div>
</template>
