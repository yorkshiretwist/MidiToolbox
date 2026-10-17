# MIDI-Toolbox Config Editor

A purely client-side GUI for authoring MIDI-Toolbox device configuration files
(the `Config → Modes → Banks → Patches → Buttons → Actions` hierarchy defined in
[`../config/schema.json`](../config/schema.json)). In the spirit of the Blackstar
Live Logic editor: navigate the tree, edit each node in a form, reorder and
duplicate items, and save the `.json` straight back to disk.

## Stack

- **Vue 3 + Vite + TypeScript**, `<script setup>` single-file components.
- **Pinia** — one store ([`src/stores/config.ts`](src/stores/config.ts)) holds the
  parsed config as the single source of truth, plus the current selection path
  and dirty flag. Validation is a getter.
- **ajv** — [`src/lib/validate.ts`](src/lib/validate.ts) validates the config live
  against the schema and indexes issues by JSON-pointer path so they can be shown
  on the offending tree node.
- **vuedraggable** — for reordering modes, banks and patches and swapping buttons.
- **File System Access API** — [`src/lib/fileIo.ts`](src/lib/fileIo.ts) opens and
  saves `.json` in place on Chromium; falls back to upload / download elsewhere.
- No backend. Builds to a static site (deployable to GitHub Pages).

## Scripts

```bash
npm install
npm run dev          # dev server (runs sync-schema first)
npm run test         # vitest, watch
npm run test:run     # vitest, once
npm run build        # type-check + production build to dist/
npm run typecheck    # vue-tsc only
```

### Schema syncing

The canonical schema lives at `../config/schema.json`. `scripts/sync-schema.mjs`
copies it to `src/generated/schema.json` (committed) so the build has no path
dependency outside `Editor/`. It runs automatically before `dev`, `build`, and
`test`. Run it by hand with `npm run sync-schema` after editing the schema.

## Using it

See the [Editor guide](../docs/editor-guide.md). The same app is bundled in the
[desktop app](../docs/desktop-app-guide.md).
