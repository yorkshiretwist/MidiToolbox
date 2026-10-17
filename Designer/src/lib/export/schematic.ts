// The wiring schematic: not to scale, illustrative. Teensy in the middle,
// footswitches on the left, the screen on the right, and the RGB LED
// shift-register chain underneath. Every connection is labelled with its pin,
// and a table sorted by Teensy pin follows for wiring from. A pin used twice is
// drawn in red, with a banner at the top.

import type { Design, NavRole } from '../../types/device'
import type { Analysis } from '../checks'
import { formatPinRuns } from '../pins'
import { isNavButton, NAV_ROLES, navRoleOf } from '../layout'
import { EXPERIMENTAL_HEADLINE, EXPERIMENTAL_WARNING, wrapText } from './notices'
import { esc } from './xml'

const INK = '#1c1c1c'
const MUTED = '#666'
const NAV = '#1e82e0'
const ALERT = '#c62828'
const ROLE_TAG: Record<NavRole, string> = { select: 'select', down: 'step down', up: 'step up' }

export function schematicSvg(design: Design, analysis: Analysis): string {
  const { plan, screen, mcu } = analysis
  const n = design.buttons.length
  const idSorted = [...design.buttons].sort((a, b) => a.id - b.id)
  const perfPins = idSorted.filter((b) => !isNavButton(design, b.uid)).map((b) => plan.buttonPins.get(b.uid))
  const navPins = NAV_ROLES.map((role) => plan.buttonPins.get(design.nav[role]))
  const clash = new Set(plan.conflicts.map((c) => c.pin))
  const pinFill = (pin: number) => (clash.has(pin) ? ALERT : INK)
  const flag = (pin: number) => (clash.has(pin) ? '⚠ ' : '')

  // wide lists of scattered pins do not fit on a wire label: point at the table
  const label = (pins: (number | undefined)[], prefix = '') => {
    const text = formatPinRuns(pins)
    if (!text) return `${prefix}—`
    return text.length > 24 ? `${prefix}${pins.filter((p) => p !== undefined).length} pins (table below)` : `${prefix}${text}`
  }
  const anyClash = (pins: (number | undefined)[]) => pins.some((p) => p !== undefined && clash.has(p))

  // ── the conflict banner, if any ──
  const bannerLines = plan.conflicts.slice(0, 6).map((c) => `Pin ${c.pin}: ${c.owners.join(' and ')}`)
  if (plan.conflicts.length > 6) bannerLines.push(`… and ${plan.conflicts.length - 6} more`)
  const bannerHeight = plan.conflicts.length ? 46 + bannerLines.length * 18 : 0
  // the EXPERIMENTAL box, always shown, styled like the drawing's warning
  const expLines = wrapText(EXPERIMENTAL_WARNING, 130)
  const expBoxHeight = 34 + expLines.length * 16
  const expHeight = expBoxHeight + 12
  const dy = expHeight + bannerHeight

  const screenRows = [...plan.screenPins, ...plan.sdPins]
  const boxTop = 110
  const boxHeight = Math.max(330, 90 + screenRows.length * 30)
  const boxBottom = boxTop + boxHeight
  const chainTop = boxBottom + 40
  const powerTop = chainTop + 156
  const diagramBottom = powerTop + 84

  // ── the pin table, by Teensy pin ──
  const columns = 3
  const uses = plan.uses
  const rowsPerColumn = Math.max(1, Math.ceil(uses.length / columns))
  const tableTop = diagramBottom + 36
  const rowHeight = 19
  const tableBottom = tableTop + 30 + rowsPerColumn * rowHeight + (plan.reservedPins.length ? 30 : 6)
  const height = dy + tableBottom + 10

  const s: string[] = []
  s.push(
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 900 ${height}" font-family="Arial, Helvetica, sans-serif" font-size="13">`,
  )
  s.push(`<rect width="900" height="${height}" fill="#fafafa"/>`)
  s.push(`<text x="24" y="34" font-size="20" font-weight="700" fill="${INK}">${esc(design.name)} — wiring schematic</text>`)
  s.push(`<rect x="24" y="44" width="852" height="${expBoxHeight}" rx="4" fill="#fff1cc" stroke="#c98a1b" stroke-width="3"/>`)
  s.push(`<text x="40" y="66" font-size="15" font-weight="700" fill="#7a4a00">⚠ ${EXPERIMENTAL_HEADLINE}</text>`)
  expLines.forEach((line, i) => s.push(`<text x="40" y="${86 + i * 16}" font-size="12" fill="#3a2a00">${esc(line)}</text>`))
  s.push(`<text x="24" y="${44 + expBoxHeight + 20}" fill="${MUTED}">Not to scale. Simplified. Every connection is labelled with its ${esc(mcu.name)} pin.</text>`)

  if (plan.conflicts.length) {
    s.push(`<g transform="translate(0 ${expHeight})">`)
    s.push(`<rect x="24" y="66" width="852" height="${bannerHeight - 12}" rx="4" fill="#fdecea" stroke="${ALERT}" stroke-width="2"/>`)
    s.push(`<text x="40" y="90" font-size="15" font-weight="700" fill="${ALERT}">⚠ ${plan.conflicts.length === 1 ? 'A pin is' : `${plan.conflicts.length} pins are`} used by more than one connection</text>`)
    bannerLines.forEach((line, i) => s.push(`<text x="40" y="${112 + i * 18}" fill="#7f1d1d">${esc(line)}</text>`))
    s.push('</g>')
  }

  s.push(`<g transform="translate(0 ${dy})">`)

  // microcontroller
  s.push(`<rect x="340" y="${boxTop}" width="220" height="${boxHeight}" rx="6" fill="#fff" stroke="${INK}" stroke-width="2"/>`)
  s.push(`<text x="450" y="${boxTop + 30}" text-anchor="middle" font-size="18" font-weight="700" fill="${INK}">${esc(mcu.name)}</text>`)
  s.push(`<text x="450" y="${boxTop + 50}" text-anchor="middle" fill="${MUTED}">USB-MIDI to host</text>`)
  s.push(`<line x1="450" y1="${boxTop}" x2="450" y2="${boxTop - 26}" stroke="${INK}" stroke-width="2"/>`)
  s.push(`<rect x="405" y="${boxTop - 42}" width="90" height="16" rx="3" fill="#ececec" stroke="${INK}"/>`)
  s.push(`<text x="450" y="${boxTop - 30}" text-anchor="middle">USB</text>`)

  // footswitches
  s.push(`<rect x="30" y="${boxTop + 20}" width="200" height="170" rx="6" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(`<text x="130" y="${boxTop + 44}" text-anchor="middle" font-weight="700" fill="${INK}">${n} × footswitch</text>`)
  s.push(`<text x="130" y="${boxTop + 62}" text-anchor="middle" fill="${MUTED}">momentary, normally open</text>`)
  s.push(`<text x="130" y="${boxTop + 90}" text-anchor="middle" fill="${INK}">one leg → its own GPIO pin</text>`)
  s.push(`<text x="130" y="${boxTop + 108}" text-anchor="middle" fill="${INK}">other leg → GND</text>`)
  s.push(`<text x="130" y="${boxTop + 136}" text-anchor="middle" fill="${MUTED}">(internal pull-ups used)</text>`)
  s.push(`<line x1="230" y1="${boxTop + 80}" x2="340" y2="${boxTop + 80}" stroke="${anyClash(perfPins) ? ALERT : INK}" stroke-width="2"/>`)
  s.push(`<line x1="230" y1="${boxTop + 120}" x2="340" y2="${boxTop + 120}" stroke="${anyClash(navPins) ? ALERT : NAV}" stroke-width="2"/>`)
  s.push(`<text x="236" y="${boxTop + 74}" font-size="11" fill="${anyClash(perfPins) ? ALERT : INK}">${esc(label(perfPins, 'pins '))}</text>`)
  s.push(`<text x="236" y="${boxTop + 114}" font-size="11" fill="${anyClash(navPins) ? ALERT : NAV}">${esc(label(navPins, 'nav pins '))}</text>`)

  // screen
  const screenHeight = 40 + screenRows.length * 30
  s.push(`<rect x="690" y="${boxTop}" width="190" height="${screenHeight}" rx="6" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(`<text x="785" y="${boxTop + 24}" text-anchor="middle" font-weight="700" fill="${INK}">${esc(screen.name)}</text>`)
  screenRows.forEach((pin, i) => {
    const y = boxTop + 50 + i * 30
    s.push(`<line x1="560" y1="${y}" x2="690" y2="${y}" stroke="${clash.has(pin.pin) ? ALERT : INK}" stroke-width="1.5"/>`)
    s.push(`<text x="566" y="${y - 5}" font-size="11" font-weight="${clash.has(pin.pin) ? 700 : 400}" fill="${clash.has(pin.pin) ? ALERT : MUTED}">${flag(pin.pin)}pin ${pin.pin}</text>`)
    s.push(`<text x="700" y="${y + 4}" font-size="12" fill="${INK}">${esc(pin.role)}</text>`)
  })
  s.push(`<text x="785" y="${boxTop + screenHeight + 18}" text-anchor="middle" font-size="11" fill="${MUTED}">VCC → VIN · GND · backlight via 100 Ω</text>`)

  // LED chain
  const { latch, data, clock } = plan.ledPins
  const ledLine = (x: number, pin: number, name: string) => {
    s.push(`<line x1="${x}" y1="${boxBottom}" x2="${x}" y2="${chainTop}" stroke="${clash.has(pin) ? ALERT : INK}" stroke-width="1.5"/>`)
    s.push(`<text x="${x + 6}" y="${boxBottom + 22}" font-size="11" font-weight="${clash.has(pin) ? 700 : 400}" fill="${clash.has(pin) ? ALERT : MUTED}">${flag(pin)}${pin} ${name}</text>`)
  }
  ledLine(380, latch, 'latch')
  ledLine(450, data, 'data')
  ledLine(520, clock, 'clock')

  const drawn = plan.registers <= 5 ? Array.from({ length: plan.registers }, (_, i) => i + 1) : [1, 2, 3, 4, plan.registers]
  drawn.forEach((index, slot) => {
    const x = 140 + slot * 120
    s.push(`<rect x="${x}" y="${chainTop}" width="104" height="44" rx="4" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
    s.push(`<text x="${x + 52}" y="${chainTop + 20}" text-anchor="middle" font-weight="700" fill="${INK}">74HC595</text>`)
    s.push(`<text x="${x + 52}" y="${chainTop + 36}" text-anchor="middle" font-size="11" fill="${MUTED}">${index} of ${plan.registers}</text>`)
    if (slot > 0) {
      const dashed = plan.registers > 5 && slot === 4 ? ` stroke-dasharray="4 3"` : ''
      s.push(`<line x1="${x - 16}" y1="${chainTop + 22}" x2="${x}" y2="${chainTop + 22}" stroke="${INK}" stroke-width="1.5"${dashed}/>`)
    }
  })
  s.push(`<polyline points="140,${chainTop + 22} 124,${chainTop + 22} 124,${chainTop - 10} 380,${chainTop - 10}" fill="none" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(
    `<text x="140" y="${chainTop + 74}" fill="${INK}">${plan.registers} shift register${plan.registers === 1 ? '' : 's'} ` +
      `(${plan.registers * 8} outputs) → ${n} RGB LEDs (${n * 3} channels, resistor per channel)</text>`,
  )
  s.push(`<text x="140" y="${chainTop + 94}" fill="${MUTED}">Chained data → data; shared latch and clock. 5 V from the regulator below for the registers and LEDs.</text>`)
  s.push(`<text x="140" y="${chainTop + 118}" font-weight="700" fill="#7a4a00">Check every connection against your own parts' datasheets before you apply power.</text>`)

  // LED power: the Teensy's own 5 V pin cannot supply every LED, so a 9 V
  // supply is stepped down to 5 V for the registers and LED anodes
  const POWER = '#b45309'
  s.push(`<text x="30" y="${powerTop - 8}" font-weight="700" fill="${INK}">LED power (separate from the USB 5 V)</text>`)
  s.push(`<rect x="30" y="${powerTop}" width="150" height="54" rx="4" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(`<text x="105" y="${powerTop + 22}" text-anchor="middle" font-weight="700" fill="${INK}">9 V DC supply</text>`)
  s.push(`<text x="105" y="${powerTop + 40}" text-anchor="middle" font-size="11" fill="${MUTED}">centre-positive barrel jack</text>`)
  s.push(`<line x1="180" y1="${powerTop + 27}" x2="250" y2="${powerTop + 27}" stroke="${POWER}" stroke-width="2"/>`)
  s.push(`<text x="184" y="${powerTop + 20}" font-size="11" fill="${POWER}">9 V</text>`)
  s.push(`<rect x="250" y="${powerTop}" width="230" height="54" rx="4" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(`<text x="365" y="${powerTop + 22}" text-anchor="middle" font-weight="700" fill="${INK}">5 V regulator</text>`)
  s.push(`<text x="365" y="${powerTop + 40}" text-anchor="middle" font-size="11" fill="${MUTED}">7805 or a 5 V buck module</text>`)
  s.push(`<line x1="480" y1="${powerTop + 27}" x2="560" y2="${powerTop + 27}" stroke="${POWER}" stroke-width="2"/>`)
  s.push(`<text x="486" y="${powerTop + 20}" font-size="11" fill="${POWER}">5 V rail</text>`)
  s.push(`<rect x="560" y="${powerTop}" width="316" height="54" rx="4" fill="#fff" stroke="${INK}" stroke-width="1.5"/>`)
  s.push(`<text x="718" y="${powerTop + 22}" text-anchor="middle" font-weight="700" fill="${INK}">74HC595 VCC · LED common anodes</text>`)
  s.push(`<text x="718" y="${powerTop + 40}" text-anchor="middle" font-size="11" fill="${MUTED}">100 µF across the 5 V rail, close to the LEDs</text>`)
  s.push(`<text x="30" y="${powerTop + 74}" font-size="11" fill="${MUTED}">Join the supply GND, the regulator GND and the ${esc(mcu.name)} GND together. Do not join this 5 V rail to the ${esc(mcu.name)} 5 V/VIN pin while USB is plugged in.</text>`)

  // ── the pin table ──
  s.push(`<line x1="24" y1="${tableTop - 18}" x2="876" y2="${tableTop - 18}" stroke="#cfcfcf"/>`)
  s.push(`<text x="24" y="${tableTop + 4}" font-size="16" font-weight="700" fill="${INK}">Wiring by ${esc(mcu.name)} pin</text>`)
  const colWidth = 284
  uses.forEach((use, i) => {
    const col = Math.floor(i / rowsPerColumn)
    const row = i % rowsPerColumn
    const x = 24 + col * colWidth
    const y = tableTop + 30 + row * rowHeight
    const bad = clash.has(use.pin)
    const role = use.uid !== undefined ? navRoleOf(design, use.uid) : undefined
    const owner = role ? `${use.owner} (${ROLE_TAG[role]})` : use.owner
    s.push(
      `<text x="${x}" y="${y}" font-size="12" font-weight="${bad ? 700 : 400}" fill="${bad ? ALERT : INK}">` +
        `<tspan font-weight="700" fill="${pinFill(use.pin)}">${flag(use.pin)}${use.pin}</tspan>  →  ${esc(owner)}</text>`,
    )
  })
  if (plan.reservedPins.length) {
    s.push(
      `<text x="24" y="${tableTop + 30 + rowsPerColumn * rowHeight + 20}" font-size="12" fill="${MUTED}">` +
        `Pins ${esc(formatPinRuns(plan.reservedPins))} are wired to the built-in SD card and are not available.</text>`,
    )
  }
  s.push('</g>')
  s.push('</svg>')
  return s.join('')
}
