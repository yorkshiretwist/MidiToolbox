// The parts list: quantities derived from the design, nothing more.

import type { Design } from '../../types/device'
import type { Analysis } from '../checks'
import { buttonDiameter, screenDims } from '../geometry'
import { MEASURE_HEADLINE, MEASURE_SHORT } from './notices'

export interface PartRow {
  qty: number
  part: string
  notes: string
}

export function partsList(design: Design, analysis: Analysis): PartRow[] {
  const { screen, mcu, plan } = analysis
  const n = design.buttons.length
  const hole = buttonDiameter(design.purpose)
  const dims = screenDims(screen, design.screen.orientation)
  const rows: PartRow[] = [
    { qty: 1, part: mcu.bomName, notes: '' },
    { qty: 1, part: screen.bomName, notes: `drawing assumes a ${dims.module[0]}×${dims.module[1]} mm visible window — measure your part` },
    { qty: n, part: 'Momentary footswitch, SPST normally open, panel mount', notes: `drawing assumes a ø${hole} mm hole — measure your part` },
    { qty: n, part: 'RGB LED, 5 mm, common anode', notes: 'ShiftPWM output is inverted' },
    { qty: plan.registers, part: '74HC595 shift register, DIP-16', notes: `${plan.registers * 8} outputs for ${n * 3} LED channels` },
    { qty: n * 3, part: 'Resistor, 220 Ω, 0.25 W', notes: 'one per LED channel — size for your LEDs' },
    { qty: 1, part: 'Resistor, 100 Ω', notes: 'screen backlight' },
    { qty: plan.registers, part: 'Capacitor, 100 nF ceramic', notes: 'decoupling, one per register (optional)' },
  ]
  rows.push(
    { qty: 1, part: '9 V DC power supply, centre-positive, with barrel jack', notes: 'powers the LEDs; the Teensy 5 V pin cannot supply them all' },
    { qty: 1, part: '5 V regulator (7805 or 5 V buck module)', notes: 'steps 9 V down to 5 V for the 74HC595s and LEDs' },
    { qty: 1, part: 'Capacitor, 100 µF electrolytic', notes: 'across the 5 V LED rail' },
  )
  if (!mcu.sdBuiltIn) rows.push({ qty: 1, part: 'microSD breakout module', notes: 'shares the screen’s SPI bus' })
  rows.push(
    { qty: 1, part: 'microSD card', notes: 'holds performance configs' },
    { qty: 1, part: 'USB cable', notes: `power and USB-MIDI (${mcu.name} uses micro-USB)` },
    { qty: 1, part: 'Hook-up wire, headers, solder', notes: 'not counted' },
    { qty: 1, part: 'Enclosure', notes: `${design.unit.width} × ${design.unit.height} mm face — not modelled` },
  )
  return rows
}

function csvCell(value: string | number): string {
  const text = String(value)
  return /[",\n]/.test(text) ? `"${text.replaceAll('"', '""')}"` : text
}

export function partsCsv(rows: PartRow[]): string {
  const lines = [
    'Qty,Part,Notes',
    ...rows.map((r) => [r.qty, r.part, r.notes].map(csvCell).join(',')),
    // a normal row, so spreadsheets and importers keep it with the data
    ['', MEASURE_HEADLINE, MEASURE_SHORT].map(csvCell).join(','),
  ]
  return lines.join('\n') + '\n'
}

export function partsMarkdown(design: Design, rows: PartRow[]): string {
  const cell = (text: string) => text.replaceAll('|', '\\|')
  return [
    `# ${design.name} — parts list`,
    '',
    `> **⚠ ${MEASURE_HEADLINE}.** ${MEASURE_SHORT}`,
    '> The hole and module sizes in the notes column are what the drawing assumes; your parts will differ.',
    '',
    'Quantities are derived from the design. Check part choices against your own sourcing before buying.',
    '',
    '| Qty | Part | Notes |',
    '| ---: | --- | --- |',
    ...rows.map((r) => `| ${r.qty} | ${cell(r.part)} | ${cell(r.notes)} |`),
    '',
  ].join('\n')
}
