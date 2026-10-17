<script setup lang="ts">
// Common frame for a step's pane: title, "Step n of 6", the fields, and the
// Back / Next buttons. A pane can replace the footer buttons.
import { STEP_COUNT, useDesignStore } from '../../stores/design'

const props = defineProps<{ n: number; title: string }>()
const store = useDesignStore()
</script>

<template>
  <div class="pane pane--active step__body" :data-level="`s${props.n}`">
    <div class="pane__header">
      <h2 class="pane__title">{{ title }}</h2>
      <span class="pane__step-count">Step {{ n }} of {{ STEP_COUNT }}</span>
    </div>
    <div class="pane__fields">
      <slot />
    </div>
    <div class="pane__nav">
      <slot name="footer">
        <button type="button" class="btn btn--revert" :disabled="n === 1" @click="store.goTo(n - 1)">Back</button>
        <button type="button" class="btn btn--save" @click="store.goTo(n + 1)">Next</button>
      </slot>
    </div>
  </div>
</template>
