<script setup lang="ts">
// Step 5's sidebar pane: pin budget for the Teensy and the per-connection pin assignments (buttons, screen, LED chain).
import { computed } from 'vue'
import Notice from '../Notice.vue'
import PinRow from '../PinRow.vue'
import PaneShell from './PaneShell.vue'
import { MICROCONTROLLERS } from '../../lib/catalog'
import { LED_PIN_KEYS } from '../../lib/pins'
import { navRoleOf } from '../../lib/layout'
import { useDesignStore } from '../../stores/design'
import type { LedPin } from '../../types/device'

const store = useDesignStore()
const design = computed(() => store.design)
const plan = computed(() => store.analysis.plan)
const mcu = computed(() => store.analysis.mcu)
const maxPin = computed(() => mcu.value.pins - 1)

const segments = computed(() => {
  const u = plan.value.usage
  return [
    { key: 'buttons', count: u.buttons, label: `Buttons ${u.buttons}` },
    { key: 'screen', count: u.screen, label: `Screen ${u.screen}` },
    { key: 'led', count: u.led, label: `LED chain ${u.led}` },
    { key: 'sd', count: u.sd, label: `SD module ${u.sd}` },
  ].filter((s) => s.count > 0)
})
const scale = computed(() => Math.max(plan.value.needed, plan.value.available))
const spare = computed(() => plan.value.available - plan.value.needed)

// ── pin assignments ──
const clash = computed(() => new Set(plan.value.conflicts.map((c) => c.pin)))
const isClash = (pin: number | undefined) => pin !== undefined && clash.value.has(pin)
// two or more buttons sharing a pin — shown in red rather than the usual amber
const severePins = computed(() => {
  const buttonsOnPin = new Map<number, number>()
  for (const use of plan.value.uses) {
    if (use.group === 'button') buttonsOnPin.set(use.pin, (buttonsOnPin.get(use.pin) ?? 0) + 1)
  }
  return new Set([...buttonsOnPin].filter(([, count]) => count > 1).map(([pin]) => pin))
})
const isSevere = (pin: number | undefined) => pin !== undefined && severePins.value.has(pin)
const pinWarnings = computed(() => store.analysis.checks.filter((c) => c.step === 5 && c.key !== 'pins' && c.severity === 'warn'))

const buttonRows = computed(() =>
  [...design.value.buttons]
    .sort((a, b) => a.id - b.id)
    .map((b) => {
      const role = navRoleOf(design.value, b.uid)
      return {
        uid: b.uid,
        label: `Button ${b.id + 1}${role ? ` · ${role === 'select' ? 'select' : role === 'down' ? 'down' : 'up'}` : ''}`,
        pin: plan.value.buttonPins.get(b.uid),
        edited: design.value.pins.buttons[b.uid] !== undefined,
      }
    }),
)

const gpioRows = computed(() => plan.value.screenPins.filter((p) => (p.kind ?? 'gpio') === 'gpio'))
const has = (kind: string) => plan.value.screenPins.some((p) => p.kind === kind)

// SPI and I2C signals must be on one hardware bus, so they are chosen as a set
const CUSTOM = '__custom__'
const spiNow = computed(() => {
  const pinOf = (role: string) => plan.value.screenPins.find((p) => p.role === role)?.pin
  const [sck, mosi, miso] = [pinOf('SCK'), pinOf('MOSI'), pinOf('MISO')]
  return mcu.value.spiBuses.find((b) => b.sck === sck && b.mosi === mosi && (miso === undefined || b.miso === miso))?.name ?? CUSTOM
})
const i2cNow = computed(() => {
  const pinOf = (role: string) => plan.value.screenPins.find((p) => p.role === role)?.pin
  return mcu.value.i2cBuses.find((b) => b.sda === pinOf('SDA') && b.scl === pinOf('SCL'))?.name ?? CUSTOM
})

function chooseSpi(name: string) {
  const bus = mcu.value.spiBuses.find((b) => b.name === name)
  if (!bus) return
  store.setScreenPin('SCK', bus.sck)
  store.setScreenPin('MOSI', bus.mosi)
  if (plan.value.screenPins.some((p) => p.role === 'MISO')) store.setScreenPin('MISO', bus.miso)
}
function chooseI2c(name: string) {
  const bus = mcu.value.i2cBuses.find((b) => b.name === name)
  if (!bus) return
  store.setScreenPin('SDA', bus.sda)
  store.setScreenPin('SCL', bus.scl)
}

// how many pins the user has set: only real buttons and this screen's own signals count
const changed = computed(() => {
  const pins = design.value.pins
  const buttons = design.value.buttons.filter((b) => pins.buttons[b.uid] !== undefined).length
  const screen = plan.value.screenPins.filter((p) => pins.screen[p.role] !== undefined).length
  const led = LED_PIN_KEYS.filter((k) => pins.led[k] !== undefined).length
  return buttons + screen + led
})
const screenEdited = (role: string) => design.value.pins.screen[role] !== undefined
const ledClash = (key: LedPin) => isClash(plan.value.ledPins[key])
</script>

<template>
  <PaneShell :n="5" title="Teensy">
    <ul class="opt-list" role="radiogroup" aria-label="Teensy model">
      <li v-for="m in MICROCONTROLLERS" :key="m.id">
        <button type="button" class="opt opt--selected" role="radio" aria-checked="true">
          <span class="opt__radio" />
          <span class="opt__main">
            <span class="opt__name">{{ m.name }}</span>
            <span class="opt__meta">
              {{ m.pins }} pins · {{ m.sdBuiltIn ? `microSD slot built in (uses ${m.sdPins.length} of them)` : 'no built-in microSD slot' }}
            </span>
          </span>
        </button>
      </li>
    </ul>
    <Notice kind="info">
      Other Teensy models are not available here because they do not have a built-in SD card slot. Advanced users can still add an SD module to another Teensy themselves.
    </Notice>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <div class="meter-total" :class="{ 'meter-total--over': plan.over }">
      <span>Pins needed</span>
      <b>{{ plan.needed }} of {{ plan.available }}</b>
    </div>
    <div class="meter" :class="{ 'meter--over': plan.over }" role="img" :aria-label="`${plan.needed} of ${plan.available} pins used`">
      <span v-for="s in segments" :key="s.key" class="meter__seg" :class="`meter__seg--${s.key}`" :style="{ width: `${(s.count / scale) * 100}%` }" />
    </div>
    <ul class="meter-legend">
      <li v-for="s in segments" :key="s.key"><i :class="`meter__seg--${s.key}`" />{{ s.label }}</li>
    </ul>

    <Notice v-if="plan.over" kind="error">
      {{ mcu.name }} has {{ plan.available }} usable pins and this design needs {{ plan.needed }}. Every button uses its own pin, so remove {{ plan.needed - plan.available }} or more buttons, or choose a screen that uses fewer pins.
    </Notice>
    <Notice v-else kind="ok">
      {{ spare }} pin{{ spare === 1 ? '' : 's' }} spare. The built-in microSD slot uses pins {{ plan.reservedPins[0] }}–{{ plan.reservedPins.at(-1) }}, which are not counted.
    </Notice>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <div class="pane__header" style="margin-bottom: 0">
      <span class="pane__label">Pin assignments</span>
      <span class="field__readout" style="margin-right: auto; margin-left: var(--space-2)">
        {{ changed === 0 ? 'all defaults' : `${changed} changed` }}
      </span>
      <button type="button" class="btn btn--blue btn--small" :disabled="changed === 0" @click="store.resetPins('all')">Reset all</button>
    </div>
    <p class="pane__hint">
      Defaults never share a pin. Change any of them by typing a pin number; ↺ puts one back. Ground and power are shared, so they are not listed.
    </p>

    <Notice v-for="check in pinWarnings" :key="check.key" kind="warn">{{ check.text }}.</Notice>

    <!-- screen -->
    <span class="pin-group">Screen</span>
    <div v-if="has('spi-sck')" class="field field--inline pin-bus">
      <label class="field__label" for="f-spi">SPI bus</label>
      <span class="select">
        <select id="f-spi" class="select__native" :value="spiNow" @change="chooseSpi(($event.target as HTMLSelectElement).value)">
          <option v-for="b in mcu.spiBuses" :key="b.name" :value="b.name">{{ b.name }} · SCK {{ b.sck }}, MOSI {{ b.mosi }}, MISO {{ b.miso }}</option>
          <option v-if="spiNow === CUSTOM" :value="CUSTOM" disabled>Custom (not a hardware bus)</option>
        </select>
      </span>
    </div>
    <div v-if="has('i2c-sda')" class="field field--inline pin-bus">
      <label class="field__label" for="f-i2c">I²C bus</label>
      <span class="select">
        <select id="f-i2c" class="select__native" :value="i2cNow" @change="chooseI2c(($event.target as HTMLSelectElement).value)">
          <option v-for="b in mcu.i2cBuses" :key="b.name" :value="b.name">{{ b.name }} · SDA {{ b.sda }}, SCL {{ b.scl }}</option>
          <option v-if="i2cNow === CUSTOM" :value="CUSTOM" disabled>Custom (not a hardware bus)</option>
        </select>
      </span>
    </div>
    <div class="pin-grid">
      <PinRow
        v-for="p in gpioRows"
        :key="p.role"
        :label="p.role"
        :pin="p.pin"
        :edited="screenEdited(p.role)"
        :max="maxPin"
        :conflict="isClash(p.pin)"
        @set="store.setScreenPin(p.role, $event)"
      />
    </div>

    <!-- LED shift registers -->
    <span class="pin-group">LED shift registers</span>
    <div class="pin-grid">
      <PinRow
        v-for="key in LED_PIN_KEYS"
        :key="key"
        :label="key"
        :pin="plan.ledPins[key]"
        :edited="design.pins.led[key] !== undefined"
        :max="maxPin"
        :conflict="ledClash(key)"
        @set="store.setLedPin(key, $event)"
      />
    </div>
    <p class="pane__hint">The firmware fixes these three pins (the LED library cannot take them from a file), so leave them as they are. The LEDs themselves hang off the registers, not the Teensy.</p>

    <!-- buttons -->
    <span class="pin-group">Buttons ({{ buttonRows.length }})</span>
    <div class="pin-grid pin-grid--scroll">
      <PinRow
        v-for="row in buttonRows"
        :key="row.uid"
        :label="row.label"
        :pin="row.pin"
        :edited="row.edited"
        :max="maxPin"
        :conflict="isClash(row.pin)"
        :severe="isSevere(row.pin)"
        @set="store.setButtonPin(row.uid, $event)"
      />
    </div>
  </PaneShell>
</template>
