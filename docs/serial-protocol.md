# USB serial protocol

How the desktop app manages a MIDI Toolbox over USB: read and write files on the SD card
(`device.json`, config files), restart the unit, and put it into the Teensy bootloader so
firmware can be flashed. The firmware side is `MIDI-Toolbox/SerialProtocol.h`.

The unit appears as an ordinary USB serial port (115200 baud; the speed is not important on USB).
Use USB Type **Serial + MIDI** when compiling.

## Framing

- Every command is one line of text ending in `\n` (a `\r` before it is ignored). A line may be at
  most 159 characters.
- Every reply starts with `OK`, `ERR` or `READY`, or is a `F ` file line in a `LIST` reply.
- **Ignore every other line.** The firmware's log messages start with `# `, and the screen driver
  prints a few unprefixed lines at boot. A reply can arrive after some of these.
- A file path is always the **rest of the line**, so it may contain spaces
  (`GET /configs/Live Rig.json`).
- File contents are sent as raw bytes straight after the line that announces them.
- Any failure replies `ERR <message>`.

## Commands

| Command | Reply |
| --- | --- |
| `HELLO` | `OK HELLO MidiToolbox 1 sd=1` - protocol name, version, whether the SD card started |
| `INFO` | `OK {"firmware":"0.1.0","protocol":1,"device":"Live Rig Unit","deviceSource":"file","buttons":13,"sd":true,"safeMode":false,"config":"Live Rig.json","buttonsActive":false}` |
| `LIST <dir>` | `F <size> <name>` for each file, then `OK <count>`. `<dir>` is `/` or `/name`. A missing directory is empty (`OK 0`). The temporary upload file and sub-directories are not listed |
| `GET <path>` | `OK <size> <crc32 hex>`, then `<size>` raw bytes |
| `PUT <size> <crc32 hex> <path>` | `READY`, then send `<size>` raw bytes, then `OK` (or `ERR`) |
| `DELETE <path>` | `OK` |
| `REBOOT [force]` | `OK`, then the unit restarts. See "Restarting safely" |
| `BOOTLOADER [force]` | `OK`, then the unit reboots into the Teensy bootloader, ready for a new `.hex` |

`deviceSource` in `INFO` is `file` (wiring read from `/device.json`), `built-in` (wiring compiled
in from `MidiToolboxDevice.h`) or `none` (safe mode).

### Uploading safely

`PUT` writes the bytes to a temporary file, checks the CRC-32 (the standard zlib/PNG one) against
the one in the command, and only then replaces the real file. A wrong CRC, a dropped connection
(no data for 3 seconds) or a full card leaves the existing file untouched. Files up to 256 KB are
accepted. The directory (for example `/configs`) is created if it does not exist.

The unit is busy for the few milliseconds an upload takes, so button presses are read a moment
late. Reads (`GET`, `LIST`) are just as brief.

### Restarting safely

A restart would never send the note-offs for anything still on, leaving notes stuck on the synth.
So while any button is on (held, latched on, or switched on by default, on any page of the current
patch) `REBOOT` and `BOOTLOADER` reply `ERR busy: buttons are active (send "force" to release them
and restart)`. `INFO` reports this as `"buttonsActive": true`.

Send `REBOOT force` (or `BOOTLOADER force`) to release everything first: the unit turns every
active button off as if it had been pressed again, which sends the note-offs and any release
actions, then restarts. An app should ask the player before doing this.

### Which paths are allowed

`/name.ext` or `/dir/name.ext` only: letters, digits, space, `.`, `_` and `-`, at most one
sub-directory, no `..`. `/UPLOAD.TMP` is reserved. Anything else replies `ERR bad path`.

## Safe mode

If `/device.json` is missing or invalid the unit drives no pin at all, but it still answers this
protocol, so the app can upload a correct `device.json` and send `REBOOT`. `INFO` reports
`"safeMode":true`.

## Flashing firmware

1. `BOOTLOADER`. The unit leaves the serial port list and appears as a Teensy HID device
   (USB `16c0:0478`).
2. Run the Teensy loader with the new `.hex`.
3. The serial port reappears (possibly with a different name) when the new firmware starts.

If the firmware is not responding, opening the serial port at 134 baud also reboots a Teensy into
its bootloader. The PROGRAM button always works.
