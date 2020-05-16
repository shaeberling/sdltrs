AC_PREREQ([2.61])
AC_INIT([sdl2trs], [1.2.9a], [dbotw@gmx.net])
AC_CONFIG_SRCDIR([src/trs.h])
AM_INIT_AUTOMAKE([subdir-objects])
AC_PROG_CC
AM_PROG_CC_C_O
AC_C_BIGENDIAN([AC_DEFINE([big_endian])])
AC_CANONICAL_HOST

case $host_os in
  *mingw*)
    AC_DEFINE([WIN32])
    ;;
esac

AC_ARG_ENABLE([fastmove],
  [AS_HELP_STRING([--enable-fastmove], [faster but not accurate Z80 block moves])],
  [AC_DEFINE([FAST_MOVE])
   AC_MSG_NOTICE([faster but not accurate Z80 block moves enabled])])

AC_ARG_ENABLE([readline],
  [AS_HELP_STRING([--enable-readline], [readline support for zbx debugger])],
  [AC_SEARCH_LIBS(tgetent, curses ncurses ncursesw, [], [])
   AC_SEARCH_LIBS([readline], [readline], AC_DEFINE([READLINE]),
   AC_MSG_ERROR([Unable to find readline function]))])

CFLAGS="$CFLAGS $( sdl2-config --cflags )"
LIBS="$LIBS $( sdl2-config --libs)"

AC_CHECK_LIB([SDL2], [SDL_Init], [:],
  AC_MSG_ERROR([Unable to find SDL_Init function in SDL2]))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
