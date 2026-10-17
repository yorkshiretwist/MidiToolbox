// Tests for the design store, currently button deletion.
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useDesignStore } from './design'
import { performanceButtons } from '../lib/layout'

describe('deleting buttons', () => {
  beforeEach(() => setActivePinia(createPinia()))

  it('deletes any button, including ones outside the unit', () => {
    const store = useDesignStore()
    store.setUnitSize(200, 140)
    const outside = performanceButtons(store.design).find((b) => b.x > 200)!
    const before = store.design.buttons.length
    store.deleteButton(outside.uid)
    expect(store.design.buttons).toHaveLength(before - 1)
    expect(store.design.buttons.some((b) => b.uid === outside.uid)).toBe(false)
  })

  it('hands a deleted navigation button its role to the nearest performance button', () => {
    const store = useDesignStore()
    const uid = store.design.nav.select
    store.deleteButton(uid)
    const nav = Object.values(store.design.nav)
    expect(new Set(nav).size).toBe(3)
    expect(nav.every((u) => store.design.buttons.some((b) => b.uid === u))).toBe(true)
  })

  it('can go below 3 performance buttons; the checks flag it instead', () => {
    const store = useDesignStore()
    while (performanceButtons(store.design).length > 2) store.deleteButton(performanceButtons(store.design)[0]!.uid)
    expect(performanceButtons(store.design)).toHaveLength(2)
    expect(store.analysis.checks.find((c) => c.key === 'perf')!.severity).toBe('error')
  })
})
