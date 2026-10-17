<script setup lang="ts">
// A list of design checks (✓ ok, ⚠ warning, ✗ error) as produced by lib/checks.ts.
import type { Check } from '../lib/checks'

defineProps<{ checks: Check[] }>()

const MARK = { ok: '✓', warn: '⚠', error: '✗' } as const
</script>

<template>
  <ul class="check-list">
    <li v-for="check in checks" :key="check.key" :class="`check--${check.severity}`">
      <span class="check-list__mark">{{ MARK[check.severity] }}</span>
      <span>{{ check.text }}</span>
    </li>
  </ul>
</template>
