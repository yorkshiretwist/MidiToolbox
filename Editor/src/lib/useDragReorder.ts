// Drag one row of a list onto another. Shared by the button list on a Patch and
// the mode / bank / patch lists: grab a row by its grip and drop it on the row
// it should go to; the pane auto-scrolls while the drag hovers near (or past)
// its top or bottom edge, and dropping anywhere else does nothing.
//   'swap' - the two rows exchange places (buttons: it swaps their footswitch slots).
//   'move' - the dragged row takes the target row's position and the rows in
//            between shift by one, so dragging item 1 onto item 3 of five gives
//            2, 3, 1, 4, 5; a line shows which side of the target it lands on.
//
// Usage in a pane component:
//   const drag = useDragReorder((from, to) => store.moveItem(parent, from, to), 'move')
//   <section :ref="drag.paneEl" @dragover="drag.onDragOverPane" @drop.prevent="drag.endDrag" @dragend="drag.endDrag">
//     <button draggable="true" :class="drag.rowClasses(i)"
//             @dragstart="drag.onDragStart(i, $event)" @dragover="drag.onDragOverRow(i, $event)"
//             @drop="drag.onDropRow(i, $event)" @dragend="drag.endDrag">

import { onBeforeUnmount, ref } from 'vue'

const AUTOSCROLL_EDGE = 56 // px band at each edge that triggers scrolling
const AUTOSCROLL_MAX_SPEED = 20 // px per frame at the very edge / past it

export function useDragReorder(onDrop: (from: number, to: number) => void, placement: 'swap' | 'move' = 'swap') {
  // Index of the row being dragged, and of the one currently under the pointer
  // as a drop target. Null when idle.
  const dragIndex = ref<number | null>(null)
  const dropIndex = ref<number | null>(null)

  // The pane element — the scroll container we auto-scroll during a drag.
  const paneEl = ref<HTMLElement | null>(null)

  // Latest pointer Y while dragging, tracked window-wide so auto-scroll keeps
  // going even when the pointer leaves the pane below the window edge.
  const pointerY = ref(0)
  let rafId: number | null = null

  function autoScrollTick() {
    if (dragIndex.value === null) {
      rafId = null
      return
    }
    const el = paneEl.value
    if (el) {
      const rect = el.getBoundingClientRect()
      const y = pointerY.value
      let dy = 0
      if (y > rect.bottom - AUTOSCROLL_EDGE) {
        const over = (y - (rect.bottom - AUTOSCROLL_EDGE)) / AUTOSCROLL_EDGE
        dy = AUTOSCROLL_MAX_SPEED * Math.min(1, over)
      } else if (y < rect.top + AUTOSCROLL_EDGE) {
        const over = (rect.top + AUTOSCROLL_EDGE - y) / AUTOSCROLL_EDGE
        dy = -AUTOSCROLL_MAX_SPEED * Math.min(1, over)
      }
      if (dy !== 0) el.scrollTop += dy
    }
    rafId = requestAnimationFrame(autoScrollTick)
  }

  function trackPointer(event: DragEvent) {
    pointerY.value = event.clientY
  }

  function onDragStart(index: number, event: DragEvent) {
    dragIndex.value = index
    dropIndex.value = null
    pointerY.value = event.clientY
    if (event.dataTransfer) {
      event.dataTransfer.effectAllowed = 'move'
      // Firefox only starts a drag once some data is set.
      event.dataTransfer.setData('text/plain', String(index))
    }
    // Keep the pointer position fresh anywhere in the window (dragover on the
    // pane alone goes stale once the pointer drops below it).
    document.addEventListener('dragover', trackPointer)
    if (rafId === null) rafId = requestAnimationFrame(autoScrollTick)
  }

  // Over another row: allow the drop and mark it as the target. Stop the event
  // here so the pane-level handler below does not clear `dropIndex` again.
  function onDragOverRow(index: number, event: DragEvent) {
    if (dragIndex.value === null) return
    event.stopPropagation()
    if (index === dragIndex.value) {
      dropIndex.value = null
      return
    }
    event.preventDefault()
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'move'
    dropIndex.value = index
  }

  function onDropRow(index: number, event: DragEvent) {
    event.preventDefault()
    if (dragIndex.value !== null && dragIndex.value !== index) {
      onDrop(dragIndex.value, index)
    }
    endDrag()
  }

  // Anywhere else in the pane: still let the drag continue (so auto-scroll works
  // over the gaps and headings) but clear the target — a drop here does nothing.
  function onDragOverPane(event: DragEvent) {
    if (dragIndex.value === null) return
    event.preventDefault()
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'move'
    dropIndex.value = null
  }

  function endDrag() {
    dragIndex.value = null
    dropIndex.value = null
    document.removeEventListener('dragover', trackPointer)
    if (rafId !== null) {
      cancelAnimationFrame(rafId)
      rafId = null
    }
  }

  /**
   * Classes for a draggable row: the grab cursor, fading while dragged, and the
   * live target marked — an accent ring for a swap, a line below (dragging down)
   * or above (dragging up) the target for a move.
   */
  function rowClasses(index: number) {
    const isTarget = dropIndex.value === index
    const below = dragIndex.value !== null && dragIndex.value < index
    return {
      'pane-child--draggable': true,
      'pane-child--dragging': dragIndex.value === index,
      'pane-child--drop-target': isTarget && placement === 'swap',
      'pane-child--drop-after': isTarget && placement === 'move' && below,
      'pane-child--drop-before': isTarget && placement === 'move' && !below,
    }
  }

  onBeforeUnmount(endDrag)

  return { paneEl, onDragStart, onDragOverRow, onDropRow, onDragOverPane, endDrag, rowClasses }
}
