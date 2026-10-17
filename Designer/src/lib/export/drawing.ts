// The design drawing: a to-scale front view of the unit with hole positions,
// the screen cut-out and overall dimensions. Units are millimetres.
//
// It opens with a boxed warning that the hole sizes are placeholders: nothing
// on the page is a measurement of the builder's own parts.

import type { Design } from '../../types/device'
import type { Analysis } from '../checks'
import { buttonDiameter, ledDiameter, ledOffset, screenDims, tidy } from '../geometry'
import { NAV_ROLES, navRoleOf } from '../layout'
import { MEASURE_HEADLINE, measureWarning, wrapText } from './notices'
import { esc } from './xml'

const ROLE_TAG = { select: 'SELECT', down: 'DOWN', up: 'UP' } as const

export function drawingSvg(design: Design, analysis: Analysis): string {
  const { width: W, height: H, pitch } = design.unit
  const fs = Math.max(4.6, Math.max(W, H) / 62)
  const diameter = buttonDiameter(design.purpose)
  const r = diameter / 2
  const dims = screenDims(analysis.screen, design.screen.orientation)

  // the warning box, wrapped to the width of the drawing
  const padL = fs * 6
  const padR = fs * 2
  const boxWidth = W + padL - fs
  const noteFs = fs * 0.95
  const lines = wrapText(measureWarning(diameter, dims.module), Math.floor((boxWidth - fs * 2) / (noteFs * 0.5)))
  const boxHeight = fs * 2.6 + lines.length * noteFs * 1.35
  const padT = fs * 3 + boxHeight + fs * 4
  const padB = fs * 8

  const out: string[] = []
  out.push(
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${-padL} ${-padT} ${W + padL + padR} ${H + padT + padB}" ` +
      `font-family="Arial, Helvetica, sans-serif" font-size="${fs}">`,
  )
  out.push(`<rect x="${-padL}" y="${-padT}" width="${W + padL + padR}" height="${H + padT + padB}" fill="#fafafa"/>`)
  out.push(`<text x="${-padL + fs * 0.5}" y="${-padT + fs * 1.8}" font-size="${fs * 1.5}" font-weight="700" fill="#1c1c1c">${esc(design.name)}</text>`)

  // ── warning banner ──
  const boxX = -padL + fs * 0.5
  const boxY = -padT + fs * 3
  out.push(
    `<rect x="${boxX}" y="${boxY}" width="${boxWidth}" height="${boxHeight}" rx="${fs * 0.3}" fill="#fff1cc" stroke="#c98a1b" stroke-width="${fs * 0.18}"/>`,
  )
  out.push(
    `<text x="${boxX + fs}" y="${boxY + fs * 1.6}" font-size="${fs * 1.15}" font-weight="700" fill="#7a4a00">⚠ ${MEASURE_HEADLINE}</text>`,
  )
  lines.forEach((line, i) => {
    out.push(`<text x="${boxX + fs}" y="${tidy(boxY + fs * 1.6 + noteFs * 1.55 + i * noteFs * 1.35)}" font-size="${noteFs}" fill="#3a2a00">${esc(line)}</text>`)
  })

  // ── layout grid and outline ──
  const grid: string[] = []
  for (let x = pitch; x < W; x += pitch) grid.push(`M${tidy(x)} 0V${H}`)
  for (let y = pitch; y < H; y += pitch) grid.push(`M0 ${tidy(y)}H${W}`)
  out.push(`<path d="${grid.join('')}" stroke="#e8e8e8" stroke-width="${fs * 0.05}" fill="none"/>`)
  out.push(`<rect x="0" y="0" width="${W}" height="${H}" rx="${fs * 0.7}" fill="none" stroke="#1c1c1c" stroke-width="${fs * 0.14}"/>`)

  // ── screen: the cut-out (the visible window, bevel included) and the lit display inside it ──
  const sx = design.screen.x
  const sy = design.screen.y
  out.push(
    `<rect x="${sx}" y="${sy}" width="${dims.module[0]}" height="${dims.module[1]}" fill="#f0f0f0" stroke="#1c1c1c" ` +
      `stroke-width="${fs * 0.12}"/>`,
  )
  out.push(
    `<rect x="${tidy(sx + (dims.module[0] - dims.active[0]) / 2)}" y="${tidy(sy + (dims.module[1] - dims.active[1]) / 2)}" ` +
      `width="${dims.active[0]}" height="${dims.active[1]}" fill="none" stroke="#6a6a6a" ` +
      `stroke-width="${fs * 0.08}" stroke-dasharray="${fs * 0.4} ${fs * 0.3}"/>`,
  )
  out.push(
    `<text x="${sx}" y="${sy - fs * 0.5}" font-size="${fs * 0.85}" fill="#444">${esc(analysis.screen.name)} — cut-out ` +
      `${dims.module[0]}×${dims.module[1]}, display ${dims.active[0]}×${dims.active[1]} (measure yours)</text>`,
  )

  // ── buttons ──
  for (const b of design.buttons) {
    const role = navRoleOf(design, b.uid)
    out.push(`<circle cx="${b.x}" cy="${b.y}" r="${r}" fill="#fff" stroke="#1c1c1c" stroke-width="${fs * 0.12}"/>`)
    if (role) {
      out.push(
        `<circle cx="${b.x}" cy="${b.y}" r="${r + 2}" fill="none" stroke="#1e82e0" stroke-width="${fs * 0.1}" stroke-dasharray="${fs * 0.3} ${fs * 0.2}"/>`,
      )
      out.push(
        `<text x="${b.x}" y="${b.y + r + fs * 1.1}" text-anchor="middle" font-size="${fs * 0.7}" fill="#1e82e0">${ROLE_TAG[role]}</text>`,
      )
    }
    out.push(
      `<text x="${b.x}" y="${tidy(b.y + fs * 0.35)}" text-anchor="middle" font-size="${fs * 1.05}" font-weight="700" fill="#1c1c1c">${b.id + 1}</text>`,
    )
    const ly = b.led === 'above' ? b.y - ledOffset(diameter) : b.y + ledOffset(diameter)
    out.push(`<circle cx="${b.x}" cy="${tidy(ly)}" r="${tidy(ledDiameter(diameter) / 2)}" fill="none" stroke="#6a6a6a" stroke-width="${fs * 0.08}"/>`)
  }

  // ── hole positions: distinct centres along the top and left edges ──
  const xs = [...new Set(design.buttons.map((b) => tidy(b.x)))].sort((a, b) => a - b)
  const ys = [...new Set(design.buttons.map((b) => tidy(b.y)))].sort((a, b) => a - b)
  const datum: string[] = []
  for (const x of xs) {
    datum.push(`<line x1="${x}" y1="${-fs * 0.3}" x2="${x}" y2="${-fs * 1.1}" stroke="#1e82e0" stroke-width="${fs * 0.07}"/>`)
    datum.push(`<text x="${x}" y="${-fs * 1.4}" text-anchor="middle" font-size="${fs * 0.72}" fill="#1e82e0">${x}</text>`)
  }
  for (const y of ys) {
    datum.push(`<line x1="${-fs * 0.3}" y1="${y}" x2="${-fs * 1.1}" y2="${y}" stroke="#1e82e0" stroke-width="${fs * 0.07}"/>`)
    datum.push(`<text x="${-fs * 1.4}" y="${tidy(y + fs * 0.25)}" text-anchor="end" font-size="${fs * 0.72}" fill="#1e82e0">${y}</text>`)
  }
  out.push(`<g>${datum.join('')}</g>`)

  // ── overall dimensions ──
  const dimY = H + fs * 2.4
  const dimX = -fs * 4.4
  out.push(
    `<g stroke="#1c1c1c" stroke-width="${fs * 0.08}" fill="#1c1c1c">` +
      `<line x1="0" y1="${dimY}" x2="${W}" y2="${dimY}"/><line x1="0" y1="${dimY - fs * 0.5}" x2="0" y2="${dimY + fs * 0.5}"/>` +
      `<line x1="${W}" y1="${dimY - fs * 0.5}" x2="${W}" y2="${dimY + fs * 0.5}"/>` +
      `<text x="${W / 2}" y="${dimY - fs * 0.4}" text-anchor="middle" stroke="none">${W} mm</text>` +
      `<line x1="${dimX}" y1="0" x2="${dimX}" y2="${H}"/><line x1="${dimX - fs * 0.5}" y1="0" x2="${dimX + fs * 0.5}" y2="0"/>` +
      `<line x1="${dimX - fs * 0.5}" y1="${H}" x2="${dimX + fs * 0.5}" y2="${H}"/>` +
      `<text transform="translate(${dimX - fs * 0.5} ${H / 2}) rotate(-90)" text-anchor="middle" stroke="none">${H} mm</text></g>`,
  )

  const roleLine = NAV_ROLES.map((role) => `${ROLE_TAG[role].toLowerCase()} ${design.buttons.find((b) => b.uid === design.nav[role])!.id + 1}`).join(', ')
  out.push(
    `<text x="${W}" y="${H + fs * 5.2}" text-anchor="end" font-size="${fs * 0.8}" fill="#444">` +
      `Front view · dimensions in mm · button holes ø${diameter} are ASSUMED, measure your parts · ` +
      `blue dashed = navigation (${esc(roleLine)}) · numbers are printed button numbers (id + 1)</text>`,
  )
  out.push(
    `<text x="${W}" y="${H + fs * 6.6}" text-anchor="end" font-size="${fs * 0.8}" font-weight="700" fill="#7a4a00">` +
      `Do not drill from this drawing until you have checked every hole against the real part.</text>`,
  )
  out.push('</svg>')
  return out.join('')
}
