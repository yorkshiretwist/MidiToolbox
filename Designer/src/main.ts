// Entry point: loads the stylesheets, creates the app, applies optional ?preset/?step/... deep-link parameters, and mounts it.
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import './styles/theme.css'
import './styles/layout.css'
import './styles/designer.css'
import './styles/stage.css'
import App from './App.vue'
import { useDesignStore } from './stores/design'
import { PRESETS } from './lib/presets'

const app = createApp(App).use(createPinia())

// Deep links, handy for sharing a starting point:
//   ?preset=large-foot   ?step=4   ?orientation=portrait   ?tab=schematic   ?zoom=1
const params = new URLSearchParams(location.search)
const store = useDesignStore()
const preset = params.get('preset')
if (preset && PRESETS.some((p) => p.id === preset)) store.startFrom(preset)
if (params.get('orientation') === 'portrait') store.setOrientation('portrait')
const step = Number(params.get('step'))
if (step >= 1 && step <= 6) store.goTo(step)
const tab = params.get('tab')
if (tab === 'drawing' || tab === 'schematic' || tab === 'parts' || tab === 'firmware') store.previewTab = tab

if (params.get('zoom') === '1') store.openZoom()

app.mount('#app')
