// Text escaping for the generated SVG (a design name can contain anything).

export function esc(text: string | number): string {
  return String(text)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
}

/** Escapes text for a C string literal. */
export function cString(text: string): string {
  return text.replaceAll('\\', '\\\\').replaceAll('"', '\\"').replaceAll('\n', ' ')
}

/** Filesystem-friendly version of a design name. */
export function fileStem(name: string): string {
  const stem = name
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')
  return stem || 'midi-toolbox-unit'
}
