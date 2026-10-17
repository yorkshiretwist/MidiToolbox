// Live validation of a device config — structural (against config/schema.json)
// plus a second, profile-aware pass checking every button id actually exists
// on the active device. Both report through the same ValidationResult shape,
// so a profile mismatch shows up in the UI exactly like a schema error.

import Ajv, { type ErrorObject } from 'ajv'
import addFormats from 'ajv-formats'
import schema from '../generated/schema.json'
import type { Action, Button, DeviceConfig } from '../types/config'
import type { DeviceFile } from '../types/device'
import {
  isIndexedTargetType,
  scopeOfPath,
  targetKeys,
  targetProblem,
  type TargetScope,
} from './actionTargets'

const ajv = new Ajv({ allErrors: true, strict: false })
addFormats(ajv)

const validateFn = ajv.compile<DeviceConfig>(schema)

export interface ValidationIssue {
  /** ajv instancePath, e.g. "/modes/0/banks/1/patches/0/buttons/2/id". */
  path: string
  message: string
  keyword: string
  params: Record<string, unknown>
}

export interface ValidationResult {
  valid: boolean
  issues: ValidationIssue[]
  /** issues grouped by instancePath for quick lookup while rendering the tree. */
  byPath: Map<string, ValidationIssue[]>
}

function toIssue(err: ErrorObject): ValidationIssue {
  const path =
    err.instancePath ||
    (err.keyword === 'required' ? `/${String(err.params.missingProperty)}` : '')
  return {
    path,
    message: err.message ?? 'invalid',
    keyword: err.keyword,
    params: err.params as Record<string, unknown>,
  }
}

function idIssue(path: string, id: number): ValidationIssue {
  return {
    path,
    message: `must be a button id that exists on this device (got ${id})`,
    keyword: 'deviceButtonId',
    params: { id },
  }
}

interface ButtonContainer {
  path: string
  buttons: Button[]
}

/**
 * Every `buttons` array in the tree, file through patch, each with its
 * ajv-style instancePath. Called only once ajv has already confirmed the
 * structure (required arrays present, etc.), so the required fields
 * (modes/banks/patches/patch.buttons) are trusted without further guards —
 * only the optional file/mode/bank `buttons` need an existence check.
 */
function* buttonContainers(config: DeviceConfig): Generator<ButtonContainer> {
  if (config.buttons) yield { path: '/buttons', buttons: config.buttons }
  for (const [mi, mode] of config.modes.entries()) {
    if (mode.buttons) yield { path: `/modes/${mi}/buttons`, buttons: mode.buttons }
    for (const [bi, bank] of mode.banks.entries()) {
      if (bank.buttons) yield { path: `/modes/${mi}/banks/${bi}/buttons`, buttons: bank.buttons }
      for (const [pi, patch] of bank.patches.entries()) {
        yield { path: `/modes/${mi}/banks/${bi}/patches/${pi}/buttons`, buttons: patch.buttons }
      }
    }
  }
}

/**
 * "Is this id actually wired on the active device" — structurally separate
 * from ajv's schema check (config/schema.json only bounds button ids loosely,
 * since it has no way to know any device's real button count). Checks both a
 * button's own id and any ids it names in an Exclusive buttonSet.
 */
function validateAgainstDevice(config: DeviceConfig, device: DeviceFile): ValidationIssue[] {
  const validIds = new Set(device.buttons.map((b) => b.id))
  const issues: ValidationIssue[] = []
  for (const { path, buttons } of buttonContainers(config)) {
    buttons.forEach((button, i) => {
      if (!validIds.has(button.id)) issues.push(idIssue(`${path}/${i}/id`, button.id))
      button.buttonSet?.forEach((id, si) => {
        if (!validIds.has(id)) issues.push(idIssue(`${path}/${i}/buttonSet/${si}`, id))
      })
    })
  }
  return issues
}

/**
 * SetPatch / SetBank / SetMode must point at something that exists: the full
 * target (mode, bank and patch as far as the action names them) has to resolve
 * in the tree. The schema can only say "an integer >= 0"; this is what catches
 * a target that went stale when its patch/bank/mode was deleted or moved.
 * See lib/actionTargets.ts for the forms a target can take.
 */
function validateActionTargets(config: DeviceConfig): ValidationIssue[] {
  const issues: ValidationIssue[] = []
  const check = (actionPath: string, action: Action, scope: TargetScope) => {
    if (!isIndexedTargetType(action.type)) return
    const params = action.actionParameters ?? {}
    // Nothing named at all: the firmware reads that as index 0, and the
    // editor always fills one in — leave hand-written files alone.
    if (targetKeys(action.type).every((key) => params[key] === undefined)) return
    const message = targetProblem(action.type, params, config, scope)
    if (message) {
      const key = targetKeys(action.type).find((k) => params[k] !== undefined) ?? 'actionParameters'
      issues.push({
        path: `${actionPath}/actionParameters/${key}`,
        message,
        keyword: 'actionTarget',
        params: { ...params },
      })
    }
  }
  for (const { path, buttons } of buttonContainers(config)) {
    const scope = scopeOfPath(path)
    buttons.forEach((button, bi) =>
      button.actions?.forEach((action, ai) => check(`${path}/${bi}/actions/${ai}`, action, scope)),
    )
  }
  // A preset has no single location, so its targets must be fully qualified —
  // the same as a File-level button's (an empty scope).
  config.presets?.forEach((preset, pi) =>
    preset.actions.forEach((action, ai) => check(`/presets/${pi}/actions/${ai}`, action, {})),
  )
  return issues
}

/**
 * Presets: ids must be unique, and an ExecutePreset action must name a preset that
 * exists. The schema covers the shape (a preset needs an id, a name and at least
 * one action, none of them ExecutePreset — presets can't be nested).
 */
function validatePresets(config: DeviceConfig): ValidationIssue[] {
  const issues: ValidationIssue[] = []
  const ids = new Set<number>()
  config.presets?.forEach((preset, pi) => {
    if (ids.has(preset.id)) {
      issues.push({
        path: `/presets/${pi}/id`,
        message: `must be unique (another preset already has id ${preset.id})`,
        keyword: 'presetId',
        params: { id: preset.id },
      })
    }
    ids.add(preset.id)
  })

  const checkRef = (actionPath: string, action: Action) => {
    if (action.type !== 'ExecutePreset') return
    const id = action.actionParameters?.presetId
    if (id === undefined) {
      issues.push({
        path: `${actionPath}/actionParameters/presetId`,
        message: 'must choose a preset to run',
        keyword: 'presetRef',
        params: {},
      })
    } else if (!ids.has(id)) {
      issues.push({
        path: `${actionPath}/actionParameters/presetId`,
        message: `must be the id of an existing preset (got ${id})`,
        keyword: 'presetRef',
        params: { id },
      })
    }
  }
  for (const { path, buttons } of buttonContainers(config)) {
    buttons.forEach((button, bi) =>
      button.actions?.forEach((action, ai) => checkRef(`${path}/${bi}/actions/${ai}`, action)),
    )
  }
  return issues
}

export function validateConfig(data: unknown, device: DeviceFile): ValidationResult {
  const structurallyValid = validateFn(data)
  const issues = (validateFn.errors ?? []).map(toIssue)
  // The device-aware pass assumes a structurally valid tree (real arrays/ids
  // to walk) — skip it otherwise, the same way ajv's own checks build on
  // each other via allErrors.
  if (structurallyValid) {
    issues.push(...validateAgainstDevice(data as DeviceConfig, device))
    issues.push(...validateActionTargets(data as DeviceConfig))
    issues.push(...validatePresets(data as DeviceConfig))
  }

  const byPath = new Map<string, ValidationIssue[]>()
  for (const issue of issues) {
    const list = byPath.get(issue.path) ?? []
    list.push(issue)
    byPath.set(issue.path, list)
  }
  return { valid: issues.length === 0, issues, byPath }
}

export { schema }
