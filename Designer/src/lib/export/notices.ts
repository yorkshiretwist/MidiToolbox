// The "measure your parts" warning. Hole and cut-out sizes in every generated
// file are assumptions taken from the catalogue, not measurements of the parts
// a builder actually has, and drilling is not reversible. The same wording is
// used everywhere so nothing understates it.

export const MEASURE_HEADLINE = 'MEASURE YOUR PARTS BEFORE YOU DRILL'

/** The build-at-your-own-risk line, shown with every warning that can end up on a workshop wall. */
export const RISK_WARNING =
  'You build this at your own risk: it comes with no warranty. Verify every connection with a multimeter before you apply power, and measure your parts before you drill or cut.'

/** The full warning, for the drawing and the build guide. */
export function measureWarning(buttonHole: number, cutout: [number, number]): string {
  return (
    `Hole sizes on this design are placeholders, not measurements. The drawing assumes ø${buttonHole} mm holes ` +
    `for the buttons and a ${cutout[0]} × ${cutout[1]} mm cut-out for the screen. Parts vary from maker to maker: ` +
    `measure your own buttons (body or thread diameter) and screen module, change the holes to match, ` +
    `and test-fit on scrap material first. ${RISK_WARNING}`
  )
}

export const EXPERIMENTAL_HEADLINE = 'EXPERIMENTAL - CHECK BEFORE YOU WIRE'

/** The schematic's warning: it is generated, simplified and untested. */
export const EXPERIMENTAL_WARNING =
  'This schematic is generated from your design and has not been tested on real hardware. It is simplified and ' +
  'may be wrong for your parts. Check every connection, including the LED power supply and regulator, against ' +
  "your own parts' datasheets, and with a multimeter, before you apply power. " +
  'You build this at your own risk: it comes with no warranty.'

/** One line, for places with little room (CSV rows, table notes). */
export const MEASURE_SHORT = 'Measure your parts before drilling: hole sizes here are assumptions, not measurements.'

/** Greedy word wrap on a character budget, for SVG text that cannot wrap itself. */
export function wrapText(text: string, maxChars: number): string[] {
  const lines: string[] = []
  let line = ''
  for (const word of text.split(/\s+/)) {
    if (line && line.length + word.length + 1 > maxChars) {
      lines.push(line)
      line = word
    } else {
      line = line ? `${line} ${word}` : word
    }
  }
  if (line) lines.push(line)
  return lines
}
