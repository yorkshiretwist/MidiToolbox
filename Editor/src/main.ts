// Entry point: loads the stylesheets, creates the app with Pinia, and mounts it.
import { createApp } from 'vue'
import { createPinia } from 'pinia'
import './styles/theme.css'
import './styles/layout.css'
import './style.css'
import App from './App.vue'
import { restoreWorkingState } from './lib/persist'

const pinia = createPinia()
restoreWorkingState(pinia)
createApp(App).use(pinia).mount('#app')
