<script setup lang="ts">
// A note in the sidebar. `info` is guidance, `warn` is advice that never blocks
// anything, `error` blocks downloads, `ok` confirms.
defineProps<{ kind: 'info' | 'warn' | 'error' | 'ok' }>()

const MARK = { info: 'i', warn: '⚠', error: '✗', ok: '✓' } as const
</script>

<template>
  <div class="callout" :class="`callout--${kind}`" role="note">
    <span class="callout__mark">{{ MARK[kind] }}</span>
    <span><slot /></span>
  </div>
</template>
