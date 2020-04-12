Modified version of Mark Grebe's [SDLTRS] with bug fixes and patches:

  * Included all patches by [EtchedPixels]: support for banked memory, Lowe LE18 ...
  * Fixed various SegFaults: ROM Selection Menu, Scaling in Fullscreen ...
  * Reworked the Interface: new shortcuts and key bindings, help screen ...
  * Ctrl-A, Ctrl-C & Ctrl-V can now be used in the Emulator (CP/M & WordStar) ...
  * Access to real floppy disks works now on Linux ...
  * Tried to fix reported bugs to the original version ...
  * Port to SDL2 (see [BUILDING.md]) ...
  * Support for Exatron Stringy Floppy ...
  * Select and execute CMD files directly in the Emulator ...
  * Save screenshot of the Emulator window as BMP file ...
  * Show Z80 registers in the window title bar ...

## SDL2

This branch contains only the SDL2 version with texture rendering support.
It should be considered as *experimental* ...

## Binaries

  * [sdl2trs.exe]    (32-bit, needs [SDL2.DLL])
  * [sdl2trs64.exe]  (64-bit, needs [SDL2.DLL])

(Build with MinGW & MinGW-w64)

## Packages

  * [sdl2trs_1.2.9-1_i386.deb]   (32-bit, SDL2)
  * [sdl2trs_1.2.9-1_amd64.deb]  (64-bit, SDL2)

(Build on Debian 9/i386 & Ubuntu 18.04LTS/amd64)

## Screenshots

![screenshot](screenshots/sdltrs01.png)
![screenshot](screenshots/sdltrs02.png)
![screenshot](screenshots/sdltrs03.png)
![screenshot](screenshots/sdltrs04.png)

[BUILDING.md]: BUILDING.md
[EtchedPixels]: https://www.github.com/EtchedPixels/xtrs
[SDL2.DLL]: https://www.libsdl.org/download-2.0.php
[SDLTRS]: http://sdltrs.sourceforge.net
[sdl2trs.exe]: bin/sdl2trs.exe
[sdl2trs64.exe]: bin/sdl2trs64.exe
[sdl2trs_1.2.9-1_i386.deb]: bin/sdl2trs_1.2.9-1_i386.deb
[sdl2trs_1.2.9-1_amd64.deb]: bin/sdl2trs_1.2.9-1_amd64.deb
