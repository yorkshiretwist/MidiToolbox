// Browser downloads for the generated files.

function saveBlob(filename: string, blob: Blob): void {
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}

export function downloadText(filename: string, text: string, mime = 'text/plain'): void {
  saveBlob(filename, new Blob([text], { type: `${mime};charset=utf-8` }))
}

export function downloadBytes(filename: string, bytes: Uint8Array<ArrayBuffer>, mime: string): void {
  saveBlob(filename, new Blob([bytes], { type: mime }))
}

/** Rasterises an SVG string to a PNG and downloads it. `scale` multiplies the SVG's own size. */
export async function downloadSvgAsPng(filename: string, svg: string, scale = 3): Promise<void> {
  const viewBox = svg.match(/viewBox="([^"]+)"/)?.[1]?.split(/\s+/).map(Number)
  const [width, height] = viewBox ? [viewBox[2]!, viewBox[3]!] : [900, 600]
  const image = new Image()
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml;charset=utf-8' }))
  try {
    await new Promise<void>((resolve, reject) => {
      image.onload = () => resolve()
      image.onerror = () => reject(new Error('Could not render the SVG'))
      image.src = url
    })
    const canvas = document.createElement('canvas')
    // one SVG unit is one millimetre in the drawing, so this is `scale` px per mm
    canvas.width = Math.round(width * scale)
    canvas.height = Math.round(height * scale)
    const ctx = canvas.getContext('2d')
    if (!ctx) throw new Error('No canvas available')
    ctx.drawImage(image, 0, 0, canvas.width, canvas.height)
    const blob = await new Promise<Blob | null>((resolve) => canvas.toBlob(resolve, 'image/png'))
    if (!blob) throw new Error('Could not encode the PNG')
    saveBlob(filename, blob)
  } finally {
    URL.revokeObjectURL(url)
  }
}
