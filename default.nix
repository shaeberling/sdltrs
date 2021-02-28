{ stdenv
, fetchFromGitLab
, cmake
, readline
, SDL2 }:

with stdenv.lib;
stdenv.mkDerivation rec {
  pname   = "sdl2trs";
  version = "1.2.16";

  src = fetchFromGitLab {
    owner  = "jengun";
    repo   = "sdltrs";
    rev    = "bef282b2cd0a3c0a74c0840ef07b5fa569270be8";
    sha256 = "12q7p8mn2d2d3cjprgwvcv2msh88zghyi8nk3pv1k59a694q7qfa";
  };

  nativeBuildInputs = [ cmake ];
  buildInputs       = [ readline SDL2 ];

  meta = {
    homepage        = "https://gitlab.con/jengun/sdltrs/";
    description     = "TRS-80 Model I/III/4/4P Emulator for SDL2 (hardware rendering)";
    longDescription = ''
      SDL2TRS is an SDL2-based emulator for the Tandy/Radio Shack line of Zilog
      Z80-based microcomputers popular in the late 1970s and early 1980s.  It
      features cassette, floppy, and hard drive emulation, timer interrupt
      emulation, file import and export from the host operating system, support
      for most of the undocumented Z80 instructions, and a built-in debugger.
      Real floppy drives can be used and application-based sound can be
      played and real cassettes read and written directly through the
      sound card or via WAVE files.  Several Hi-Res graphics cards are
      emulated and, in Model 4/4P mode, mice are supported.  There is also
      real-time clock, sound card, serial port, joystick, and CPU clock
      speedup emulation.
      SDL2TRS requires ROM images from the original machines.  The ROMs are
      copyrighted by Radio Shack and are not freely licensed.  (Exception: in
      Model 4P mode, a freely-licensed boot ROM included with this package can be
      used to boot a Model 4 operating system from a diskette image.)
    '';
    maintainers = [ maintainers.jengun ];
    license     = licenses.bsd2;
    platforms   = stdenv.lib.platforms.all;
  };
}
