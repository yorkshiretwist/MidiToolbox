<script setup lang="ts">
// The sidebar accordion: one section per design step. The open step shows its
// pane; the others collapse to a one-line summary with a status mark.
import { computed } from 'vue'
import UnitPane from './panes/UnitPane.vue'
import ButtonsPane from './panes/ButtonsPane.vue'
import NavigationPane from './panes/NavigationPane.vue'
import ScreenPane from './panes/ScreenPane.vue'
import TeensyPane from './panes/TeensyPane.vue'
import ExportPane from './panes/ExportPane.vue'
import { stepSeverity } from '../lib/checks'
import { navUids } from '../lib/layout'
import { useDesignStore } from '../stores/design'

const store = useDesignStore()

const MARK = { ok: '✓', warn: '⚠', error: '✗' } as const

const steps = computed(() => {
  const d = store.design
  const a = store.analysis
  const ids = new Map(d.buttons.map((b) => [b.uid, b.id]))
  return [
    { n: 1, label: 'Unit', value: `${d.unit.width} × ${d.unit.height} mm · ${d.purpose}`, pane: UnitPane },
    { n: 2, label: 'Buttons', value: `${d.buttons.length} buttons`, pane: ButtonsPane },
    { n: 3, label: 'Navigation', value: navUids(d).map((uid) => (ids.get(uid) ?? 0) + 1).join(' · '), pane: NavigationPane },
    { n: 4, label: 'Screen', value: `${a.screen.name} · ${d.screen.orientation}`, pane: ScreenPane },
    { n: 5, label: 'Teensy', value: a.mcu.name, pane: TeensyPane },
    { n: 6, label: 'Export', value: a.blocked ? 'Fix issues first' : 'Ready', pane: ExportPane },
  ].map((step) => ({ ...step, severity: stepSeverity(a, step.n) }))
})
</script>

<template>
  <div class="pane-stack">
    <section v-for="step in steps" :key="step.n" class="step" :class="{ 'step--active': store.step === step.n }">
      <button v-if="store.step !== step.n" type="button" class="pane-crumb" :data-level="`s${step.n}`" @click="store.goTo(step.n)">
        <span class="pane-crumb__num">{{ step.n }}</span>
        <span class="pane-crumb__label">{{ step.label }}:</span>
        <span class="pane-crumb__value">{{ step.value }}</span>
        <span class="pane-crumb__status" :class="`pane-crumb__status--${step.severity}`">{{ MARK[step.severity] }}</span>
      </button>
      <component :is="step.pane" v-else />
    </section>
  </div>
</template>
