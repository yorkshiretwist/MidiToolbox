# Vendored source

## teensy_loader_cli

The open-source command-line programmer for Teensy boards, by PJRC (the maker of the Teensy). The app
runs it to install firmware. It is licensed under the GNU General Public License version 3, the same
as this project.

Upstream: https://github.com/PaulStoffregen/teensy_loader_cli

`teensy_loader_cli/` holds these files, unmodified, from upstream commit
`03fca4156c244c7ad36bd368cf6e24531dbd566a` (27 February 2024):

- `teensy_loader_cli.c` - the program
- `Makefile` - upstream's build recipe (`scripts/build-loader.mjs` uses the same compiler flags)
- `README.md` and `gpl3.txt` - upstream's notes and the licence text

The sample `.hex` files and other systems' makefiles upstream are not copied. Its licence is GPL version 3.

`npm run loader` (in `App/`) compiles `teensy_loader_cli.c` for the system you are on into
`App/resources/loader/`. The compiled program is shipped inside the app, and this folder is the source
offered with it.
