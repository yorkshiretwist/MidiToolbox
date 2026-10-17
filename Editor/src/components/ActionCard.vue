<script setup lang="ts">
// Editor for one action on a button: type picker plus the parameter fields that type needs.
import { computed, ref, useId } from 'vue'
import { useConfigStore } from '../stores/config'
import NoteKeyboard from './NoteKeyboard.vue'
import {
  findOption,
  isIndexedTargetType,
  targetKeys,
  targetOptions,
  type TargetOption,
  type TargetScope,
} from '../lib/actionTargets'
import { findPreset, presetName, sortedPresets } from '../lib/presets'
import { CC_DEFAULT_OFF, CC_DEFAULT_ON, DEFAULT_VELOCITY, ensureActionDefaults } from '../lib/actionDefaults'
import { clampToRange, isInRange } from '../lib/range'
import type { Action, ActionParameters, ActionType, Button } from '../types/config'

const props = defineProps<{ action: Action }>()
const emit = defineEmits<{ (e: 'delete'): void }>()

const store = useConfigStore()

const CHANNELS = Array.from({ length: 16 }, (_, i) => i + 1)

type FieldKind = 'number' | 'text' | 'notes' | 'spread' | 'direction' | 'channel' | 'target' | 'config' | 'preset'
interface FieldDef {
  key: keyof ActionParameters
  label: string
  kind: FieldKind
  min?: number
  max?: number
  /**
   * A number field that is never left empty: it shows this value when nothing is stored, and puts
   * it (or the stored value) back if the box is emptied and left.
   */
  default?: number
}

// MIDI controller numbers, CC values and program numbers are all 7-bit.
const MIDI_7BIT = { min: 0, max: 127 } as const

const DIRECTION: FieldDef = { key: 'direction', label: 'Direction', kind: 'direction' }
const CHANNEL: FieldDef = { key: 'midiChannel', label: 'MIDI Ch', kind: 'channel' }

const FIELDS_BY_TYPE: Record<ActionType, FieldDef[]> = {
  // Runs a preset's actions at this point in the button's list (see lib/presets.ts).
  ExecutePreset: [{ key: 'presetId', label: 'Preset', kind: 'preset' }],
  PlayNotes: [
    { key: 'noteNames', label: 'Notes', kind: 'notes' },
    { key: 'velocity', label: 'Velocity', kind: 'number', default: DEFAULT_VELOCITY, ...MIDI_7BIT },
    { key: 'noteSpread', label: 'Spread', kind: 'spread' },
    CHANNEL,
  ],
  ProgramChange: [
    { key: 'programNumber', label: 'Program', kind: 'number', ...MIDI_7BIT },
    CHANNEL,
  ],
  ControlChange: [
    { key: 'controllerNumber', label: 'CC', kind: 'number', ...MIDI_7BIT },
    { key: 'controlValue', label: 'On', kind: 'number', default: CC_DEFAULT_ON, ...MIDI_7BIT },
    { key: 'releaseValue', label: 'Off', kind: 'number', default: CC_DEFAULT_OFF, ...MIDI_7BIT },
    CHANNEL,
  ],
  SetParameter: [{ key: 'parameterName', label: 'Parameter', kind: 'text' }],
  CycleParameter: [
    { key: 'parameterName', label: 'Parameter', kind: 'text' },
    DIRECTION,
  ],
  // No parameters - CycleActivateParameterAction (firmware) always advances
  // the selector, or activates a pending change, with nothing to configure.
  // This is the built-in default action for the device's 'select' navigation
  // footswitch (printed 11, id 10 on the reference device) - see
  // builtInNavButtons() in lib/buttons.ts.
  CycleActivateParameter: [],
  SetParameterValue: [
    { key: 'parameterName', label: 'Parameter', kind: 'text' },
    { key: 'parameterValue', label: 'Value', kind: 'text' },
  ],
  CycleParameterValue: [
    { key: 'parameterName', label: 'Parameter', kind: 'text' },
    DIRECTION,
  ],
  SetPatch: [{ key: 'patchIndex', label: 'Patch', kind: 'target' }],
  CyclePatch: [DIRECTION],
  SetBank: [{ key: 'bankIndex', label: 'Bank', kind: 'target' }],
  CycleBank: [DIRECTION],
  SetMode: [{ key: 'modeIndex', label: 'Mode', kind: 'target' }],
  CycleMode: [DIRECTION],
  SetConfig: [{ key: 'configFilename', label: 'File', kind: 'config' }],
  CycleConfig: [DIRECTION],
  SetPage: [{ key: 'page', label: 'Page', kind: 'number' }],
  CyclePage: [DIRECTION],
}

const OPTION_GROUPS: { label: string; types: [ActionType, string][] }[] = [
  {
    label: 'MIDI messages',
    types: [
      ['PlayNotes', 'Play notes'],
      ['ControlChange', 'Control change'],
      ['ProgramChange', 'Program change'],
    ],
  },
  {
    label: 'Parameter',
    types: [
      ['SetParameter', 'Set parameter'],
      ['CycleParameter', 'Cycle parameter'],
      ['CycleActivateParameter', 'Cycle/Activate parameter'],
      ['SetParameterValue', 'Set parameter value'],
      ['CycleParameterValue', 'Cycle parameter value'],
    ],
  },
  { label: 'Patch', types: [['SetPatch', 'Set patch'], ['CyclePatch', 'Cycle patch']] },
  { label: 'Bank', types: [['SetBank', 'Set bank'], ['CycleBank', 'Cycle bank']] },
  { label: 'Mode', types: [['SetMode', 'Set mode'], ['CycleMode', 'Cycle mode']] },
  { label: 'Config', types: [['SetConfig', 'Set config'], ['CycleConfig', 'Cycle config']] },
  { label: 'Page', types: [['SetPage', 'Set page'], ['CyclePage', 'Cycle page']] },
]

const fields = computed(() => FIELDS_BY_TYPE[props.action.type] ?? [])

// ---- Execute preset: run a preset's actions here, picked by name (never by id) ----

// A preset's own actions can't run another preset (no nesting), so the option is
// not offered while one is being edited.
const offersPresets = computed(() => store.parsed.level !== 'preset')

// Alphabetical, so the list is easy to scan; what is stored is the preset's id.
const presetList = computed(() => sortedPresets(store.config))
const hasPresets = computed(() => presetList.value.length > 0)
const currentPreset = computed(() => findPreset(store.config, params.value.presetId))

function onPreset(event: Event) {
  const id = Number((event.target as HTMLSelectElement).value)
  store.mutate(() => {
    props.action.actionParameters = { presetId: id }
  })
}
const params = computed<ActionParameters>(() => props.action.actionParameters ?? {})

// ---- Set patch / bank / mode: pick the target by its path, never by raw index ----

// Where this action runs decides which targets are offered and how short their
// stored form can be: a button on a Patch or Bank always runs in that one bank
// (and its mode); on a Mode, in any bank of that mode; on the File, anywhere.
// See lib/actionTargets.ts.
const scope = computed<TargetScope>(() => {
  const p = store.parsed
  const level = store.editingLevel
  if (level === 'file') return {}
  if (level === 'mode') return { modeIndex: p.modeIndex ?? undefined }
  return { modeIndex: p.modeIndex ?? undefined, bankIndex: p.bankIndex ?? undefined }
})

const targetOptionList = computed<TargetOption[]>(() =>
  isIndexedTargetType(props.action.type) ? targetOptions(props.action.type, store.config, scope.value) : [],
)

/** The option the stored parameters correspond to, or undefined when none is set or it no longer exists. */
const currentTarget = computed(() =>
  isIndexedTargetType(props.action.type) ? findOption(targetOptionList.value, props.action.type, params.value) : undefined,
)

const targetIsSet = computed(
  () => isIndexedTargetType(props.action.type) && targetKeys(props.action.type).some((k) => params.value[k] !== undefined),
)

const TARGET_NOUN = { SetPatch: 'patch', SetBank: 'bank', SetMode: 'mode' } as const
const targetNoun = computed(() => (isIndexedTargetType(props.action.type) ? TARGET_NOUN[props.action.type] : ''))

/** "mode.bank.patch" for the stored parameters, shown only if the target went missing. */
const missingTargetText = computed(() => {
  if (!isIndexedTargetType(props.action.type)) return ''
  return targetKeys(props.action.type)
    .map((k) => params.value[k])
    .filter((v) => v !== undefined)
    .join('.')
})

/** Writes a target: clears every index this action type uses, then stores just the ones the option needs. */
function applyTarget(type: ActionType, option: TargetOption) {
  if (!isIndexedTargetType(type)) return
  store.mutate(() => {
    const ap = (props.action.actionParameters ??= {}) as Record<string, unknown>
    for (const key of targetKeys(type)) delete ap[key]
    Object.assign(ap, option.params)
  })
}

function onTarget(event: Event) {
  const key = (event.target as HTMLSelectElement).value
  const option = targetOptionList.value.find((o) => o.key === key)
  if (option) applyTarget(props.action.type, option)
}

// ---- Set config: the Editor can't see the unit's SD card, so offer the names
// already known (this file's own, and any other the config already points at). ----

const configListId = useId()

function collectConfigFilenames(buttons: Button[] | undefined, into: Set<string>) {
  for (const button of buttons ?? []) {
    collectFromActions(button.actions, into)
  }
}

function collectFromActions(actions: Action[] | undefined, into: Set<string>) {
  for (const action of actions ?? []) {
    const name = action.actionParameters?.configFilename?.trim()
    if (name) into.add(name)
  }
}

const configSuggestions = computed(() => {
  const names = new Set<string>()
  if (store.fileName) names.add(store.fileName)
  const config = store.config
  collectConfigFilenames(config.buttons, names)
  for (const preset of config.presets ?? []) collectFromActions(preset.actions, names)
  for (const mode of config.modes) {
    collectConfigFilenames(mode.buttons, names)
    for (const bank of mode.banks) {
      collectConfigFilenames(bank.buttons, names)
      for (const patch of bank.patches) collectConfigFilenames(patch.buttons, names)
    }
  }
  return [...names].sort((a, b) => a.localeCompare(b))
})

function notesText(): string {
  return (params.value.noteNames ?? []).join(', ')
}

function setParam(
  key: keyof ActionParameters,
  value: string | number | string[] | undefined,
) {
  store.mutate(() => {
    const ap = (props.action.actionParameters ??= {}) as Record<string, unknown>
    const empty =
      value === undefined ||
      value === '' ||
      (Array.isArray(value) && value.length === 0)
    if (empty) delete ap[key]
    else ap[key] = value
  })
}

/** Whether this box has a range (the 7-bit MIDI boxes do), which its value is kept inside. */
const hasRange = (field: FieldDef) => field.min !== undefined || field.max !== undefined

function onNumber(field: FieldDef, event: Event) {
  const input = event.target as HTMLInputElement
  const raw = input.value
  // A box that must never be empty: while it is blank (say, mid-edit) keep the stored value, and
  // let onNumberLeave put a value back if it is still blank when the player moves away.
  if (raw === '' && field.default !== undefined) return
  if (raw === '') {
    setParam(field.key, undefined)
    return
  }
  // A box with a range never holds a number outside it: the typed value is pulled to the nearest
  // end (and rounded to a whole number), and the box shows what was kept.
  const value = hasRange(field) ? clampToRange(Number(raw), field.min, field.max) : Number(raw)
  if (hasRange(field) && String(value) !== raw) input.value = String(value)
  setParam(field.key, value)
}

// Leaving a box: a never-empty box with nothing in it gets back the value it had (or the default),
// and a stored value outside the box's range (a file can hold one) is brought inside it.
function onNumberLeave(field: FieldDef, event: Event) {
  const input = event.target as HTMLInputElement
  if (input.value === '' && field.default !== undefined) {
    input.value = String(params.value[field.key] ?? field.default)
    return
  }
  const stored = params.value[field.key]
  if (hasRange(field) && typeof stored === 'number' && !isInRange(stored, field.min, field.max)) {
    const value = clampToRange(stored, field.min, field.max)
    setParam(field.key, value)
    input.value = String(value)
  }
}

function onText(key: keyof ActionParameters, event: Event) {
  setParam(key, (event.target as HTMLInputElement | HTMLSelectElement).value)
}

function onNotes(event: Event) {
  const raw = (event.target as HTMLInputElement).value
  const list = raw
    .split(/[,\s]+/)
    .map((n) => n.trim())
    .filter(Boolean)
  setParam('noteNames', list)
}

const keyboardOpen = ref(false)

function onKeyboardSave(notes: string[]) {
  setParam('noteNames', notes)
  keyboardOpen.value = false
}

function onType(event: Event) {
  const value = (event.target as HTMLSelectElement).value as ActionType
  store.mutate(() => {
    props.action.type = value
    if (value === 'ExecutePreset') {
      // Start on a real preset (the first alphabetically) rather than an unset one,
      // with none of the previous type's parameters left behind.
      const first = sortedPresets(store.config)[0]
      props.action.actionParameters = first ? { presetId: first.id } : {}
      return
    }
    if (props.action.actionParameters) delete props.action.actionParameters.presetId
    if (isIndexedTargetType(value)) {
      // Start on a real target rather than an unset (or stale) one: keep the
      // current target if it still makes sense for the new type, else the first.
      const options = targetOptions(value, store.config, scope.value)
      const ap = (props.action.actionParameters ??= {}) as Record<string, unknown>
      const keep = findOption(options, value, ap as ActionParameters)
      for (const key of ['modeIndex', 'bankIndex', 'patchIndex']) delete ap[key]
      const chosen = keep ?? options[0]
      if (chosen) Object.assign(ap, chosen.params)
    }
    // A note always has a velocity, and a CC toggle sends full on / full off by default: neither is
    // ever left empty.
    ensureActionDefaults(props.action)
  })
}
</script>

<template>
  <section class="action-card" aria-label="Action">
    <div class="field field--inline">
      <span class="select">
        <select
          class="select__native action-card__type"
          aria-label="Action type"
          :value="action.type"
          @change="onType"
        >
          <option
            v-if="offersPresets"
            value="ExecutePreset"
            :disabled="!hasPresets && action.type !== 'ExecutePreset'"
          >
            {{ hasPresets || action.type === 'ExecutePreset' ? 'Execute preset' : 'Execute preset (no presets yet)' }}
          </option>
          <optgroup
            v-for="group in OPTION_GROUPS"
            :key="group.label"
            :label="group.label"
          >
            <option v-for="[value, text] in group.types" :key="value" :value="value">
              {{ text }}
            </option>
          </optgroup>
        </select>
      </span>
      <button
        type="button"
        class="icon-btn icon-btn--delete"
        aria-label="Delete action"
        @click="emit('delete')"
      >
        &#10005;
      </button>
    </div>

    <div class="action-card__fields">
      <p v-if="fields.length === 0" class="action-card__note">No parameters.</p>

      <div v-for="field in fields" :key="field.key" class="field field--inline">
        <label class="field__label">{{ field.label }}</label>

        <input
          v-if="field.kind === 'notes'"
          type="text"
          class="text-input"
          :value="notesText()"
          placeholder="C2, E2, G2"
          @change="onNotes"
        />
        <button
          v-if="field.kind === 'notes'"
          type="button"
          class="btn btn--blue action-card__keyboard-btn"
          @click="keyboardOpen = true"
        >
          Keyboard
        </button>
        <span v-else-if="field.kind === 'preset'" class="select">
          <select
            class="select__native"
            aria-label="Preset to run"
            :value="currentPreset?.id ?? '__missing'"
            @change="onPreset"
          >
            <option v-if="!currentPreset" value="__missing" disabled>
              {{ params.presetId !== undefined ? 'Preset not found: choose one' : 'Choose a preset…' }}
            </option>
            <option v-for="preset in presetList" :key="preset.id" :value="preset.id">
              {{ presetName(preset) }}
            </option>
          </select>
        </span>
        <span v-else-if="field.kind === 'target'" class="select">
          <select
            class="select__native"
            :aria-label="`Target ${targetNoun}`"
            :value="currentTarget?.key ?? '__missing'"
            @change="onTarget"
          >
            <option v-if="!currentTarget" value="__missing" disabled>
              {{ targetIsSet ? `Not valid (saved as ${missingTargetText}): choose a ${targetNoun}` : `Choose a ${targetNoun}…` }}
            </option>
            <option v-for="o in targetOptionList" :key="o.key" :value="o.key">{{ o.label }}</option>
          </select>
        </span>
        <template v-else-if="field.kind === 'config'">
          <input
            type="text"
            class="text-input"
            :list="configListId"
            placeholder="config.json"
            :value="params.configFilename ?? ''"
            @input="onText('configFilename', $event)"
          />
          <datalist :id="configListId">
            <option v-for="name in configSuggestions" :key="name" :value="name"></option>
          </datalist>
        </template>
        <input
          v-else-if="field.kind === 'number'"
          type="number"
          class="text-input text-input--num"
          :min="field.min"
          :max="field.max"
          :value="params[field.key] ?? field.default ?? ''"
          @input="onNumber(field, $event)"
          @blur="onNumberLeave(field, $event)"
        />
        <span v-else-if="field.kind === 'spread'" class="select">
          <select
            class="select__native"
            :value="params.noteSpread ?? 'NoSpread'"
            @change="onText('noteSpread', $event)"
          >
            <option value="NoSpread">No spread</option>
            <option value="SpreadUp">Spread up</option>
            <option value="SpreadDown">Spread down</option>
            <option value="SpreadRandom">Spread random</option>
          </select>
        </span>
        <span v-else-if="field.kind === 'direction'" class="select">
          <select
            class="select__native"
            :value="params.direction ?? 'Up'"
            @change="onText('direction', $event)"
          >
            <option value="Up">Up</option>
            <option value="Down">Down</option>
          </select>
        </span>
        <span v-else-if="field.kind === 'channel'" class="select">
          <select
            class="select__native"
            :value="params.midiChannel ?? ''"
            @change="onNumber(field, $event)"
          >
            <option value="">(default)</option>
            <option v-for="c in CHANNELS" :key="c" :value="c">{{ c }}</option>
          </select>
        </span>
        <input
          v-else
          type="text"
          class="text-input"
          :value="params[field.key] ?? ''"
          @input="onText(field.key, $event)"
        />
      </div>
    </div>

    <NoteKeyboard
      v-if="keyboardOpen"
      :notes="params.noteNames ?? []"
      @save="onKeyboardSave"
      @cancel="keyboardOpen = false"
    />
  </section>
</template>

<style scoped>
.action-card__note {
  margin: 0;
  color: var(--color-text-muted);
  font-size: var(--font-size-sm);
}

.action-card__keyboard-btn {
  flex: none;
  height: var(--control-height);
  padding: 0 var(--space-3);
}
</style>
