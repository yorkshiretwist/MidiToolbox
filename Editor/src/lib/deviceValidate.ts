// Parsing and validating a device profile (-design.json) against
// config/design-schema.json — the same file format the Designer app writes.
// Lighter than the Designer's own parseDevice(): the Editor has no catalogue
// of screens/microcontrollers to cross-check against, so it only confirms the
// file is structurally valid and internally consistent (ids unique, the three
// navigation roles point at real buttons).

import Ajv from 'ajv'
import schema from '../generated/design-schema.json'
import type { DeviceFile, NavRole } from '../types/device'

const NAV_ROLES: NavRole[] = ['select', 'down', 'up']

const ajv = new Ajv({ allErrors: true, strict: false })
const validateFn = ajv.compile<DeviceFile>(schema)

export type ParseDeviceResult =
  | { device: DeviceFile; errors?: undefined }
  | { device?: undefined; errors: string[] }

export function parseDeviceFile(text: string): ParseDeviceResult {
  let data: unknown
  try {
    data = JSON.parse(text)
  } catch (error) {
    return { errors: [`Not valid JSON: ${(error as Error).message}`] }
  }
  if (!validateFn(data)) {
    const errors = (validateFn.errors ?? []).map((e) => `${e.instancePath || '/'} ${e.message ?? 'is invalid'}`)
    return { errors: errors.slice(0, 6) }
  }
  const device = data

  const errors: string[] = []
  const ids = device.buttons.map((b) => b.id)
  if (new Set(ids).size !== ids.length) errors.push('Two buttons share the same id')
  const navIds = NAV_ROLES.map((role) => device.navigation[role])
  if (new Set(navIds).size !== 3) errors.push('The three navigation roles must be three different buttons')
  for (const id of navIds) {
    if (!ids.includes(id)) errors.push(`Navigation refers to button id ${id}, which does not exist`)
  }
  if (errors.length > 0) return { errors }

  return { device }
}
