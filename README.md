Modified version of Mark Grebe's [SDLTRS] with bug fixes and patches:

  * Included all patches by [EtchedPixels]: banked memory support, Lowe LE18
  * Fixed various SegFaults: ROM Selection Menu, Scaling in Fullscreen
  * Reworked the TextGUI: new shortcuts and key bindings, help screen
  * Ctrl-A, Ctrl-C & Ctrl-V can now be used in the Emulator (CP/M & WordStar)
  * Access to real floppy disks works now on Linux
  * Tried to fix reported bugs to the original version
  * Port to SDL2 (see [BUILDING.md])
  * Support for Exatron Stringy Floppy
  * Select and execute CMD files directly in the Emulator
  * Save screenshot of the Emulator window as BMP file
  * Show Z80 registers in the window title bar
  * Adjust speed of Z80 CPU on the fly
  * Emulate Z80 memory refresh register
  * Support for Sprinter II/III speed-up kits
  * Change Z80 CPU default MHz of each TRS-80 Model
  * More accurate emulation of Z80 block instructions

## License

  BSD-2-Clause

## SDL2

This branch contains the SDL2 version with hardware rendering support.
Although *experimental*, it's recommended for newer operating systems ...

This version is available in [RetroPie] since version 4.6.6 and Valerio
Lupi's fork of [RetroPie-Setup] ...

## Binaries

  * [sdl2trs.exe]    (32-bit, needs [SDL2.DLL])
  * [sdl2trs64.exe]  (64-bit, needs [SDL2.DLL])

(Build with MinGW & MinGW-w64)

## Packages

  * [sdl2trs_1.2.14-1_i386.deb]   (32-bit, SDL2)
  * [sdl2trs_1.2.14-1_amd64.deb]  (64-bit, SDL2)

(Build on Debian 9/i386 & Linux Mint 20/amd64)

Thanks to Tércio Martins a package for Arch Linux is available in the [AUR].

## Screenshots

![screenshot](screenshots/sdltrs01.png)
![screenshot](screenshots/sdltrs02.png)
![screenshot](screenshots/sdltrs03.png)
![screenshot](screenshots/sdltrs04.png)

[AUR]: https://aur.archlinux.org/packages/sdl2trs/
[BUILDING.md]: BUILDING.md
[EtchedPixels]: https://www.github.com/EtchedPixels/xtrs
[RetroPie]: https://github.com/RetroPie
[RetroPie-Setup]: https://github.com/valerino/RetroPie-Setup
[SDL2.DLL]: https://www.libsdl.org/download-2.0.php
[SDLTRS]: http://sdltrs.sourceforge.net
[sdl2trs.exe]: bin/sdl2trs.exe
[sdl2trs64.exe]: bin/sdl2trs64.exe
[sdl2trs_1.2.14-1_i386.deb]: bin/sdl2trs_1.2.14-1_i386.deb
[sdl2trs_1.2.14-1_amd64.deb]: bin/sdl2trs_1.2.14-1_amd64.deb
