<script setup lang="ts">
// The footswitch board in edit mode: click to edit a button, drag to move or swap, with inherited and built-in buttons marked.
import { computed, onBeforeUnmount, ref } from 'vue'
import { useConfigStore } from '../stores/config'
import { useDeviceStore } from '../stores/device'
import {
  BUTTON_LEVEL_LABEL,
  effectiveColourCode,
  navButtonDefaultLabel,
  navButtonDefaultRole,
  resolveButtonFrom,
  rgbCss,
} from '../lib/buttons'
import { deviceBoardLayout, deviceGeometry, type BoardSlot } from '../lib/deviceLayout'
import { confirmAction } from '../lib/confirm'

const store = useConfigStore()
const deviceStore = useDeviceStore()

const boardLayout = computed(() => deviceBoardLayout(deviceStore.profile))

const geometry = computed(() => deviceGeometry(deviceStore.profile))
const btnD = computed(() => `${(geometry.value.buttonMm / geometry.value.widthMm) * 100}cqw`)
/** Places a footswitch's centre at its real position on the unit's face. */
function posStyle(id: number) {
  const b = geometry.value.buttons.find((x) => x.id === id)!
  return { left: `${b.xPct}%`, top: `${b.yPct}%` }
}

// Physical ids of the active device's three navigation footswitches. Moving a
// button onto or off one of these disturbs its built-in role, so a drag that
// touches one is confirmed first.
const NAV_IDS = computed(() => Object.values(deviceStore.profile.navigation))

interface Unit {
  slot: BoardSlot
  label: string
  colour: string
  bound: boolean
  /**
   * Resolved from a less-specific level (Bank/Mode/Config) than the one
   * being edited — not yet overridden here. NOT true for a navigation id
   * still at its built-in default (see isBuiltIn below): that needs no
   * configuration, so it isn't "inherited" from anywhere in the config tree
   * the way a real Bank/Mode/Config override is.
   */
  inherited: boolean
  inheritedFrom?: string
  /** A navigation id currently running its built-in default — nothing overrides it anywhere. */
  isBuiltIn: boolean
  active: boolean
  inSet: boolean
  /** Belongs to the selected Exclusive button's set (shown when not editing the set). */
  setPeer: boolean
}

const units = computed<Unit[]>(() => {
  const level = store.editingLevel
  const selected = store.selectedButton
  const bank = store.activeBank
  // While the "Button set" toggle is on, the draft is the source of truth.
  const draft = store.buttonSetDraft
  // Otherwise a selected Exclusive button's saved set is shown as a subtle hint.
  const peers =
    draft === null && selected?.buttonMode === 'Exclusive' ? (selected.buttonSet ?? []) : []
  return boardLayout.value.slots.map((slot) => {
    const resolved = resolveButtonFrom(
      store.config,
      deviceStore.profile,
      level,
      store.parsed,
      slot.id,
      store.currentPage,
    )
    const button = resolved?.button
    const isBuiltIn = resolved?.level === 'builtin'
    // A pure built-in default isn't "inherited" — see the Unit.inherited doc above.
    const inherited = resolved != null && resolved.level !== level && !isBuiltIn
    const defaultLabel = isBuiltIn ? navButtonDefaultLabel(deviceStore.profile, slot.id) : undefined
    return {
      slot,
      label: defaultLabel ?? (button?.name?.trim() || String(slot.id + 1)),
      colour: rgbCss(effectiveColourCode(button, bank)),
      bound: button != null,
      inherited,
      inheritedFrom: inherited ? BUTTON_LEVEL_LABEL[resolved!.level] : undefined,
      isBuiltIn,
      active: selected != null && selected.id === slot.id,
      inSet: draft?.includes(slot.id) ?? false,
      setPeer: peers.includes(slot.id),
    }
  })
})

/* ── Drag and drop: swap two footswitches' buttons on the current page ────────
   Only a footswitch that actually has a button is draggable, and only while the
   board is not in "pick set members" mode. Dropping onto another footswitch
   swaps their buttons (an empty target just receives the dragged one). The drop
   zone is the board itself — a drop anywhere else does nothing. */

const dragId = ref<number | null>(null)
const dropId = ref<number | null>(null)

// Slot-swapping is a Patch-only, own-button feature: swapFootswitchButtons()
// only knows how to reslot the active Patch's own buttons array, so an
// inherited (not-yet-overridden) unit, a built-in nav default (nothing to
// actually swap — see isBuiltIn), or a Mode/Bank/File-level board, isn't
// draggable — only overriding or deleting can change those.
const canDrag = (unit: Unit) =>
  unit.bound &&
  !unit.inherited &&
  !unit.isBuiltIn &&
  !store.buttonSetEditing &&
  store.editingLevel === 'patch'

function onDragStart(id: number, event: DragEvent) {
  dragId.value = id
  dropId.value = null
  if (event.dataTransfer) {
    event.dataTransfer.effectAllowed = 'move'
    event.dataTransfer.setData('text/plain', String(id))
  }
}

function onDragOver(id: number, event: DragEvent) {
  if (dragId.value === null || id === dragId.value) {
    dropId.value = null
    return
  }
  event.preventDefault()
  if (event.dataTransfer) event.dataTransfer.dropEffect = 'move'
  dropId.value = id
}

async function onDrop(id: number, event: DragEvent) {
  event.preventDefault()
  const from = dragId.value
  endDrag()
  if (from === null || from === id) return
  if (!(await confirmNavSwap(from, id))) return
  store.swapFootswitchButtons(from, id)
}

// Confirm the move when either endpoint is a navigation footswitch; otherwise
// resolve straight to true.
function confirmNavSwap(from: number, to: number): Promise<boolean> {
  const touched = [from, to].filter((n) => NAV_IDS.value.includes(n))
  if (touched.length === 0) return Promise.resolve(true)
  const lines = touched.map((n) => {
    const role = navButtonDefaultRole(deviceStore.profile, n)
    return role
      ? `Footswitch ${n + 1} normally ${role}.`
      : `Footswitch ${n + 1} is a navigation control.`
  })
  return confirmAction(
    `${lines.join(' ')} Moving a button onto or off it changes what that ` +
      `footswitch does on this page. Continue?`,
    { title: 'Navigation footswitch', confirmLabel: 'Move', cancelLabel: 'Cancel' },
  )
}

function endDrag() {
  dragId.value = null
  dropId.value = null
}

onBeforeUnmount(endDrag)
</script>

<template>
  <div
    class="button-board"
    role="group"
    aria-label="Footswitches"
    :data-mode="store.buttonSetEditing ? 'button-set' : 'edit'"
    :style="{ '--btn-d': btnD }"
    @dragover.prevent
    @drop.prevent="endDrag"
    @dragend="endDrag"
  >
    <button
      v-for="unit in units"
      :key="unit.slot.id"
      type="button"
      class="footswitch"
      :class="{
        'footswitch--active': unit.active,
        'footswitch--unbound': !unit.bound,
        'footswitch--inherited': unit.inherited,
        'footswitch--in-set': unit.inSet,
        'footswitch--set-peer': unit.setPeer,
        'footswitch--draggable': canDrag(unit),
        'footswitch--dragging': dragId === unit.slot.id,
        'footswitch--drop-target': dropId === unit.slot.id,
      }"
      :style="{
        '--button-color': unit.colour,
        ...posStyle(unit.slot.id),
      }"
      :data-button-id="unit.slot.id"
      :data-led="unit.slot.led"
      :draggable="canDrag(unit)"
      :aria-pressed="store.buttonSetEditing ? unit.inSet : unit.active"
      :aria-current="unit.active ? 'true' : undefined"
      :title="unit.inherited ? `Inherited from ${unit.inheritedFrom} level — click to override here` : undefined"
      @click="store.pressFootswitch(unit.slot.id)"
      @dragstart="onDragStart(unit.slot.id, $event)"
      @dragover="onDragOver(unit.slot.id, $event)"
      @drop="onDrop(unit.slot.id, $event)"
      @dragend="endDrag"
    >
      <template v-if="unit.slot.led === 'below'">
        <span class="footswitch__dome" aria-hidden="true"></span>
        <span class="footswitch__led" aria-hidden="true"></span>
      </template>
      <template v-else>
        <span class="footswitch__led" aria-hidden="true"></span>
        <span class="footswitch__dome" aria-hidden="true"></span>
      </template>
      <span class="footswitch__label">{{ unit.label }}</span>
    </button>
  </div>
</template>

<style scoped>
/* An unbound footswitch (no button configured on this page) still selectable —
   it just reads as empty until it gets an action. */
.footswitch--unbound .footswitch__label {
  opacity: 0.5;
}

.footswitch--draggable {
  cursor: grab;
}

/* The footswitch being dragged fades; its live drop target gets an accent ring
   around the dome (matching the "in set" treatment's vocabulary). */
.footswitch--dragging {
  opacity: 0.4;
}

.footswitch--drop-target .footswitch__dome {
  outline: max(4px, 0.6cqw) solid var(--color-accent-info);
  outline-offset: max(3px, 0.4cqw);
}

/* Inherited from a less-specific level, not yet overridden at this one — a
   dashed ring around the dome (distinct from the solid "in set" ring) reads as
   "borrowed", and the dimmer label hints it isn't this level's own button. */
.footswitch--inherited .footswitch__dome {
  outline: max(3px, 0.5cqw) dashed var(--color-text-muted);
  outline-offset: max(3px, 0.4cqw);
}
/* Sibling in the selected Exclusive button's set: a thin solid blue halo
   (box-shadow, so it can combine with the inherited dashed outline) plus a
   blue-tinted label — far lighter than the thick "in set" editing ring. */
.footswitch--set-peer .footswitch__dome {
  box-shadow:
    0 0 0 max(2px, 0.3cqw) var(--color-accent-info),
    0 0 max(6px, 0.8cqw) color-mix(in srgb, var(--color-accent-info) 55%, transparent);
}
.footswitch--set-peer .footswitch__label {
  color: var(--color-accent-info);
}
.footswitch--inherited .footswitch__label {
  opacity: 0.7;
  font-style: italic;
}
</style>
