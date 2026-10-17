# Security policy

## Reporting a vulnerability

Please **do not open a public issue** for a security problem. Instead, use GitHub's private
reporting: on the repository's **Security** tab choose **Report a vulnerability**. If that is not
available, open an issue titled "Security concern" **without any details** and I will arrange a
private way for you to share them.

Include what you found, how to reproduce it and the version you tested. This is a hobby project run
by one person, so I cannot promise a response time, but I will try to acknowledge a report within
a week and to fix real problems quickly.

## What is in scope

- The **desktop app** (`App/`): for example, anything that lets a web page, a config file or a
  `device.json` run code, read files, or reach the serial port without the user's say-so.
- The **Editor and Designer** web apps: for example, a crafted config or design file that runs
  script.
- The **firmware** (`MIDI-Toolbox/`): for example, a crafted file on the SD card, or a command over
  the USB serial link, that crashes the unit or corrupts its storage.
- The **GitHub workflows**: anything that could leak secrets or publish a tampered release.

## Not in scope

- Physical safety of a unit you build. This is not a security issue; see the warnings in the
  [README](README.md) and the [hardware guide](docs/hardware-build.md).
- Problems in third-party software (Electron, serialport and so on) that are already reported
  upstream. Tell me if I should update to a fixed version.

## Supported versions

Only the latest release is supported.
