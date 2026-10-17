<script setup lang="ts">
// Step 3's sidebar pane: choose which three buttons play the built-in select / step-down / step-up roles.
import { computed } from 'vue'
import CheckList from '../CheckList.vue'
import Notice from '../Notice.vue'
import PaneShell from './PaneShell.vue'
import { navRoleOf, performanceButtons } from '../../lib/layout'
import { useDesignStore } from '../../stores/design'
import type { NavRole } from '../../types/device'

const store = useDesignStore()
const design = computed(() => store.design)
const options = computed(() => [...design.value.buttons].sort((a, b) => a.id - b.id))
const checks = computed(() => {
  const perf = performanceButtons(design.value).length
  return [
    ...store.analysis.checks.filter((c) => c.step === 3),
    {
      key: 'perf-left',
      severity: perf >= 3 ? ('ok' as const) : ('error' as const),
      step: 3,
      text: `${perf} performance buttons remain (minimum 3)`,
    },
  ]
})

const ROLES: { role: NavRole; label: string; hint: string }[] = [
  { role: 'select', label: 'Select / Set', hint: 'cycles the parameter · “Set?” · hold to revert' },
  { role: 'down', label: 'Step down ◀', hint: 'previous value' },
  { role: 'up', label: 'Step up ▶', hint: 'next value' },
]

function optionText(uid: number) {
  const b = design.value.buttons.find((x) => x.uid === uid)!
  const other = navRoleOf(design.value, uid)
  return `Button ${b.id + 1} (id ${b.id})${other ? '' : ' · performance'}`
}
</script>

<template>
  <PaneShell :n="3" title="Navigation">
    <p class="pane__hint">
      Every unit has exactly three built-in navigation buttons. Choose which of your buttons play each role, by picking here or by clicking the unit.
    </p>

    <div v-for="item in ROLES" :key="item.role" class="nav-role">
      <span class="field__label">{{ item.label }} <span class="nav-role__hint">{{ item.hint }}</span></span>
      <span class="select">
        <select
          class="select__native"
          :aria-label="`${item.label} button`"
          :value="design.nav[item.role]"
          @change="store.setNav(item.role, Number(($event.target as HTMLSelectElement).value))"
        >
          <option v-for="b in options" :key="b.uid" :value="b.uid">{{ optionText(b.uid) }}</option>
        </select>
      </span>
      <button
        type="button"
        class="icon-btn icon-btn--pick"
        :class="{ 'icon-btn--active': store.pickingRole === item.role }"
        :aria-pressed="store.pickingRole === item.role"
        aria-label="Pick on the unit"
        @click="store.startPicking(item.role)"
      >
        ◎
      </button>
    </div>
    <Notice v-if="store.pickingRole" kind="info">Click a button on the unit to give it this role.</Notice>

    <CheckList :checks="checks" />

    <hr class="pane__divider" style="margin: var(--space-1) 0" />
    <p class="pane__hint">
      The on-screen ◀ Parameter ▶ row always mirrors these three, so navigation stays reachable even if a config repurposes the footswitches.
    </p>
  </PaneShell>
</template>
