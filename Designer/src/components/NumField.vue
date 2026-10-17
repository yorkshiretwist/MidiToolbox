<script setup lang="ts">
// A numeric text input that commits on blur/Enter, clamps to min/max and reverts bad input.
import { nextTick, ref, watch } from 'vue'

const props = defineProps<{
  modelValue: number
  min?: number
  max?: number
  disabled?: boolean
  label?: string
}>()
const emit = defineEmits<{ 'update:modelValue': [value: number] }>()

const text = ref(String(props.modelValue))
watch(
  () => props.modelValue,
  (value) => {
    text.value = String(value)
  },
)

// Commit on change (blur / Enter) rather than per keystroke, so a half-typed
// number never reaches the design. Anything that is not a number reverts.
function commit() {
  const parsed = Number(text.value)
  if (text.value.trim() === '' || !Number.isFinite(parsed)) {
    text.value = String(props.modelValue)
    return
  }
  let value = parsed
  if (props.min !== undefined) value = Math.max(props.min, value)
  if (props.max !== undefined) value = Math.min(props.max, value)
  emit('update:modelValue', value)
  // the parent may adjust or ignore the value: show whatever it settled on
  nextTick(() => {
    text.value = String(props.modelValue)
  })
}
</script>

<template>
  <input
    v-model="text"
    class="text-input text-input--num"
    type="text"
    inputmode="decimal"
    :aria-label="label"
    :disabled="disabled"
    @change="commit"
    @keydown.enter="commit"
  />
</template>
