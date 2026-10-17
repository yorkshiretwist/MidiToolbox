# How navigation works

The unit is a live instrument, so moving around the Config > Mode > Bank > Patch hierarchy must
never cut off sound that is playing. This page describes the behaviour; the code is in
`MIDI-Toolbox/Navigation.h` and `Runtime.h`, and the [Editor's Play mode](editor-guide.md#trying-it-out-play-mode)
behaves the same way.

## The core rule: browsing is free, confirming is what changes things

- **Cycle** actions (next/previous Patch, Bank, Mode, Config, and stepping a parameter value) only
  move a **pending** selection. The affected text on the screen flashes. Nothing live changes, so
  held notes and latched buttons keep sounding.
- The change happens when you **confirm** it. Only then are the outgoing selection's buttons turned
  off (sending note-offs and release actions) and the new selection's default-on buttons fired. Every
  note-off comes before every note-on.
- **Set** actions (`Set patch`, `Set bank`, `Set mode`, `Set config`) are direct jumps. They apply
  **immediately**, with the same clean teardown. They are meant for performance buttons.
- **Page** changes are always immediate and never need confirming, because flipping a page tears
  nothing down.
- Choosing which level the arrows steer (**Set / Cycle Parameter**) has no side effects at all.

| Kind | Actions | Takes effect |
| --- | --- | --- |
| Cycle | Cycle Patch / Bank / Mode / Config, Cycle Parameter Value | when confirmed |
| Set | Set Patch / Bank / Mode / Config, Set Parameter Value | at once |
| Page | Cycle Page, Set Page, stepping the Page parameter | at once |
| Selector | Cycle Parameter, Set Parameter | at once, changes nothing live |

MIDI channel follows the same split: cycling it is pending, a `Set` is immediate. A button's
role is never assumed from its position: any action can be put on any button, and a `Set patch` on
button 2 is immediate while a `Cycle patch` on button 2 is pending.

## The three navigation buttons

By default the last three buttons (ids 10-12 on the 13-button unit, printed 11-13) are the built-in
navigation controls, and the on-screen `◀ Parameter ▶` row mirrors them.

| Input | Nothing pending | A change is pending |
| --- | --- | --- |
| **Select** (tap) | choose the next level to steer: Config, Mode, Bank, Patch, MIDI channel, Page (a mode's `parameters` list can limit this) | **Set?** - confirm the change |
| **Select** (hold 500 ms) | nothing | **Revert** - discard the pending change. Fires when you let go |
| **Step down / up** | change the pending value of the chosen level, wrapping round; greyed out if that level has only one option | step it further |

Details worth knowing:

- While a change is pending the selector is **locked** (the middle button reads **Set?**), so you
  navigate top-down: choose the Mode, then its Bank, then its Patch, then confirm or revert.
- Moving to a different Mode or Bank lands on its first Bank and Patch. Going back to what is
  currently live puts the live Bank and Patch back, so a round trip clears the pending flag by
  itself without needing Revert.
- There is **no timeout**. A pending change waits until you confirm or revert it.
- Pressing any performance button (0-9) while a change is pending **confirms** it first, if that
  button only plays notes, sends program changes or control changes; the button then runs on the new
  patch. A button carrying a `Set` jump replaces the pending change; a button carrying a `Cycle`
  extends it.
- A config may give any of the three buttons its own actions, and the Editor warns before it lets
  you. That changes only the physical footswitch: the on-screen row always keeps the built-in
  behaviour, so you can never lock yourself out of navigation. Hold-to-revert is a footswitch
  gesture only while the button has its built-in role; on screen, **Revert** is not available from
  the touch buttons.
- Changing only the MIDI channel needs no teardown, so confirming it just switches the channel.

## What the screen shows

- The Config, Mode, Bank, Patch and channel readouts flash while their level (or an outer one) has a
  pending change: a new Patch flashes Patch; a new Bank flashes Bank and Patch; a new Mode flashes
  Mode, Bank and Patch; a new Config flashes all four.
- The middle navigation button shows the chosen level's name, or **Set?** when something is pending.
- A button that sends MIDI shows its name over the screen for at least half a second (a Momentary
  button's name stays while it is held; a Latched or Exclusive button reads `Name ON` / `Name OFF`).
  Buttons that only navigate show nothing, and it is hidden while a change is pending.
- A Bank can use the large-type **Live** layout (`screenLayout`): Bank and Patch in big letters (and
  the Page, if the patch has more than one), the navigation row along the bottom, no button grid. The
  default is **Grid**. The live bank decides the layout. The big fonts need the matching sizes
  enabled in `lv_conf.h`, otherwise the nearest size is used.

## Pages

A button has a `page` number (default 1) so that one Patch can define more buttons than the unit has.
Page 0 means "show on every page". Only the patch's own buttons are filtered by page; Bank, Mode
and Config buttons show on every page.

- Entering a patch always starts on page 1, and fires the default-on buttons of **every** page once.
  A patch with more than one page also moves the selector onto **Page**, so Step down/up flip pages
  straight away.
- Leaving the patch releases every button on every page in one teardown. Pages are not remembered.
- The Patch readout gains a page suffix (`My Patch / 2`) when the live patch has several pages.
- **Limitation:** an Exclusive set only cancels siblings on the *same page*. Keep one
  mutually-exclusive group (an amp channel, say) on a single page.
- The old `bankType: PersistentButtons` no longer does anything; a bank with it behaves as an
  ordinary bank. Pages replaced the idea.

## Keeping a button on through a patch change

A bank can set `maintainActiveButtonOnPatchChange` (default off). When you change Patch **within the
same bank**, every button that was on is turned off as usual (note-offs sent), then turned on again
using the *new* patch's definition of that button id. This suits a held chord whose voicing changes
between sibling patches. It does not apply to Bank, Mode or Config changes.

Limitation: if the new patch has no button with that id on page 1 (or page 0), the button is
dropped rather than restored. Keep the id present in every patch of the bank.

## Where a Set action can reach

`Set patch`, `Set bank` and `Set mode` give their target as indices, innermost first, naming only
the outer levels they need:

| Action | Names | Target |
| --- | --- | --- |
| Set patch | patch | a patch of the current bank |
| | bank, patch | a patch of another bank of the current mode |
| | mode, bank, patch | a patch of another mode |
| Set bank | bank | a bank of the current mode |
| | mode, bank | a bank of another mode |
| Set mode | mode | a mode |

A Set action lands on the default of whatever it does not name: a bank opens on its default patch, a
mode on its first bank's default patch, and `Set config` on that config's first Mode, Bank and Patch.
A target that does not exist logs a warning and does nothing.

The Editor shows targets as paths (**Bank > Patch**, **Mode > Bank > Patch**) and stores the
shortest form that is unambiguous for where the button lives. A button on a Mode or the File runs in
more than one bank, so it must name its bank (and mode) in full; the Editor flags the short form
there.

**Pressing a Set that lands where you already are is a reset, not a no-op.** Every held note and CC
is released, every button returns to its default status, the page returns to 1 and any pending
change is discarded. (`maintainActiveButtonOnPatchChange` does not apply to a reset.) If the Set
names the bank or mode you are in but you are on a different patch, it takes you "home" to its
landing patch.

The one exception is a Set reached from inside a selection pass, such as a default-on button whose
own Set lands on the selection being set up. That does nothing, otherwise it would tear down and
re-seed forever.