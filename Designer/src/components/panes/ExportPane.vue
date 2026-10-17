<script setup lang="ts">
// Step 6's sidebar pane: the final checks plus downloads for the drawing, schematic, parts list, firmware header, device.json and the design file.
import { computed } from 'vue'
import CheckList from '../CheckList.vue'
import Notice from '../Notice.vue'
import PaneShell from './PaneShell.vue'
import { downloadBytes, downloadSvgAsPng, downloadText } from '../../lib/download'
import { buildZip, zipEntries } from '../../lib/export/artifacts'
import { fileStem } from '../../lib/export/xml'
import { buttonDiameter, screenDims } from '../../lib/geometry'
import { useDesignStore } from '../../stores/design'

const store = useDesignStore()
const blocked = computed(() => store.analysis.blocked)
const stem = computed(() => fileStem(store.design.name))
const art = computed(() => store.artifacts)
const hole = computed(() => buttonDiameter(store.design.purpose))
const cutout = computed(() => screenDims(store.screenSpec, store.design.screen.orientation).module)

type Tab = 'drawing' | 'schematic' | 'parts' | 'firmware'
const preview = (tab: Tab) => {
  store.previewTab = tab
}

const png = (name: string, svg: string) => () => downloadSvgAsPng(`${stem.value}-${name}.png`, svg)
const zip = () => downloadBytes(`${stem.value}.zip`, buildZip(zipEntries(art.value, stem.value)) as Uint8Array<ArrayBuffer>, 'application/zip')
</script>

<template>
  <PaneShell :n="6" title="Export">
    <CheckList :checks="store.analysis.checks" />
    <Notice v-if="blocked" kind="error">Fix the ✗ items to enable downloads. Warnings (⚠) never block them.</Notice>

    <hr class="pane__divider" style="margin: var(--space-1) 0" />

    <Notice kind="warn">
      <strong>Measure your parts before you drill.</strong>
      The hole sizes in these files are placeholders, not measurements: ø{{ hole }} mm for each button and a
      {{ cutout[0] }} × {{ cutout[1] }} mm cut-out for the screen. Parts differ from maker to maker, so measure your own
      buttons and screen module, correct the drawing, and test-fit on scrap before cutting the real panel.
    </Notice>

    <div class="artifact">
      <div class="artifact__head">
        <span class="artifact__name">Design drawing</span>
        <span class="artifact__desc">to-scale front view with hole positions</span>
        <button type="button" class="artifact__link" @click="preview('drawing')">Preview</button>
      </div>
      <p class="artifact__warn">⚠ Hole sizes are placeholders. Measure your parts before you drill.</p>
      <div class="artifact__actions">
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText(`${stem}-drawing.svg`, art.drawingSvg, 'image/svg+xml')">Download SVG</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="png('drawing', art.drawingSvg)()">Download PNG</button>
      </div>
    </div>

    <div class="artifact">
      <div class="artifact__head">
        <span class="artifact__name">Wiring schematic</span>
        <span class="artifact__desc">not to scale</span>
        <button type="button" class="artifact__link" @click="preview('schematic')">Preview</button>
      </div>
      <div class="artifact__actions">
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText(`${stem}-schematic.svg`, art.schematicSvg, 'image/svg+xml')">Download SVG</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="png('schematic', art.schematicSvg)()">Download PNG</button>
      </div>
    </div>

    <div class="artifact">
      <div class="artifact__head">
        <span class="artifact__name">Parts list</span>
        <span class="artifact__desc">quantities from your design</span>
        <button type="button" class="artifact__link" @click="preview('parts')">Preview</button>
      </div>
      <div class="artifact__actions">
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText(`${stem}-parts.csv`, art.partsCsv, 'text/csv')">Download CSV</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText(`${stem}-parts.md`, art.partsMarkdown, 'text/markdown')">Download Markdown</button>
      </div>
    </div>

    <div class="artifact">
      <div class="artifact__head">
        <span class="artifact__name">Firmware files</span>
        <span class="artifact__desc">device.json for the SD card, design file, optional header, build guide</span>
        <button type="button" class="artifact__link" @click="preview('firmware')">Preview</button>
      </div>
      <div class="artifact__actions">
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText('device.json', art.deviceJson, 'application/json')">device.json</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText(`${stem}-design.json`, art.designJson, 'application/json')">Design file</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText('MidiToolboxDevice.h', art.header, 'text/x-c')">Header</button>
        <button type="button" class="btn btn--blue btn--small" :disabled="blocked" @click="downloadText('BUILD.md', art.buildGuide, 'text/markdown')">Build guide</button>
      </div>
    </div>

    <template #footer>
      <button type="button" class="btn btn--revert" @click="store.goTo(5)">Back</button>
      <button type="button" class="btn btn--save" :disabled="blocked" @click="zip">Download all (.zip)</button>
    </template>
  </PaneShell>
</template>
