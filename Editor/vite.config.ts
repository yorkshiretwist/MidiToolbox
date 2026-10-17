/// <reference types="vitest/config" />
// Vite and Vitest configuration. The base path comes from the environment so the app can be deployed under a subpath.
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  // Static deploy under a subpath (e.g. GitHub Pages at /MidiToolbox/).
  // Override at build time:  EDITOR_BASE=/MidiToolbox/editor/ npm run build
  base: process.env.EDITOR_BASE ?? '/',
  plugins: [vue()],
  test: {
    environment: 'jsdom',
    globals: true,
  },
})
