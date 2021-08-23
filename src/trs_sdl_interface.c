/*
 * Copyright (C) 2006-2011, Mark Grebe
 * Copyright (C) 2018-2021, Jens Guenther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*#define MOUSEDEBUG 1*/
/*#define XDEBUG 1*/

/*
 * trs_sdl_interface.c
 *
 * SDL interface for TRS-80 Emulator
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL.h>
#include "blit.h"
#include "error.h"
#include "trs.h"
#include "trs_cassette.h"
#include "trs_disk.h"
#include "trs_iodefs.h"
#include "trs_sdl_gui.h"
#include "trs_sdl_keyboard.h"
#include "trs_state_save.h"
#include "trs_stringy.h"
#include "trs_uart.h"
#include "web_debugger.h"

#define MAX_RECTS 2048
#define MAX_SCALE 4
#define WHITE     0xe0e0ff
#define BLACK     0
#define GREEN     0x344843

/* currentmode values */
#ifdef _WIN32
#undef  ALTERNATE
#endif
#define NORMAL    0
#define EXPANDED  1
#define INVERSE   2
#define ALTERNATE 4

/* Public data */
unsigned int foreground;
unsigned int background;
unsigned int gui_foreground;
unsigned int gui_background;
int trs_charset;
int trs_charset1;
int trs_charset3;
int trs_charset4;
int trs_paused;
int trs_emu_mouse;
int trs_show_led;
int scale;
int fullscreen;
int resize3;
int resize4;
int scanlines;
int scanshade;
int window_border_width;
#if defined(SDL2) || !defined(NOX)
int turbo_paste = 0;
#endif
char romfile[FILENAME_MAX];
char romfile3[FILENAME_MAX];
char romfile4p[FILENAME_MAX];
char trs_disk_dir[FILENAME_MAX];
char trs_hard_dir[FILENAME_MAX];
char trs_cass_dir[FILENAME_MAX];
char trs_disk_set_dir[FILENAME_MAX];
char trs_state_dir[FILENAME_MAX];
char trs_printer_dir[FILENAME_MAX];
char trs_cmd_file[FILENAME_MAX];
char trs_config_file[FILENAME_MAX];
char trs_state_file[FILENAME_MAX];
char trs_printer_command[FILENAME_MAX];

/* Private data */
#include "trs_chars.c"

static Uint8 trs_screen[2048];
static int cpu_panel = 0;
static int debugger = 0;
static int screen_chars = 1024;
static int row_chars = 64;
static int col_chars = 16;
static int border_width = 2;
static int resize;
static int text80x24 = 0, screen640x240 = 0;
static int drawnRectCount = 0;
static int top_margin = 0;
static int left_margin = 0;
static int screen_height = 0;
static int currentmode = NORMAL;
static int OrigHeight, OrigWidth;
static int cur_char_width = TRS_CHAR_WIDTH;
static int cur_char_height = TRS_CHAR_HEIGHT * 2;
static int disksizes[8] = { 5, 5, 5, 5, 8, 8, 8, 8 };
#ifdef __linux
static int disksteps[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
#endif
static int mousepointer = 1;
static int mouse_x_size = 640, mouse_y_size = 240;
static int mouse_sens = 3;
static int mouse_last_x = -1, mouse_last_y = -1;
static int mouse_old_style = 0;
static unsigned int mouse_last_buttons;
static SDL_Surface *trs_char[6][MAXCHARS];
static SDL_Surface *trs_box[3][64];
static SDL_Surface *image;
static SDL_Surface *screen;
static SDL_Rect drawnRects[MAX_RECTS];
#ifdef SDL2
static SDL_Window *window = NULL;
#endif
static Uint32 light_red;
static Uint32 bright_red;
static Uint32 light_orange;
static Uint32 bright_orange;
static Uint32 last_key[256];

#if defined(SDL2) || !defined(NOX)
#define PASTE_IDLE    0
#define PASTE_GETNEXT 1
#define PASTE_KEYDOWN 2
#define PASTE_KEYUP   3
static int  paste_state = PASTE_IDLE;
static int  paste_lastkey = FALSE;
extern int  PasteManagerStartPaste(void);
extern void PasteManagerStartCopy(const char *string);
extern int  PasteManagerGetChar(Uint8 *character);

#define COPY_IDLE     0
#define COPY_STARTED  1
#define COPY_DEFINED  2
#define COPY_CLEAR    3
static int copyStatus = COPY_IDLE;
static int selectionStartX = 0;
static int selectionStartY = 0;
static int selectionEndX = 0;
static int selectionEndY = 0;
static int requestSelectAll = FALSE;
static int timer_saved;
static unsigned int cycles_saved;
#endif

/* Support for Micro-Labs Grafyx Solution and Radio Shack hi-res card */

/* True size of graphics memory -- some is offscreen */
#define G_XSIZE 128
#define G_YSIZE 256
static char grafyx[(2 * G_YSIZE * MAX_SCALE) * (G_XSIZE * MAX_SCALE)];
static Uint8 grafyx_unscaled[G_YSIZE][G_XSIZE];

static Uint8 grafyx_microlabs = 0;
static Uint8 grafyx_x = 0, grafyx_y = 0, grafyx_mode = 0;
static Uint8 grafyx_enable = 0;
static Uint8 grafyx_overlay = 0;
static Uint8 grafyx_xoffset = 0, grafyx_yoffset = 0;

/* Port 0x83 (grafyx_mode) bits */
#define G_ENABLE    1
#define G_UL_NOTEXT 2   /* Micro-Labs only */
#define G_RS_WAIT   2   /* Radio Shack only */
#define G_XDEC      4
#define G_YDEC      8
#define G_XNOCLKR   16
#define G_YNOCLKR   32
#define G_XNOCLKW   64
#define G_YNOCLKW   128

/* Port 0xFF (grafyx_m3_mode) bits */
#define G3_COORD    0x80
#define G3_ENABLE   0x40
#define G3_COMMAND  0x20
#define G3_YLOW(v)  (((v) & 0x1e) >> 1)

#define HRG_MEMSIZE (1024 * 12)        /* 12k * 8 bit graphics memory */
static Uint8 hrg_screen[HRG_MEMSIZE];
static int hrg_pixel_x[2][6 + 1];
static int hrg_pixel_y[12 + 1];
static int hrg_pixel_width[2][6];
static int hrg_pixel_height[12];
static int hrg_enable = 0;
static int hrg_addr = 0;
static void hrg_update_char(int position);

/* Option handling */
typedef struct trs_opt_struct {
  const char *name;
  void (*handler)(char *, int, int *);
  int hasArg;
  int intArg;
  void *strArg;
} trs_opt;

static void trs_opt_borderwidth(char *arg, int intarg, int *stringarg);
static void trs_opt_cass(char *arg, int intarg, int *stringarg);
static void trs_opt_charset(char *arg, int intarg, int *stringarg);
static void trs_opt_clock(char *arg, int intarg, int *stringarg);
static void trs_opt_color(char *arg, int intarg, int *color);
static void trs_opt_disk(char *arg, int intarg, int *stringarg);
static void trs_opt_diskset(char *arg, int intarg, int *stringarg);
static void trs_opt_dirname(char *arg, int intarg, int *stringarg);
static void trs_opt_doubler(char *arg, int intarg, int *stringarg);
#ifdef __linux
static void trs_opt_doublestep(char *arg, int intarg, int *stringarg);
#endif
static void trs_opt_hard(char *arg, int intarg, int *stringarg);
static void trs_opt_huffman(char *arg, int intarg, int *stringarg);
static void trs_opt_hypermem(char *arg, int intarg, int *stringarg);
static void trs_opt_joybuttonmap(char *arg, int intarg, int *stringarg);
static void trs_opt_joysticknum(char *arg, int intarg, int *stringarg);
static void trs_opt_keystretch(char *arg, int intarg, int *stringarg);
static void trs_opt_microlabs(char *arg, int intarg, int *stringarg);
static void trs_opt_model(char *arg, int intarg, int *stringarg);
static void trs_opt_printer(char *arg, int intarg, int *stringarg);
static void trs_opt_rom(char *arg, int intarg, int *stringarg);
static void trs_opt_samplerate(char *arg, int intarg, int *stringarg);
static void trs_opt_scale(char *arg, int intarg, int *stringarg);
static void trs_opt_scanshade(char *arg, int intarg, int *stringarg);
static void trs_opt_selector(char *arg, int intarg, int *stringarg);
static void trs_opt_shiftbracket(char *arg, int intarg, int *stringarg);
static void trs_opt_sizemap(char *arg, int intarg, int *stringarg);
static void trs_opt_speedup(char *arg, int intarg, int *stringarg);
#ifdef __linux
static void trs_opt_stepmap(char *arg, int intarg, int *stringarg);
#endif
static void trs_opt_string(char *arg, int intarg, int *stringarg);
static void trs_opt_supermem(char *arg, int intarg, int *stringarg);
static void trs_opt_switches(char *arg, int intarg, int *stringarg);
static void trs_opt_turborate(char *arg, int intarg, int *stringarg);
static void trs_opt_value(char *arg, int intarg, int *variable);
static void trs_opt_wafer(char *arg, int intarg, int *stringarg);

static const trs_opt options[] = {
  { "background",      trs_opt_color,         1, 0, &background          },
  { "bg",              trs_opt_color,         1, 0, &background          },
  { "borderwidth",     trs_opt_borderwidth,   1, 0, NULL                 },
  { "bw",              trs_opt_borderwidth,   1, 0, NULL                 },
  { "cass",            trs_opt_cass,          1, 0, NULL                 },
  { "cassdir",         trs_opt_dirname,       1, 0, trs_cass_dir         },
  { "cassette",        trs_opt_cass,          1, 0, NULL                 },
  { "charset1",        trs_opt_charset,       1, 1, NULL                 },
  { "charset3",        trs_opt_charset,       1, 3, NULL                 },
  { "charset4",        trs_opt_charset,       1, 4, NULL                 },
  { "clock1",          trs_opt_clock,         1, 1, NULL                 },
  { "clock3",          trs_opt_clock,         1, 3, NULL                 },
  { "clock4",          trs_opt_clock,         1, 4, NULL                 },
#ifdef ZBX
  { "debug",           trs_opt_value,         0, 1, &debugger            },
#endif
  { "disk0",           trs_opt_disk,          1, 0, NULL                 },
  { "disk1",           trs_opt_disk,          1, 1, NULL                 },
  { "disk2",           trs_opt_disk,          1, 2, NULL                 },
  { "disk3",           trs_opt_disk,          1, 3, NULL                 },
  { "disk4",           trs_opt_disk,          1, 4, NULL                 },
  { "disk5",           trs_opt_disk,          1, 5, NULL                 },
  { "disk6",           trs_opt_disk,          1, 6, NULL                 },
  { "disk7",           trs_opt_disk,          1, 7, NULL                 },
  { "diskdir",         trs_opt_dirname,       1, 0, trs_disk_dir         },
  { "diskset",         trs_opt_diskset,       1, 0, NULL                 },
  { "disksetdir",      trs_opt_dirname,       1, 0, trs_disk_set_dir     },
  { "doubler",         trs_opt_doubler,       1, 0, NULL                 },
#ifdef __linux
  { "doublestep",      trs_opt_doublestep,    0, 2, NULL                 },
#endif
  { "emtsafe",         trs_opt_value,         0, 1, &trs_emtsafe         },
  { "fg",              trs_opt_color,         1, 0, &foreground          },
  { "foreground",      trs_opt_color,         1, 0, &foreground          },
  { "fullscreen",      trs_opt_value,         0, 1, &fullscreen          },
  { "fs",              trs_opt_value,         0, 1, &fullscreen          },
  { "guibackground",   trs_opt_color,         1, 0, &gui_background      },
  { "guibg",           trs_opt_color,         1, 0, &gui_background      },
  { "guifg",           trs_opt_color,         1, 0, &gui_foreground      },
  { "guiforeground",   trs_opt_color,         1, 0, &gui_foreground      },
  { "hard0",           trs_opt_hard,          1, 0, NULL                 },
  { "hard1",           trs_opt_hard,          1, 1, NULL                 },
  { "hard2",           trs_opt_hard,          1, 2, NULL                 },
  { "hard3",           trs_opt_hard,          1, 3, NULL                 },
  { "harddir",         trs_opt_dirname,       1, 0, trs_hard_dir         },
  { "hideled",         trs_opt_value,         0, 0, &trs_show_led        },
  { "huffman",         trs_opt_huffman,       0, 1, NULL                 },
  { "hypermem",        trs_opt_hypermem,      0, 1, NULL                 },
  { "joyaxismapped",   trs_opt_value,         0, 1, &jaxis_mapped        },
  { "joybuttonmap",    trs_opt_joybuttonmap,  1, 0, NULL                 },
  { "joysticknum",     trs_opt_joysticknum,   1, 0, NULL                 },
  { "keypadjoy",       trs_opt_value,         0, 1, &trs_keypad_joystick },
  { "keystretch",      trs_opt_keystretch,    1, 0, NULL                 },
  { "le18",            trs_opt_value,         0, 1, &lowe_le18           },
  { "lower",           trs_opt_value,         0, 1, &lowercase           },
  { "lowercase",       trs_opt_value,         0, 1, &lowercase           },
  { "microlabs",       trs_opt_microlabs,     0, 1, NULL                 },
  { "m1",              trs_opt_value,         0, 1, &trs_model           },
  { "m3",              trs_opt_value,         0, 3, &trs_model           },
  { "m4",              trs_opt_value,         0, 4, &trs_model           },
  { "m4p",             trs_opt_value,         0, 5, &trs_model           },
  { "model",           trs_opt_model,         1, 0, NULL                 },
  { "mousepointer",    trs_opt_value,         0, 1, &mousepointer        },
#ifdef ZBX
  { "nodebug",         trs_opt_value,         0, 0, &debugger            },
#endif
#ifdef __linux
  { "nodoublestep",    trs_opt_doublestep,    0, 1, NULL                 },
#endif
  { "noemtsafe",       trs_opt_value,         0, 0, &trs_emtsafe         },
  { "nofullscreen",    trs_opt_value,         0, 0, &fullscreen          },
  { "nofs",            trs_opt_value,         0, 0, &fullscreen          },
  { "nohuffman",       trs_opt_huffman,       0, 0, NULL                 },
  { "nohypermem",      trs_opt_hypermem,      0, 0, NULL                 },
  { "nojoyaxismapped", trs_opt_value,         0, 0, &jaxis_mapped        },
  { "nokeypadjoy",     trs_opt_value,         0, 0, &trs_keypad_joystick },
  { "nole18",          trs_opt_value,         0, 0, &lowe_le18           },
  { "nolower",         trs_opt_value,         0, 0, &lowercase           },
  { "nolowercase",     trs_opt_value,         0, 0, &lowercase           },
  { "nomicrolabs",     trs_opt_microlabs,     0, 0, NULL                 },
  { "nomousepointer",  trs_opt_value,         0, 0, &mousepointer        },
  { "noresize3",       trs_opt_value,         0, 0, &resize3             },
  { "noresize4",       trs_opt_value,         0, 0, &resize4             },
  { "noscanlines",     trs_opt_value,         0, 0, &scanlines           },
  { "noselector",      trs_opt_selector,      0, 0, NULL                 },
  { "noshiftbracket",  trs_opt_shiftbracket,  0, 0, NULL                 },
  { "nosound",         trs_opt_value,         0, 0, &trs_sound           },
  { "nostringy",       trs_opt_value,         0, 0, &stringy             },
  { "nosupermem",      trs_opt_supermem,      0, 0, NULL                 },
  { "notruedam",       trs_opt_value,         0, 0, &trs_disk_truedam    },
  { "noturbo",         trs_opt_value,         0, 0, &timer_overclock     },
#if defined(SDL2) || !defined(NOX)
  { "noturbopaste",    trs_opt_value,         0, 0, &turbo_paste         },
#endif
  { "printer",         trs_opt_printer,       1, 0, NULL                 },
  { "printercmd",      trs_opt_string,        1, 0, trs_printer_command  },
  { "printerdir",      trs_opt_dirname,       1, 0, trs_printer_dir      },
  { "resize3",         trs_opt_value,         0, 1, &resize3             },
  { "resize4",         trs_opt_value,         0, 1, &resize4             },
  { "rom",             trs_opt_rom,           1, 0, NULL                 },
  { "romfile",         trs_opt_string,        1, 0, romfile              },
  { "romfile1",        trs_opt_string,        1, 0, romfile              },
  { "romfile3",        trs_opt_string,        1, 0, romfile3             },
  { "romfile4p",       trs_opt_string,        1, 0, romfile4p            },
  { "samplerate",      trs_opt_samplerate,    1, 0, NULL                 },
  { "scale",           trs_opt_scale,         1, 0, NULL                 },
  { "scanlines",       trs_opt_value,         0, 1, &scanlines           },
  { "scanshade",       trs_opt_scanshade,     1, 0, NULL                 },
  { "selector",        trs_opt_selector,      0, 1, NULL                 },
  { "serial",          trs_opt_string,        1, 0, trs_uart_name        },
  { "shiftbracket",    trs_opt_shiftbracket,  0, 1, NULL                 },
  { "showled",         trs_opt_value,         0, 1, &trs_show_led        },
  { "sizemap",         trs_opt_sizemap,       1, 0, NULL                 },
  { "sound",           trs_opt_value,         0, 1, &trs_sound           },
  { "speedup",         trs_opt_speedup,       1, 0, NULL                 },
  { "statedir",        trs_opt_dirname,       1, 0, trs_state_dir        },
#ifdef __linux
  { "stepmap",         trs_opt_stepmap,       1, 0, NULL                 },
#endif
  { "stringy",         trs_opt_value,         0, 1, &stringy             },
  { "supermem",        trs_opt_supermem,      0, 1, NULL                 },
  { "switches",        trs_opt_switches,      1, 0, NULL                 },
  { "truedam",         trs_opt_value,         0, 1, &trs_disk_truedam    },
  { "turbo",           trs_opt_value,         0, 1, &timer_overclock     },
#if defined(SDL2) || !defined(NOX)
  { "turbopaste",      trs_opt_value,         0, 1, &turbo_paste         },
#endif
  { "turborate",       trs_opt_turborate,     1, 0, NULL                 },
  { "wafer0",          trs_opt_wafer,         1, 0, NULL                 },
  { "wafer1",          trs_opt_wafer,         1, 1, NULL                 },
  { "wafer2",          trs_opt_wafer,         1, 2, NULL                 },
  { "wafer3",          trs_opt_wafer,         1, 3, NULL                 },
  { "wafer4",          trs_opt_wafer,         1, 4, NULL                 },
  { "wafer5",          trs_opt_wafer,         1, 5, NULL                 },
  { "wafer6",          trs_opt_wafer,         1, 6, NULL                 },
  { "wafer7",          trs_opt_wafer,         1, 7, NULL                 },
};

static const int num_options = sizeof(options) / sizeof(trs_opt);

/* Private routines */
static void bitmap_init(void);
static void grafyx_rescale(int y, int x, char byte);

static void stripWhitespace(char *inputStr)
{
  char *pos = inputStr;

  while (*pos && isspace(*pos))
    pos++;
  memmove(inputStr, pos, strlen(pos) + 1);
  pos = inputStr + strlen(inputStr) - 1;
  while (*pos && isspace(*pos))
    pos--;
  *(pos + 1) = '\0';
}

static const char *charset_name(int charset)
{
  switch (charset) {
    case 0:
      return "early";
    case 1:
      return "stock";
    case 2:
      return "lcmod";
    case 3:
    default:
      return "wider";
    case 4:
    case 7:
      return "katakana";
    case 5:
    case 8:
      return "international";
    case 6:
    case 9:
      return "bold";
    case 10:
      return "genie";
    case 11:
      return "ht-1080z";
    case 12:
      return "videogenie";
  }
}

static void trs_opt_borderwidth(char *arg, int intarg, int *stringarg)
{
  window_border_width = atol(arg);
  if (window_border_width < 0 || window_border_width > 50)
    window_border_width = 2;
}

static void trs_opt_cass(char *arg, int intarg, int *stringarg)
{
  trs_cassette_insert(arg);
}

static void trs_opt_charset(char *arg, int intarg, int *stringarg)
{
  if (intarg == 1) {
    if (isdigit((int)*arg)) {
      trs_charset1 = atoi(arg);
      if (trs_charset1 < 0 || (trs_charset1 > 3 && (trs_charset1 < 10 ||
          trs_charset1 > 12)))
        trs_charset1 = 3;
    } else
      switch (tolower((int)*arg)) {
        case 'e': /*early*/
          trs_charset1 = 0;
          break;
        case 's': /*stock*/
          trs_charset1 = 1;
          break;
        case 'l': /*lcmod*/
          trs_charset1 = 2;
          break;
        case 'w': /*wider*/
          trs_charset1 = 3;
          break;
        case 'g': /*genie or german*/
          trs_charset1 = 10;
          break;
        case 'h': /*ht-1080z*/
          trs_charset1 = 11;
          break;
        case 'v': /*video genie*/
          trs_charset1 = 12;
          break;
        default:
          error("unknown charset1 name: %s", arg);
    }
  } else {
    if (isdigit((int)*arg)) {
      if (intarg == 3) {
        trs_charset3 = atoi(arg);
        if (trs_charset3 < 4 || trs_charset3 > 6)
          trs_charset3 = 4;
      } else {
        trs_charset4 = atoi(arg);
        if (trs_charset4 < 7 || trs_charset4 > 9)
          trs_charset4 = 8;
      }
    } else {
      int charset;

      switch (tolower((int)*arg)) {
        case 'k': /*katakana*/
          charset = 4;
          break;
        case 'i': /*international*/
          charset = 5;
          break;
        case 'b': /*bold*/
          charset = 6;
          break;
        default:
          error("unknown charset%d name: %s", intarg, arg);
          return;
      }
      if (intarg == 3)
        trs_charset3 = charset;
      else
        trs_charset4 = charset + 3;
    }
  }
}

static void trs_opt_clock(char *arg, int intarg, int *stringarg)
{
  float clock_mhz = atof(arg);

  if (clock_mhz >= 0.1 && clock_mhz <= 99.0) {
    switch (intarg) {
      case 1:
        clock_mhz_1 = clock_mhz;
        break;
      case 3:
        clock_mhz_3 = clock_mhz;
        break;
      case 4:
        clock_mhz_4 = clock_mhz;
        break;
    }
  }
}

static void trs_opt_color(char *arg, int intarg, int *color)
{
  *color = strtol(arg, NULL, 16);
}

static void trs_opt_disk(char *arg, int intarg, int *stringarg)
{
  trs_disk_insert(intarg, arg);
}

static void trs_opt_diskset(char *arg, int intarg, int *stringarg)
{
  trs_diskset_load(arg);
}

static void trs_opt_dirname(char *arg, int intarg, int *stringarg)
{
  struct stat st;

  if (arg[strlen(arg) - 1] == DIR_SLASH)
    snprintf((char *)stringarg, FILENAME_MAX, "%s", arg);
  else
    snprintf((char *)stringarg, FILENAME_MAX, "%s%c", arg, DIR_SLASH);
  if (stat((char *)stringarg, &st) < 0)
    strcpy((char *)stringarg, ".");
}

static void trs_opt_doubler(char *arg, int intarg, int *stringarg)
{
  switch (tolower((int)*arg)) {
    case 'p':
      trs_disk_doubler = TRSDISK_PERCOM;
      break;
    case 'r':
    case 't':
      trs_disk_doubler = TRSDISK_TANDY;
      break;
    case 'b':
    default:
      trs_disk_doubler = TRSDISK_BOTH;
      break;
    case 'n':
      trs_disk_doubler = TRSDISK_NODOUBLER;
      break;
    }
}

#ifdef __linux
static void trs_opt_doublestep(char *arg, int intarg, int *stringarg)
{
  int i;

  for (i = 0; i < 8; i++)
    disksteps[i] = intarg;
}

static void trs_opt_stepmap(char *arg, int intarg, int *stringarg)
{
  sscanf(arg, "%d,%d,%d,%d,%d,%d,%d,%d",
         &disksteps[0], &disksteps[1], &disksteps[2], &disksteps[3],
         &disksteps[4], &disksteps[5], &disksteps[6], &disksteps[7]);
}
#endif

static void trs_opt_hard(char *arg, int intarg, int *stringarg)
{
  trs_hard_attach(intarg, arg);
}

static void trs_opt_huffman(char *arg, int intarg, int *stringarg)
{
  huffman_ram = intarg;
  if (huffman_ram)
    hypermem = 0;
}

static void trs_opt_hypermem(char *arg, int intarg, int *stringarg)
{
  hypermem = intarg;
  if (hypermem)
    huffman_ram = 0;
}

static void trs_opt_joybuttonmap(char *arg, int intarg, int *stringarg)
{
  int i;

  for (i = 0; i < N_JOYBUTTONS; i++) {
    char *ptr = strchr(arg, ',');

    if (ptr != NULL)
      *ptr = '\0';
    if (sscanf(arg, "%d", &jbutton_map[i]) == 0)
      jbutton_map[i] = -1;
    if (ptr != NULL)
      arg = ptr + 1;
  }
}

static void trs_opt_joysticknum(char *arg, int intarg, int *stringarg)
{
  if (strcasecmp(arg, "none") == 0)
    trs_joystick_num = -1;
  else
    trs_joystick_num = atoi(arg);
}

static void trs_opt_keystretch(char *arg, int intarg, int *stringarg)
{
  stretch_amount = atol(arg);
  if (stretch_amount < 0)
    stretch_amount = STRETCH_AMOUNT;
}

static void trs_opt_microlabs(char *arg, int intarg, int *stringarg)
{
  grafyx_set_microlabs(intarg);
}

static void trs_opt_model(char *arg, int intarg, int *stringarg)
{
  if (strcmp(arg, "1") == 0 ||
      strcasecmp(arg, "I") == 0) {
    trs_model = 1;
  } else if (strcmp(arg, "3") == 0 ||
             strcasecmp(arg, "III") == 0) {
    trs_model = 3;
  } else if (strcmp(arg, "4") == 0 ||
             strcasecmp(arg, "IV") == 0) {
    trs_model = 4;
  } else if (strcasecmp(arg, "4P") == 0 ||
             strcasecmp(arg, "IVp") == 0) {
    trs_model = 5;
  } else
    error("TRS-80 Model %s not supported", arg);
}

static void trs_opt_rom(char *arg, int intarg, int *stringarg)
{
  switch (trs_model) {
    case 1:
      snprintf(romfile, FILENAME_MAX, "%s", arg);
      break;
    case 3:
    case 4:
      snprintf(romfile3, FILENAME_MAX, "%s", arg);
      break;
    case 5:
      snprintf(romfile4p, FILENAME_MAX, "%s", arg);
      break;
   }
}

static void trs_opt_printer(char *arg, int intarg, int *stringarg)
{
  if (isdigit((int)*arg)) {
    trs_printer = atoi(arg);
    if (trs_printer < 0 || trs_printer > 1)
      trs_printer = 0;
  } else
    switch (tolower((int)*arg)) {
      case 'n': /*none*/
        trs_printer = 0;
        break;
      case 't': /*text*/
        trs_printer = 1;
        break;
      default:
        error("unknown printer type: %s", arg);
    }
}

static void trs_opt_samplerate(char *arg, int intarg, int *stringarg)
{
  cassette_default_sample_rate = atol(arg);
  if (cassette_default_sample_rate < 0 ||
      cassette_default_sample_rate > DEFAULT_SAMPLE_RATE)
    cassette_default_sample_rate = DEFAULT_SAMPLE_RATE;
}

static void trs_opt_scale(char *arg, int intarg, int *stringarg)
{
  scale = atoi(arg);
  if (scale <= 0)
    scale = 1;
  else if (scale > MAX_SCALE)
    scale = MAX_SCALE;
}

static void trs_opt_scanshade(char *arg, int intarg, int *stringarg)
{
  scanshade = atoi(arg) & 255;
}

static void trs_opt_selector(char *arg, int intarg, int *stringarg)
{
  selector = intarg;
  if (selector)
    supermem = 0;
}

static void trs_opt_shiftbracket(char *arg, int intarg, int *stringarg)
{
  trs_kb_bracket(intarg);
}

static void trs_opt_sizemap(char *arg, int intarg, int *stringarg)
{
  sscanf(arg, "%d,%d,%d,%d,%d,%d,%d,%d",
         &disksizes[0], &disksizes[1], &disksizes[2], &disksizes[3],
         &disksizes[4], &disksizes[5], &disksizes[6], &disksizes[7]);
}

static void trs_opt_speedup(char *arg, int intarg, int *stringarg)
{
  switch (tolower((int)*arg)) {
    case 'n': /*None*/
      speedup = 0;
      break;
    case 'a': /*Archbold*/
      speedup = 1;
      break;
    case 'h': /*Holmes*/
      speedup = 2;
      break;
    case 's': /*Seatronics*/
      speedup = 3;
      break;
    default:
      error("unknown speedup kit: %s", arg);
  }
}

static void trs_opt_string(char *arg, int intarg, int *stringarg)
{
  snprintf((char *)stringarg, FILENAME_MAX, "%s", arg);
}

static void trs_opt_supermem(char *arg, int intarg, int *stringarg)
{
  supermem = intarg;
  if (supermem)
    selector = 0;
}

static void trs_opt_switches(char *arg, int intarg, int *stringarg)
{
  int base = 10;

  if (!strncasecmp(arg, "0x", 2))
    base = 16;

  trs_uart_switches = strtol(arg, NULL, base);
}

static void trs_opt_turborate(char *arg, int intarg, int *stringarg)
{
  timer_overclock_rate = atoi(arg);
  if (timer_overclock_rate <= 0)
    timer_overclock_rate = 1;
}

static void trs_opt_value(char *arg, int intarg, int *variable)
{
  *variable = intarg;
}

static void trs_opt_wafer(char *arg, int intarg, int *stringarg)
{
  stringy_insert(intarg, arg);
}

static void trs_disk_setsizes(void)
{
  int i;

  for (i = 0; i < 8; i++) {
    if (disksizes[i] == 5 || disksizes[i] == 8)
      trs_disk_setsize(i, disksizes[i]);
    else
      error("bad value %d for disk %d size", disksizes[i], i);
  }
}

#ifdef __linux
static void trs_disk_setsteps(void)
{
  int i;

  /* Disk Steps are 1 for Single Step or 2 for Double Step for all Eight Default Drives */
  for (i = 0; i < 8; i++) {
    if (disksteps[i] == 1 || disksteps[i] == 2)
      trs_disk_setstep(i, disksteps[i]);
    else
      error("bad value %d for disk %d single/double step", disksteps[i], i);
  }
}
#endif

int trs_load_config_file(void)
{
  char line[FILENAME_MAX];
  char *arg;
  FILE *config_file;
  int i;

  for (i = 0; i < 8; i++)
    trs_disk_remove(i);

  for (i = 0; i < 4; i++)
    trs_hard_remove(i);

  for (i = 0; i < 8; i++)
    stringy_remove(i);

  trs_cassette_remove();

  background = BLACK;
  cassette_default_sample_rate = DEFAULT_SAMPLE_RATE;
  /* Disk Sizes are 5" or 8" for all Eight Default Drives */
  /* Corrected by Larry Kraemer 08-01-2011 */
  disksizes[0] = 5;
  disksizes[1] = 5;
  disksizes[2] = 5;
  disksizes[3] = 5;
  disksizes[4] = 8;
  disksizes[5] = 8;
  disksizes[6] = 8;
  disksizes[7] = 8;
  trs_disk_setsizes();
#ifdef __linux
  /* Disk Steps are 1 for Single Step, 2 for Double Step for all Eight Default Drives */
  /* Corrected by Larry Kraemer 08-01-2011 */
  disksteps[0] = 1;
  disksteps[1] = 1;
  disksteps[2] = 1;
  disksteps[3] = 1;
  disksteps[4] = 1;
  disksteps[5] = 1;
  disksteps[6] = 1;
  disksteps[7] = 1;
  trs_disk_setsteps();
#endif
  foreground = WHITE;
  fullscreen = 0;
  grafyx_set_microlabs(FALSE);
  gui_background = GREEN;
  gui_foreground = WHITE;
  lowercase = 1;
  resize3 = 1;
  resize4 = 0;
  scale = 1;
  scanlines = 0;
  scanshade = 127;
  strcpy(romfile, "level2.rom");
  strcpy(romfile3, "model3.rom");
  strcpy(romfile4p, "model4p.rom");
  strcpy(trs_cass_dir, ".");
  strcpy(trs_disk_dir, ".");
  strcpy(trs_disk_set_dir, ".");
  strcpy(trs_hard_dir, ".");
#ifdef _WIN32
  strcpy(trs_printer_command, "notepad %s");
#else
  strcpy(trs_printer_command, "lpr %s");
#endif
  strcpy(trs_printer_dir, ".");
  strcpy(trs_state_dir, ".");
  stretch_amount = STRETCH_AMOUNT;
  trs_charset = 3;
  trs_charset1 = 3;
  trs_charset3 = 4;
  trs_charset4 = 8;
  trs_disk_doubler = TRSDISK_BOTH;
  trs_disk_truedam = 0;
  trs_emtsafe = 1;
  trs_joystick_num = 0;
  trs_kb_bracket(FALSE);
  trs_keypad_joystick = TRUE;
  trs_model = 1;
  trs_show_led = TRUE;
  trs_uart_switches = 0x7 | TRS_UART_NOPAR | TRS_UART_WORD8;
  window_border_width = 2;

  if (trs_config_file[0] == 0) {
    const char *home = getenv("HOME");

    if (home)
      snprintf(trs_config_file, FILENAME_MAX, "%s/.sdltrs.t8c", home);
    else
      snprintf(trs_config_file, FILENAME_MAX, "./sdltrs.t8c");

    if ((config_file = fopen(trs_config_file, "r")) == NULL) {
      debug("create default configuration file: %s\n", trs_config_file);
      trs_write_config_file(trs_config_file);
      return -1;
    }
  } else {
    if ((config_file = fopen(trs_config_file, "r")) == NULL) {
      error("failed to load %s: %s", trs_config_file, strerror(errno));
      return -1;
    }
  }

  while (fgets(line, sizeof(line), config_file)) {
    arg = strchr(line, '=');
    if (arg != NULL) {
      *arg++ = '\0';
      stripWhitespace(arg);
    }

    stripWhitespace(line);

    for (i = 0; i < num_options; i++) {
      if (strcasecmp(line, options[i].name) == 0) {
        if (options[i].hasArg) {
          if (arg)
            (*options[i].handler)(arg, options[i].intArg, options[i].strArg);
        } else
          (*options[i].handler)(NULL, options[i].intArg, options[i].strArg);
        break;
      }
    }
    if (i == num_options)
      error("unrecognized option: %s", line);
  }

  fclose(config_file);
  return 0;
}

void trs_parse_command_line(int argc, char **argv, int *debug)
{
  int i, j, len;

  /* Check for config or state files on the command line */
  trs_config_file[0] = 0;
  trs_state_file[0] = 0;
  trs_cmd_file[0] = 0;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      for (j = 0; j < num_options; j++) {
        if (strcasecmp(&argv[i][1], options[j].name) == 0) {
          if (options[j].hasArg)
            i++;
          break;
        }
      }
    }
    else if ((len = strlen(argv[i]) - 4) > 0) {
      if (strcasecmp(&argv[i][len], ".t8c") == 0)
        snprintf(trs_config_file, FILENAME_MAX, "%s", argv[i]);
      else if (strcasecmp(&argv[i][len], ".t8s") == 0)
        snprintf(trs_state_file, FILENAME_MAX, "%s", argv[i]);
      else if (strcasecmp(&argv[i][len], ".cmd") == 0)
        snprintf(trs_cmd_file, FILENAME_MAX, "%s", argv[i]);
    }
  }

  trs_load_config_file();

  for (i = 1; i < argc; i++) {
    int argAvail = ((i + 1) < argc); /* is argument available? */

    for (j = 0; j < num_options; j++) {
      if (argv[i][0] == '-') {
        if (strcasecmp(&argv[i][1], options[j].name) == 0) {
          if (options[j].hasArg) {
            if (argAvail)
              (*options[j].handler)(argv[++i], options[j].intArg, options[j].strArg);
          } else
            (*options[j].handler)(NULL, options[j].intArg, options[j].strArg);
          break;
        }
      }
    }
    if (j == num_options && argv[i][0] == '-')
      error("unrecognized option %s", argv[i]);
  }

  *debug = debugger;
  trs_disk_setsizes();
#ifdef __linux
  trs_disk_setsteps();
#endif
}

int trs_write_config_file(const char *filename)
{
  FILE *config_file;
  int i;

  if ((config_file = fopen(filename, "w")) == NULL) {
    error("failed to write %s: %s", filename, strerror(errno));
    return -1;
  }

  fprintf(config_file, "background=0x%x\n", background);
  fprintf(config_file, "borderwidth=%d\n", window_border_width);
  fprintf(config_file, "cassdir=%s\n", trs_cass_dir);
  {
    const char *cassname = trs_cassette_getfilename();

    if (cassname[0])
      fprintf(config_file, "cassette=%s\n", cassname);
  }
  fprintf(config_file, "charset1=%s\n", charset_name(trs_charset1));
  fprintf(config_file, "charset3=%s\n", charset_name(trs_charset3));
  fprintf(config_file, "charset4=%s\n", charset_name(trs_charset4));
  fprintf(config_file, "clock1=%.2f\n", clock_mhz_1);
  fprintf(config_file, "clock3=%.2f\n", clock_mhz_3);
  fprintf(config_file, "clock4=%.2f\n", clock_mhz_4);
  for (i = 0; i < 8; i++) {
    const char *diskname = trs_disk_getfilename(i);

    if (diskname[0])
      fprintf(config_file, "disk%d=%s\n", i, diskname);
  }
  fprintf(config_file, "diskdir=%s\n", trs_disk_dir);
  fprintf(config_file, "disksetdir=%s\n", trs_disk_set_dir);
  fprintf(config_file, "doubler=");
  switch (trs_disk_doubler) {
    case TRSDISK_PERCOM:
      fprintf(config_file, "percom\n");
      break;
    case TRSDISK_TANDY:
      fprintf(config_file, "tandy\n");
      break;
    case TRSDISK_BOTH:
      fprintf(config_file, "both\n");
      break;
    case TRSDISK_NODOUBLER:
      fprintf(config_file, "none\n");
      break;
  }
  fprintf(config_file, "%semtsafe\n", trs_emtsafe ? "" : "no");
  fprintf(config_file, "%sfullscreen\n", fullscreen ? "" : "no");
  fprintf(config_file, "foreground=0x%x\n", foreground);
  fprintf(config_file, "guibackground=0x%x\n", gui_background);
  fprintf(config_file, "guiforeground=0x%x\n", gui_foreground);
  for (i = 0; i < 4; i++) {
    const char *diskname = trs_hard_getfilename(i);

    if (diskname[0])
      fprintf(config_file, "hard%d=%s\n", i, diskname);
  }
  fprintf(config_file, "harddir=%s\n", trs_hard_dir);
  fprintf(config_file, "%shuffman\n", huffman_ram ? "" : "no");
  fprintf(config_file, "%shypermem\n", hypermem ? "" : "no");
  fprintf(config_file, "%sjoyaxismapped\n", jaxis_mapped ? "" : "no");
  fprintf(config_file, "joybuttonmap=");
  for (i = 0; i < N_JOYBUTTONS; i++)
    fprintf(config_file, i < N_JOYBUTTONS - 1 ? "%d," : "%d\n", jbutton_map[i]);
  fprintf(config_file, "joysticknum=");
  if (trs_joystick_num == -1)
    fprintf(config_file, "none\n");
  else
    fprintf(config_file, "%d\n", trs_joystick_num);
  fprintf(config_file, "%skeypadjoy\n", trs_keypad_joystick ? "" : "no");
  fprintf(config_file, "keystretch=%d\n", stretch_amount);
  fprintf(config_file, "%sle18\n", lowe_le18 ? "" : "no");
  fprintf(config_file, "%slowercase\n", lowercase ? "" : "no");
  fprintf(config_file, "%smicrolabs\n", grafyx_microlabs ? "" : "no");
  fprintf(config_file, "model=%d%s\n",
          trs_model == 5 ? 4 : trs_model, trs_model == 5 ? "P" : "");
  fprintf(config_file, "%smousepointer\n", mousepointer ? "" : "no");
  fprintf(config_file, "printer=%d\n", trs_printer);
  fprintf(config_file, "printercmd=%s\n", trs_printer_command);
  fprintf(config_file, "printerdir=%s\n", trs_printer_dir);
  fprintf(config_file, "%sresize3\n", resize3 ? "" : "no");
  fprintf(config_file, "%sresize4\n", resize4 ? "" : "no");
  fprintf(config_file, "romfile=%s\n", romfile);
  fprintf(config_file, "romfile3=%s\n", romfile3);
  fprintf(config_file, "romfile4p=%s\n", romfile4p);
  fprintf(config_file, "samplerate=%d\n", cassette_default_sample_rate);
  fprintf(config_file, "scale=%d\n", scale);
  fprintf(config_file, "%sscanlines\n", scanlines ? "" : "no");
  fprintf(config_file, "scanshade=%d\n", scanshade);
  fprintf(config_file, "%sselector\n", selector ? "" : "no");
  fprintf(config_file, "serial=%s\n", trs_uart_name);
  fprintf(config_file, "%sshiftbracket\n", trs_kb_bracket_state ? "" : "no");
  fprintf(config_file, "%s\n", trs_show_led ? "showled" : "hideled");
  fprintf(config_file, "sizemap=%d,%d,%d,%d,%d,%d,%d,%d\n",
      trs_disk_getsize(0), trs_disk_getsize(1), trs_disk_getsize(2), trs_disk_getsize(3),
      trs_disk_getsize(4), trs_disk_getsize(5), trs_disk_getsize(6), trs_disk_getsize(7));
  fprintf(config_file, "%ssound\n", trs_sound ? "" : "no");
  fprintf(config_file, "speedup=");
  switch (speedup) {
    case 0:
    default:
      fprintf(config_file, "none\n");
      break;
    case 1:
      fprintf(config_file, "archbold\n");
      break;
    case 2:
      fprintf(config_file, "holmes\n");
      break;
    case 3:
      fprintf(config_file, "seatronics\n");
      break;
  }
  fprintf(config_file, "statedir=%s\n", trs_state_dir);
#ifdef __linux
  /* Corrected to trs_disk_getstep vs getsize by Larry Kraemer 08-01-2011 */
  fprintf(config_file, "stepmap=%d,%d,%d,%d,%d,%d,%d,%d\n",
      trs_disk_getstep(0), trs_disk_getstep(1), trs_disk_getstep(2), trs_disk_getstep(3),
      trs_disk_getstep(4), trs_disk_getstep(5), trs_disk_getstep(6), trs_disk_getstep(7));
#endif
  fprintf(config_file, "%sstringy\n", stringy ? "" : "no");
  fprintf(config_file, "%ssupermem\n", supermem ? "" : "no");
  fprintf(config_file, "switches=0x%x\n", trs_uart_switches);
  fprintf(config_file, "%struedam\n", trs_disk_truedam ? "" : "no");
  fprintf(config_file, "%sturbo\n", timer_overclock ? "" : "no");
#if defined(SDL2) || !defined(NOX)
  fprintf(config_file, "%sturbopaste\n", turbo_paste ? "" : "no");
#endif
  fprintf(config_file, "turborate=%d\n", timer_overclock_rate);
  for (i = 0; i < 8; i++) {
    const char *diskname = stringy_get_name(i);

    if (diskname[0])
      fprintf(config_file, "wafer%d=%s\n", i, diskname);
  }

  fclose(config_file);
  return 0;
}

void trs_screen_var_reset(void)
{
  text80x24 = 0;
  screen640x240 = 0;
  screen_chars = 1024;
  row_chars = 64;
  col_chars = 16;
}

void trs_screen_caption(void)
{
  char title[80];

  if (cpu_panel)
    snprintf(title, 79, "AF:%04X BC:%04X DE:%04X HL:%04X IX/IY:%04X/%04X PC/SP:%04X/%04X",
             Z80_AF, Z80_BC, Z80_DE, Z80_HL, Z80_IX, Z80_IY, Z80_PC, Z80_SP);
  else {
    const char *trs_name[] = { "", "I", "", "III", "4", "4P" };

    snprintf(title, 79, "%sTRS-80 Model %s (%.2f MHz) %s%s",
             timer_overclock ? "Turbo " : "",
             trs_name[trs_model],
             z80_state.clockMHz,
             trs_paused ? "PAUSED " : "",
             trs_sound ? "" : "(Mute)");
  }
#ifdef SDL2
  SDL_SetWindowTitle(window, title);
#else
  SDL_WM_SetCaption(title, NULL);
#endif
}

void trs_screen_init(void)
{
  int led_height, led_width;
  int x, y;
  SDL_Color colors[2];

  switch (trs_model) {
    case 1:
      trs_charset = trs_charset1;
      currentmode = NORMAL;
      break;
    case 3:
      trs_charset = trs_charset3;
      currentmode = NORMAL;
      break;
    default:
      trs_charset = trs_charset4;
  }

  if (trs_model == 1) {
    if (trs_charset < 3)
      cur_char_width = 6 * scale;
    else
      cur_char_width = 8 * scale;
    cur_char_height = TRS_CHAR_HEIGHT * (scale * 2);
  } else {
    cur_char_width = TRS_CHAR_WIDTH * scale;
    if (screen640x240 || text80x24)
      cur_char_height = TRS_CHAR_HEIGHT4 * (scale * 2);
    else
      cur_char_height = TRS_CHAR_HEIGHT * (scale * 2);
  }

  border_width = fullscreen ? 0 : window_border_width;
  led_width = trs_show_led ? 8 : 0;
  led_height = led_width * scale;
  resize = (trs_model >= 4) ? resize4 : resize3;

  if (trs_model >= 3  && !resize) {
    OrigWidth = cur_char_width * 80 + 2 * border_width;
    left_margin = cur_char_width * (80 - row_chars) / 2 + border_width;
    OrigHeight = TRS_CHAR_HEIGHT4 * (scale * 2) * 24 + 2 * border_width + led_height;
    top_margin = (TRS_CHAR_HEIGHT4 * (scale * 2) * 24 -
                 cur_char_height * col_chars) / 2 + border_width;
  } else {
    OrigWidth = cur_char_width * row_chars + 2 * border_width;
    left_margin = border_width;
    OrigHeight = cur_char_height * col_chars + 2 * border_width + led_height;
    top_margin = border_width;
  }
  screen_height = OrigHeight - led_height;

#ifdef SDL2
  if (window == NULL) {
#ifdef XDEBUG
    debug("SDL_VIDEODRIVER=%s\n", SDL_GetCurrentVideoDriver());
#endif
    window = SDL_CreateWindow(NULL,
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              OrigWidth, OrigHeight,
                              SDL_WINDOW_HIDDEN);
    if (window == NULL)
      fatal("failed to create window: %s", SDL_GetError());
  }
  SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
  SDL_SetWindowSize(window, OrigWidth, OrigHeight);
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);
  screen = SDL_GetWindowSurface(window);
  if (screen == NULL)
    fatal("failed to get window surface: %s", SDL_GetError());
#else
  screen = SDL_SetVideoMode(OrigWidth, OrigHeight, 0, fullscreen ?
                            SDL_ANYFORMAT | SDL_FULLSCREEN : SDL_ANYFORMAT);
  if (screen == NULL)
    fatal("failed to set video mode: %s", SDL_GetError());
  SDL_WarpMouse(OrigWidth / 2, OrigHeight / 2);
#endif
  SDL_ShowCursor(mousepointer ? SDL_ENABLE : SDL_DISABLE);

  for (y = 0; y < G_YSIZE; y++)
    for (x = 0; x < G_XSIZE; x++)
      grafyx_rescale(y, x, grafyx_unscaled[y][x]);

  if (image)
    SDL_FreeSurface(image);
  image = SDL_CreateRGBSurfaceFrom(grafyx, G_XSIZE * scale * 8, G_YSIZE * scale * 2,
                                   1, G_XSIZE * scale, 1, 1, 1, 0);

#if defined(big_endian) && !defined(__linux)
  colors[0].r   = (background) & 0xFF;
  colors[0].g   = (background >> 8) & 0xFF;
  colors[0].b   = (background >> 16) & 0xFF;
  colors[1].r   = (foreground) & 0xFF;
  colors[1].g   = (foreground >> 8) & 0xFF;
  colors[1].b   = (foreground >> 16) & 0xFF;
  light_red     = SDL_MapRGB(screen->format, 0x00, 0x00, 0x40);
  bright_red    = SDL_MapRGB(screen->format, 0x00, 0x00, 0xff);
  light_orange  = SDL_MapRGB(screen->format, 0x40, 0x28, 0x40);
  bright_orange = SDL_MapRGB(screen->format, 0x00, 0xa0, 0xff);
#else
  colors[0].r   = (background >> 16) & 0xFF;
  colors[0].g   = (background >> 8) & 0xFF;
  colors[0].b   = (background) & 0xFF;
  colors[1].r   = (foreground >> 16) & 0xFF;
  colors[1].g   = (foreground >> 8) & 0xFF;
  colors[1].b   = (foreground) & 0xFF;
  light_red     = SDL_MapRGB(screen->format, 0x40, 0x00, 0x00);
  bright_red    = SDL_MapRGB(screen->format, 0xff, 0x00, 0x00);
  light_orange  = SDL_MapRGB(screen->format, 0x40, 0x28, 0x00);
  bright_orange = SDL_MapRGB(screen->format, 0xff, 0xa0, 0x00);
#endif

#ifdef SDL2
  SDL_SetPaletteColors(image->format->palette, colors, 0, 2);
#else
  SDL_SetPalette(image, SDL_LOGPAL, colors, 0, 2);
#endif

  TrsBlitMap(image->format->palette, screen->format);
  bitmap_init();

  trs_screen_caption();
  trs_screen_refresh();
}

static void addToDrawList(SDL_Rect *rect)
{
  if (drawnRectCount < MAX_RECTS)
    drawnRects[drawnRectCount++] = *rect;
}

#if defined(SDL2) || !defined(NOX)
static void DrawSelectionRectangle(int orig_x, int orig_y, int copy_x, int copy_y)
{
  int const bpp   = screen->format->BytesPerPixel;
  int const pitch = screen->pitch;
  Uint8 *pixels   = screen->pixels;
  Uint8 *pixel;
  int x, y;

  if (copy_x < orig_x) {
    int swap = copy_x;

    copy_x = orig_x;
    orig_x = swap;
  }
  if (copy_y < orig_y) {
    int swap = copy_y;

    copy_y = orig_y;
    orig_y = swap;
  }

  SDL_LockSurface(screen);

  copy_x *= bpp;
  orig_x *= bpp;
  copy_y *= pitch;
  orig_y *= pitch;

  pixel = pixels + orig_y + orig_x;
  for (x = 0; x < copy_x - orig_x + bpp; x++)
    *pixel++ ^= 0xFF;
  if (copy_y > orig_y) {
    pixel = pixels + copy_y + orig_x;
    for (x = 0; x < copy_x - orig_x + bpp; x++)
      *pixel++ ^= 0xFF;
  }
  for (y = orig_y + pitch; y < copy_y; y += pitch) {
    pixel = pixels + y + orig_x;
    for (x = 0; x < bpp; x++)
      *pixel++ ^= 0xFF;
  }
  if (copy_x > orig_x) {
    for (y = orig_y + pitch; y < copy_y; y += pitch) {
      pixel = pixels + y + copy_x;
        for (x = 0; x < bpp; x++)
          *pixel++ ^= 0xFF;
    }
  }

  SDL_UnlockSurface(screen);
}

static void ProcessCopySelection(int selectAll)
{
  static int orig_x = 0;
  static int orig_y = 0;
  static int end_x = 0;
  static int end_y = 0;
  static int copy_x = 0;
  static int copy_y = 0;
  static Uint8 mouse = 0;

  if (selectAll) {
    if (copyStatus == COPY_STARTED)
      return;
    if (copyStatus == COPY_DEFINED || copyStatus == COPY_CLEAR)
      DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
    orig_x = 0;
    orig_y = 0;
    copy_x = end_x = screen->w - scale;
    copy_y = end_y = screen_height - scale;
    DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
    selectionStartX = orig_x - left_margin;
    selectionStartY = orig_y - top_margin;
    selectionEndX = copy_x - left_margin;
    selectionEndY = copy_y - top_margin;
    drawnRectCount = MAX_RECTS;
    copyStatus = COPY_DEFINED;
  } else {
    mouse = SDL_GetMouseState(&copy_x, &copy_y);
    if (copy_x > screen->w - scale)
      copy_x = screen->w - scale;
    if (copy_y > screen_height - scale)
      copy_y = screen_height - scale;
    if ((copyStatus == COPY_IDLE) &&
        ((mouse & SDL_BUTTON(SDL_BUTTON_LEFT)) == 0)) {
      return;
    }
  }

  switch (copyStatus) {
    case COPY_IDLE:
      if (selectAll) {
        orig_x = 0;
        orig_y = 0;
        DrawSelectionRectangle(orig_x, orig_y, copy_x, copy_y);
        selectionStartX = orig_x - left_margin;
        selectionStartY = orig_y - top_margin;
        selectionEndX = copy_x - left_margin;
        selectionEndY = copy_y - top_margin;
        drawnRectCount = MAX_RECTS;
        copyStatus = COPY_DEFINED;
      }
      else if (mouse & SDL_BUTTON(SDL_BUTTON_LEFT) ) {
        orig_x = copy_x;
        orig_y = copy_y;
        DrawSelectionRectangle(orig_x, orig_y, copy_x, copy_y);
        drawnRectCount = MAX_RECTS;
        copyStatus = COPY_STARTED;
      }
      end_x = copy_x;
      end_y = copy_y;
      break;
    case COPY_STARTED:
      DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
      if (mouse & SDL_BUTTON(SDL_BUTTON_LEFT))
        DrawSelectionRectangle(orig_x, orig_y, copy_x, copy_y);
      drawnRectCount = MAX_RECTS;
      end_x = copy_x;
      end_y = copy_y;
      if ((mouse & SDL_BUTTON(SDL_BUTTON_LEFT)) == 0) {
        if (orig_x == copy_x && orig_y == copy_y) {
          copyStatus = COPY_IDLE;
        } else {
          DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
          selectionStartX = orig_x - left_margin;
          selectionStartY = orig_y - top_margin;
          selectionEndX = copy_x - left_margin;
          selectionEndY = copy_y - top_margin;
          copyStatus = COPY_DEFINED;
        }
      }
      break;
    case COPY_DEFINED:
      if (mouse & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) {
        DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
        orig_x = end_x = copy_x;
        orig_y = end_y = copy_y;
        DrawSelectionRectangle(orig_x, orig_y, copy_x, copy_y);
        drawnRectCount = MAX_RECTS;
        copyStatus = COPY_STARTED;
      }
      break;
    case COPY_CLEAR:
      DrawSelectionRectangle(orig_x, orig_y, end_x, end_y);
      drawnRectCount = MAX_RECTS;
      copyStatus = COPY_IDLE;
  }
}
#endif

/*
 * Flush SDL output
 */
void trs_sdl_flush(void)
{
#if defined(SDL2) || !defined(NOX)
  if (mousepointer) {
    if (!trs_emu_mouse && paste_state == PASTE_IDLE) {
      ProcessCopySelection(requestSelectAll);
      requestSelectAll = FALSE;
    }
  }
#endif
  if (drawnRectCount == 0)
    return;

  if (scanlines) {
#ifdef OLD_SCANLINES
    SDL_Rect rect;

    rect.x = 0;
    rect.w = OrigWidth;
    rect.h = scale;

    for (rect.y = 0; rect.y < screen_height; rect.y += (scale * 2))
      SDL_FillRect(screen, &rect, background);
#else
    int const width = screen->format->BytesPerPixel * scale * OrigWidth;
    Uint8 *pixels   = screen->pixels;
    Uint8 *pixel;
    int x, y;

    SDL_LockSurface(screen);
    for (y = 0; y < screen->pitch * screen_height;
        y += screen->pitch * (scale * 2)) {
      pixel = pixels + y;
      for (x = 0; x < width; x++)
        *pixel++ &= scanshade;
    }
    SDL_UnlockSurface(screen);
#endif
  }

  if (drawnRectCount == MAX_RECTS)
#ifdef SDL2
    SDL_UpdateWindowSurface(window);
  else
    SDL_UpdateWindowSurfaceRects(window, drawnRects, drawnRectCount);
#else
    SDL_UpdateRect(screen, 0, 0, 0, 0);
  else
    SDL_UpdateRects(screen, drawnRectCount, drawnRects);
#endif
  drawnRectCount = 0;
}

void trs_exit(int confirm)
{
  static int recursion = 0;

  if (recursion && confirm)
    return;
  recursion = 1;

  if (confirm) {
    SDL_Surface *buffer = SDL_ConvertSurface(screen, screen->format, SDL_SWSURFACE);
    if (!trs_gui_exit_sdltrs() && buffer) {
      SDL_BlitSurface(buffer, NULL, screen, NULL);
      SDL_FreeSurface(buffer);
      trs_screen_update();
      recursion = 0;
      return;
    }
  }
  exit(0);
}

void trs_sdl_cleanup(void)
{
  int i, ch;

  /* SDL cleanup */
  for (i = 0; i < 6; i++) {
    for (ch = 0; ch < MAXCHARS; ch++) {
      if (trs_char[i][ch]) {
        free(trs_char[i][ch]->pixels);
        SDL_FreeSurface(trs_char[i][ch]);
      }
    }
  }
  for (i = 0; i < 3; i++)
    for (ch = 0; ch < 64; ch++)
      SDL_FreeSurface(trs_box[i][ch]);

  SDL_FreeSurface(image);
#ifdef SDL2
  SDL_FreeSurface(screen);
  SDL_DestroyWindow(window);
#endif
  SDL_Quit();
}

static void trs_flip_fullscreen(void)
{
  static int window_scale = 1;

  fullscreen = !fullscreen;
  if (fullscreen) {
    window_scale = scale;
    scale = 1;
  } else
    scale = window_scale;

  trs_screen_init();
}

#if defined(SDL2) || !defined(NOX)
static char *trs_get_copy_data(void)
{
  static char copy_data[2048];
  char *curr_data = copy_data;
  Uint8 data;
  Uint8 *screen_ptr;
  int col, row;
  int start_col, end_col, start_row, end_row;

  if (grafyx_enable && !grafyx_overlay) {
    copy_data[0] = 0;
    return copy_data;
  }

  if (selectionStartX < 0)
    selectionStartX = 0;
  if (selectionStartY < 0)
    selectionStartY = 0;

  if (selectionStartX % cur_char_width == 0)
    start_col = selectionStartX / cur_char_width;
  else
    start_col = selectionStartX / cur_char_width + 1;

  if (selectionEndX % cur_char_width == cur_char_width - 1)
    end_col = selectionEndX / cur_char_width;
  else
    end_col = selectionEndX / cur_char_width - 1;

  if (selectionStartY % cur_char_height == 0)
    start_row = selectionStartY / cur_char_height;
  else
    start_row = selectionStartY / cur_char_height + 1;

  if (selectionEndY % cur_char_height >= cur_char_height / 2)
    end_row = selectionEndY / cur_char_height;
  else
    end_row = selectionEndY / cur_char_height - 1;

  if (end_col >= row_chars)
    end_col = row_chars - 1;
  if (end_row >= col_chars)
    end_row = col_chars - 1;

  for (row = start_row; row <= end_row; row++) {
    screen_ptr = &trs_screen[row * row_chars + start_col];
    for (col = start_col; col <= end_col; col++, screen_ptr++) {
      data = *screen_ptr;
      if (data < 0x20)
        data += 0x40;
      if ((currentmode & INVERSE) && (data & 0x80))
        data -= 0x80;
      if (data >= 0x20 && data <= 0x7e)
        *curr_data++ = data;
      else
        *curr_data++ = ' ';
    }
    if (row != end_row) {
#ifdef _WIN32
      *curr_data++ = 0xd;
#endif
      *curr_data++ = 0xa;
    }
  }
  *curr_data = 0;
  return copy_data;
}
#endif

/*
 * Get and process SDL event(s).
 *   If wait is true, process one event, blocking until one is available.
 *   If wait is false, process as many events as are available, returning
 *     when none are left.
 * Handle interrupt-driven uart input here too.
 */
void trs_get_event(int wait)
{
  SDL_Event event;
#ifdef SDL2
  SDL_Keysym keysym;
  Uint32 scancode = 0;

  SDL_StartTextInput();
#else
  SDL_keysym keysym;
#endif

  if (trs_model > 1)
    (void)trs_uart_check_avail();

  trs_sdl_flush();

  if (cpu_panel)
    trs_screen_caption();

#if defined(SDL2) || !defined(NOX)
  if (paste_state != PASTE_IDLE) {
    static Uint8 paste_key;

    if (SDL_PollEvent(&event)) {
      if (event.type == SDL_KEYDOWN) {
        if (paste_state == PASTE_KEYUP)
          trs_xlate_keysym(0x10000 | paste_key);
        paste_state = PASTE_KEYUP;
        paste_lastkey = TRUE;
      }
    }

    switch (paste_state) {
      case PASTE_GETNEXT:
        paste_lastkey = !PasteManagerGetChar(&paste_key);
#ifndef _WIN32
        if (paste_key == 0xa)
          paste_key = 0xd;
        else
#endif
        if (paste_key >= 0x5b && paste_key <= 0x60)
          paste_key += 0x20;
        else if (paste_key >= 0x7b && paste_key <= 0x7e)
          paste_key -= 0x20;
        trs_xlate_keysym(paste_key);
        paste_state = PASTE_KEYDOWN;
        break;
      case PASTE_KEYDOWN:
        trs_xlate_keysym(0x10000 | paste_key);
        paste_state = PASTE_KEYUP;
        break;
      case PASTE_KEYUP:
        if (paste_lastkey) {
          paste_state = PASTE_IDLE;
          if (turbo_paste)
            trs_turbo_mode(timer_saved);
          cycles_per_timer = cycles_saved;
        }
        else
          paste_state = PASTE_GETNEXT;
        break;
    }
    return;
  }
#endif

  do {
    if (wait)
      SDL_WaitEvent(&event);
    else
      if (!SDL_PollEvent(&event))
        return;

    switch (event.type) {
      case SDL_QUIT:
        trx_shutdown();
        trs_exit(0);
        break;
#ifdef SDL2
      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
          SDL_UpdateWindowSurface(window);
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
          if ((screen = SDL_GetWindowSurface(window)) == NULL)
            fatal("failed to get window surface: %s", SDL_GetError());
          trs_screen_refresh();
#else
      case SDL_ACTIVEEVENT:
        if (event.active.state & SDL_APPACTIVE) {
          if (event.active.gain) {
#endif
#if XDEBUG
            debug("Active\n");
#endif
            if (trs_model == 1)
              clear_key_queue();
          }
#ifndef SDL2
        }
#endif
        break;

      case SDL_KEYDOWN:
        keysym = event.key.keysym;
#if XDEBUG
        debug("KeyDown: mod 0x%x, scancode 0x%x keycode 0x%x\n",
            keysym.mod, keysym.scancode, keysym.sym);
#endif
#if defined(SDL2) || !defined(NOX)
        if (keysym.sym != SDLK_LALT) {
          if (copyStatus != COPY_IDLE) {
            copyStatus = COPY_CLEAR;
            trs_sdl_flush();
          }
        }
#endif

        switch (keysym.sym) {
          /* Trap some function keys here */
          case SDLK_F7:
            if (SDL_GetModState() & KMOD_SHIFT)
              call_function(EMULATOR);
            else
              call_function(GUI);
            continue;
          case SDLK_F8:
            trs_exit(!(SDL_GetModState() & KMOD_SHIFT));
            continue;
          case SDLK_F9:
            if (SDL_GetModState() & KMOD_SHIFT) {
              cpu_panel = !cpu_panel;
              trs_screen_caption();
            } else {
#ifdef ZBX
              if (fullscreen)
                trs_flip_fullscreen();
              trs_debug();
#else
              trs_flip_fullscreen();
#endif
            }
            continue;
          case SDLK_F10:
            trs_reset(SDL_GetModState() & KMOD_SHIFT);
            continue;
          case SDLK_F11:
            if (SDL_GetModState() & KMOD_SHIFT)
              call_function(SAVE_BMP);
            else
              call_function(KEYS);
            continue;
          case SDLK_F12:
            if (SDL_GetModState() & KMOD_SHIFT)
              trs_timer_init();
            else {
              trs_turbo_mode(!timer_overclock);
            }
            continue;
          case SDLK_PAUSE:
            call_function(PAUSE);
            continue;
#ifndef SDL2
          case SDLK_PRINT:
#else
          case SDLK_PRINTSCREEN:
#endif
            call_function(SAVE_BMP);
            continue;
#ifdef SDL2
          case SDLK_NUMLOCKCLEAR:
#else
          case SDLK_NUMLOCK:
#endif
            trs_keypad_joystick = !trs_keypad_joystick;
            trs_set_keypad_joystick();
            continue;
          default:
            break;
        }
        /* Trap the alt keys here */
        if (SDL_GetModState() & KMOD_LALT) {
          switch (keysym.sym) {
#if defined(SDL2) || !defined(NOX)
            case SDLK_c:
              PasteManagerStartCopy(trs_get_copy_data());
              copyStatus = COPY_IDLE;
              break;
            case SDLK_v:
            case SDLK_INSERT:
              if (turbo_paste) {
                timer_saved = timer_overclock;
                trs_turbo_mode(1);
              }
              cycles_saved = cycles_per_timer;
              cycles_per_timer *= 4;
              PasteManagerStartPaste();
              paste_state = PASTE_GETNEXT;
              break;
            case SDLK_a:
              requestSelectAll = mousepointer = TRUE;
              SDL_ShowCursor(SDL_ENABLE);
              break;
#endif
#ifdef _WIN32
            case SDLK_F4:
              trs_exit(1);
              break;
#endif
            case SDLK_DELETE:
              trs_reset(0);
              break;
            case SDLK_RETURN:
              trs_flip_fullscreen();
              break;
            case SDLK_HOME:
              fullscreen = 0;
              scale = 1;
              trs_screen_init();
              break;
            case SDLK_PAGEDOWN:
              fullscreen = 0;
              scale++;
              if (scale > MAX_SCALE)
                scale = 1;
              trs_screen_init();
              break;
            case SDLK_PAGEUP:
              fullscreen = 0;
              scale--;
              if (scale < 1)
                scale = MAX_SCALE;
              trs_screen_init();
              break;
            case SDLK_MINUS:
            case SDLK_8:
              if (z80_state.clockMHz > 0.1) {
                z80_state.clockMHz -= 0.1;
                cycles_per_timer = z80_state.clockMHz * 1000000 / timer_hz;
                trs_screen_caption();
              }
              break;
            case SDLK_PLUS:
            case SDLK_9:
              if (z80_state.clockMHz < 99.0) {
                z80_state.clockMHz += 0.1;
                cycles_per_timer = z80_state.clockMHz * 1000000 / timer_hz;
                trs_screen_caption();
              }
              break;
            case SDLK_PERIOD:
              mousepointer = !mousepointer;
#ifdef SDL2
              SDL_SetRelativeMouseMode(mousepointer ? SDL_FALSE : SDL_TRUE);
              SDL_SetWindowGrab(window, mousepointer ? SDL_FALSE : SDL_TRUE);
#else
              SDL_ShowCursor(mousepointer ? SDL_ENABLE : SDL_DISABLE);
              SDL_WM_GrabInput(mousepointer ? SDL_GRAB_OFF : SDL_GRAB_ON);
#endif
              break;
            case SDLK_b:
              trs_show_led = !trs_show_led;
              trs_screen_init();
              break;
            case SDLK_d:
            case SDLK_f:
              call_function(DISK);
              break;
            case SDLK_e:
              call_function(EMULATOR);
              break;
            case SDLK_g:
              call_function(STRINGY);
              break;
            case SDLK_h:
              call_function(HARD);
              break;
            case SDLK_i:
              call_function(INTERFACE);
              break;
            case SDLK_j:
              call_function(JOYGUI);
              break;
            case SDLK_k:
              call_function(KEYS);
              break;
            case SDLK_l:
              call_function(LOAD);
              break;
            case SDLK_m:
              call_function(GUI);
              break;
            case SDLK_n:
              trs_turbo_mode(!timer_overclock);
              break;
            case SDLK_o:
              call_function(OTHER);
              break;
            case SDLK_p:
              call_function(PAUSE);
              break;
            case SDLK_q:
            case SDLK_END:
              trs_exit(1);
              break;
            case SDLK_r:
              call_function(READ);
              break;
            case SDLK_s:
              call_function(SAVE);
              break;
            case SDLK_t:
              call_function(TAPE);
              break;
            case SDLK_u:
              trs_sound = !trs_sound;
              trs_screen_caption();
              break;
            case SDLK_w:
              call_function(WRITE);
              break;
            case SDLK_x:
              call_function(EXEC);
              break;
            case SDLK_y:
              scanlines = !scanlines;
              trs_screen_refresh();
              break;
            case SDLK_z:
#ifdef ZBX
              if (fullscreen)
                trs_flip_fullscreen();
              trs_debug();
#else
              trs_flip_fullscreen();
#endif
              break;
            case SDLK_0:
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
              if (SDL_GetModState() & KMOD_SHIFT) {
                trs_disk_remove(keysym.sym - SDLK_0);
              } else {
                char filename[FILENAME_MAX];

                if (trs_gui_file_browse(trs_disk_dir, filename, NULL, 0,
                      "Floppy Disk Image") != -1)
                  trs_disk_insert(keysym.sym - SDLK_0, filename);
                trs_screen_refresh();
              }
              break;
            default:
              break;
          }
          continue;
        }
        if (last_key[keysym.scancode])
        /*
         * We think this hardware key is already pressed.
         * Assume we are getting key repeat and ignore it.
         */
          break;

        /* Make Shift + CapsLock give lower case */
        if (((SDL_GetModState() & (KMOD_CAPS | KMOD_LSHIFT))
            == (KMOD_CAPS | KMOD_LSHIFT) ||
            ((SDL_GetModState() & (KMOD_CAPS | KMOD_RSHIFT))
            == (KMOD_CAPS | KMOD_RSHIFT)))
#ifdef SDL2
            && keysym.sym >= 'A' && keysym.sym <= 'Z')
          keysym.sym = (int) keysym.sym + 0x20;
#else
            && keysym.unicode >= 'A' && keysym.unicode <= 'Z')
          keysym.unicode = (int) keysym.unicode + 0x20;
#endif
        if (keysym.sym == SDLK_RSHIFT && trs_model == 1)
          keysym.sym = SDLK_LSHIFT;

        if (trs_model == 1) {
          switch (keysym.sym) {
            case SDLK_F1: keysym.sym = 0x115; break; /* _ */
            case SDLK_F2: keysym.sym = 0x120; break; /* \ */
            case SDLK_F3: keysym.sym = 0x121; break; /* ] */
            case SDLK_F4: keysym.sym = 0x122; break; /* ^ */
            default:
              break;
          }
        }

#ifdef SDL2
        /* Convert numeric keypad */
        if (keysym.sym >= SDLK_KP_1 && keysym.sym <= SDLK_KP_9)
          keysym.sym = (keysym.sym - SDLK_KP_1) + 0x101;
        else
        /* Convert arrow/control/function/shift keys */
        switch (keysym.sym) {
          case SDLK_KP_0:       keysym.sym = 0x100; break;
          case SDLK_UP:         keysym.sym = 0x111; break;
          case SDLK_DOWN:       keysym.sym = 0x112; break;
          case SDLK_RIGHT:      keysym.sym = 0x113; break;
          case SDLK_LEFT:       keysym.sym = 0x114; break;
          case SDLK_INSERT:     keysym.sym = 0x115; break;
          case SDLK_HOME:       keysym.sym = 0x116; break;
          case SDLK_END:        keysym.sym = 0x117; break;
          case SDLK_PAGEUP:     keysym.sym = 0x118; break;
          case SDLK_PAGEDOWN:   keysym.sym = 0x119; break;
          case SDLK_CAPSLOCK:   keysym.sym = 0x11d; break;
          case SDLK_SCROLLLOCK: keysym.sym = 0x11e; break;
          case SDLK_F1:         keysym.sym = 0x11a; break;
          case SDLK_F2:         keysym.sym = 0x11b; break;
          case SDLK_F3:         keysym.sym = 0x11c; break;
          case SDLK_F4:         keysym.sym = 0x11d; break;
          case SDLK_F5:         keysym.sym = 0x11e; break;
          case SDLK_F6:         keysym.sym = 0x11f; break;
          case SDLK_RSHIFT:     keysym.sym = 0x12f; break;
          case SDLK_LSHIFT:     keysym.sym = 0x130; break;
          case SDLK_LCTRL:      keysym.sym = 0x132; break;
          default:
            break;
        }

        if (SDL_GetModState() & (KMOD_SHIFT | KMOD_RALT)) {
          if (keysym.sym >= 0x21 && keysym.sym <= 0xDF) {
            scancode = keysym.scancode;
            break;
          }
        }
#else
        if (keysym.sym < 0x100 && keysym.unicode >= 0x20 && keysym.unicode <= 0xFF) {
          last_key[keysym.scancode] = keysym.unicode;
          trs_xlate_keysym(keysym.unicode);
        } else
#endif
        if (keysym.sym) {
          last_key[keysym.scancode] = keysym.sym;
          trs_xlate_keysym(keysym.sym);
        }
        break;

      case SDL_KEYUP:
        keysym = event.key.keysym;
#if XDEBUG
        debug("KeyUp: mod 0x%x, scancode 0x%x keycode 0x%x\n",
            keysym.mod, keysym.scancode, keysym.sym);
#endif
        if (SDL_GetModState() & KMOD_LALT)
          break;
        trs_xlate_keysym(0x10000 | last_key[keysym.scancode]);
        last_key[keysym.scancode] = 0;
        break;

      case SDL_JOYAXISMOTION:
        if (jaxis_mapped == 1 && (event.jaxis.axis == 0 || event.jaxis.axis == 1)) {
          static int hor_value = 0, ver_value = 0, hor_key = 0, ver_key = 0;
          int value = 0, trigger_keyup = 0, trigger_keydown = 0;

          if (event.jaxis.axis == 0)
            value = hor_value;
          else
            value = ver_value;

          if (event.jaxis.value < -JOY_BOUNCE) {
            if (value == 1)
              trigger_keyup = 1;
            if (value != -1)
              trigger_keydown = 1;
            value = -1;
          }
          else if (event.jaxis.value > JOY_BOUNCE) {
            if (value == -1)
              trigger_keyup = 1;
            if (value != 1)
              trigger_keydown = 1;
            value = 1;
          }
          else if (abs(event.jaxis.value) < JOY_BOUNCE / 8) {
            if (value)
              trigger_keyup = 1;
            value = 0;
          }

          if (trigger_keyup) {
            if (event.jaxis.axis == 0)
              trs_xlate_keysym(0x10000 | hor_key);
            else
              trs_xlate_keysym(0x10000 | ver_key);
          }
          if (trigger_keydown) {
            if (event.jaxis.axis == 0) {
              hor_key = (value == -1 ? 0x114 : 0x113); /* Left/Right */
              trs_xlate_keysym(hor_key);
            }
            else {
              ver_key = (value == -1 ? 0x111 : 0x112); /*  Up / Down */
              trs_xlate_keysym(ver_key);
            }
          }

          if (event.jaxis.axis == 0)
            hor_value = value;
          else
            ver_value = value;
        }
        else
          trs_joy_axis(event.jaxis.axis, event.jaxis.value, JOY_BOUNCE);
        break;

      case SDL_JOYHATMOTION:
        trs_joy_hat(event.jhat.value);
        break;

      case SDL_JOYBUTTONUP:
      case SDL_MOUSEBUTTONUP:
        if (event.type == SDL_MOUSEBUTTONUP) {
          if (mousepointer)
            break;
          else
            event.jbutton.button = event.button.button;
        }
        if (event.jbutton.button < N_JOYBUTTONS) {
          int const key = jbutton_map[event.jbutton.button];

          if (key >= 0)
            trs_xlate_keysym(0x10000 | key);
          else if (key == -1)
            trs_joy_button_up();
        }
        else
          trs_joy_button_up();
        break;

      case SDL_JOYBUTTONDOWN:
      case SDL_MOUSEBUTTONDOWN:
        if (event.type == SDL_MOUSEBUTTONDOWN) {
          if (mousepointer)
            break;
          else
            event.jbutton.button = event.button.button;
        }
        if (event.jbutton.button < N_JOYBUTTONS) {
          int const key = jbutton_map[event.jbutton.button];

          if (key >= 0)
            trs_xlate_keysym(key);
          else if (key == -1)
            trs_joy_button_down();
          else {
            call_function(key);
          }
        }
        else
          trs_joy_button_down();
        break;

      case SDL_MOUSEMOTION:
        if (!mousepointer) {
          SDL_MouseMotionEvent motion = event.motion;

          if (motion.xrel != 0) {
            if (jaxis_mapped) {
              if (abs(motion.xrel) > 2) {
                int const key = motion.xrel < 0 ? 0x114 : 0x113;
                int i;

                for (i = 0; i < abs(motion.xrel); i++)
                  trs_xlate_keysym(key);
              }
            } else
              trs_joy_axis(0, motion.xrel, 1);
          } else
            if (jaxis_mapped)
              trs_xlate_keysym(0x10000);

          if (motion.yrel != 0) {
            if (jaxis_mapped) {
              if (abs(motion.yrel) > 2) {
                int const key = motion.yrel < 0 ? 0x111 : 0x112;
                int i;

                for (i = 0; i < abs(motion.yrel); i++)
                  trs_xlate_keysym(key);
              }
            } else
              trs_joy_axis(1, motion.yrel, 2);
          } else
            if (jaxis_mapped)
              trs_xlate_keysym(0x10000);
        }
        break;

#ifdef SDL2
      case SDL_TEXTINPUT:
        if (scancode) {
          last_key[scancode] = event.text.text[0];
          trs_xlate_keysym(event.text.text[0]);
          scancode = 0;
        }
        break;
#endif

      default:
#if XDEBUG
      /* debug("Unhandled event: type %d\n", event.type); */
#endif
        break;
    }
    if (trs_paused) {
      if (fullscreen)
        trs_gui_display_pause();
    }
  } while (!wait);
#ifdef SDL2
  SDL_StopTextInput();
#endif
}

void trs_screen_expanded(int flag)
{
  int const bit = flag ? EXPANDED : 0;

  if ((currentmode ^ bit) & EXPANDED) {
    currentmode ^= EXPANDED;
    SDL_FillRect(screen, NULL, background);
    trs_screen_refresh();
  }
}

void trs_screen_inverse(int flag)
{
  int const bit = flag ? INVERSE : 0;
  int i;

  if ((currentmode ^ bit) & INVERSE) {
    currentmode ^= INVERSE;
    for (i = 0; i < screen_chars; i++) {
      if (trs_screen[i] & 0x80)
        trs_screen_write_char(i, trs_screen[i]);
    }
  }
}

void trs_screen_alternate(int flag)
{
  int const bit = flag ? ALTERNATE : 0;
  int i;

  if ((currentmode ^ bit) & ALTERNATE) {
    currentmode ^= ALTERNATE;
    for (i = 0; i < screen_chars; i++) {
      if (trs_screen[i] >= 0xc0)
        trs_screen_write_char(i, trs_screen[i]);
    }
  }
}

static void trs_screen_640x240(int flag)
{
  if (flag == screen640x240) return;
  screen640x240 = flag;
  if (flag) {
    row_chars = 80;
    col_chars = 24;
    cur_char_height = TRS_CHAR_HEIGHT4 * (scale * 2);
  } else {
    row_chars = 64;
    col_chars = 16;
    cur_char_height = TRS_CHAR_HEIGHT * (scale * 2);
  }
  screen_chars = row_chars * col_chars;
  if (resize)
    trs_screen_init();
  else {
    left_margin = cur_char_width * (80 - row_chars) / 2 + border_width;
    top_margin = (TRS_CHAR_HEIGHT4 * (scale * 2) * 24 -
        cur_char_height * col_chars) / 2 + border_width;
    if (left_margin > border_width || top_margin > border_width)
      SDL_FillRect(screen, NULL, background);
    trs_screen_refresh();
  }
}

void trs_screen_80x24(int flag)
{
  if (!grafyx_enable || grafyx_overlay)
    trs_screen_640x240(flag);
  text80x24 = flag;
}

void screen_init(void)
{
  /* initially, screen is blank (i.e. full of spaces) */
  memset(trs_screen, ' ', sizeof(trs_screen));
  memset(grafyx, 0, (2 * G_YSIZE * MAX_SCALE) * (G_XSIZE * MAX_SCALE));
}

static void
boxes_init(int fg_color, int bg_color, int width, int height, int expanded)
{
  int graphics_char, bit;
  SDL_Rect fullrect;
  SDL_Rect bits[6];

  /*
   * Calculate what the 2x3 boxes look like.
   */
  bits[0].x = bits[2].x = bits[4].x = 0;
  bits[0].w = bits[2].w = bits[4].w =
    bits[1].x = bits[3].x = bits[5].x = width / 2;
  bits[1].w = bits[3].w = bits[5].w = width - bits[1].x;

  bits[0].y = bits[1].y = 0;
  bits[0].h = bits[1].h =
    bits[2].y = bits[3].y = height / 3;
  bits[4].y = bits[5].y = (height * 2) / 3;
  bits[2].h = bits[3].h = bits[4].y - bits[2].y;
  bits[4].h = bits[5].h = height - bits[4].y;

  fullrect.x = 0;
  fullrect.y = 0;
  fullrect.h = height;
  fullrect.w = width;

  for (graphics_char = 0; graphics_char < 64; ++graphics_char) {
    if (trs_box[expanded][graphics_char])
      SDL_FreeSurface(trs_box[expanded][graphics_char]);
    trs_box[expanded][graphics_char] =
      SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32,
#if defined(big_endian) && !defined(__linux)
                           0x000000ff, 0x0000ff00, 0x00ff0000, 0);
#else
                           0x00ff0000, 0x0000ff00, 0x000000ff, 0);
#endif

    /* Clear everything */
    SDL_FillRect(trs_box[expanded][graphics_char], &fullrect, bg_color);

    for (bit = 0; bit < 6; ++bit) {
      if (graphics_char & (1 << bit)) {
        SDL_FillRect(trs_box[expanded][graphics_char], &bits[bit], fg_color);
      }
    }
  }
}

static SDL_Surface *CreateSurfaceFromDataScale(const Uint8 *data,
    unsigned int fg_color,
    unsigned int bg_color,
    unsigned int scale_x,
    unsigned int scale_y)
{
  unsigned int *mydata, *currdata;
  Uint8 *mypixels, *currpixel;
  int i, j, w;

  /*
   * Allocate a bit more room than necessary - There shouldn't be
   * any proportional characters, but just in case...
   * The memory allocated for "mydata" will be released in the
   * "bitmap_init" and "trs_sdl_cleanup" functions.
   */
  mydata = (unsigned int *)malloc(TRS_CHAR_WIDTH * TRS_CHAR_HEIGHT *
      scale_x * scale_y * sizeof(unsigned int));
  mypixels = (Uint8 *)malloc(TRS_CHAR_WIDTH * TRS_CHAR_HEIGHT * 8);
  if (mydata == NULL || mypixels == NULL)
    fatal("CreateSurfaceFromDataScale: failed to allocate memory");

  /* Read the character data */
  for (j = 0; (unsigned)j < TRS_CHAR_WIDTH * TRS_CHAR_HEIGHT; j += 8)
    for (i = j + 7; i >= j; i--)
      *(mypixels + i) = (*(data + (j >> 3)) >> (i - j)) & 1;

  currdata = mydata;
  /* And prepare our rescaled character. */
  for (j = 0; (unsigned)j < TRS_CHAR_HEIGHT * scale_y; j++) {
    currpixel = mypixels + ((j / scale_y) * TRS_CHAR_WIDTH);
    for (w = 0; w < TRS_CHAR_WIDTH; w++) {
      if (*currpixel++ == 0) {
        for (i = 0; (unsigned)i < scale_x; i++)
          *currdata++ = bg_color;
      }
      else {
        for (i = 0; (unsigned)i < scale_x; i++)
          *currdata++ = fg_color;
      }
    }
  }

  free(mypixels);

  return SDL_CreateRGBSurfaceFrom(mydata, TRS_CHAR_WIDTH * scale_x,
         TRS_CHAR_HEIGHT * scale_y, 32, TRS_CHAR_WIDTH * scale_x * 4,
#if defined(big_endian) && !defined(__linux)
         0x000000ff, 0x0000ff00, 0x00ff0000, 0);
#else
         0x00ff0000, 0x0000ff00, 0x000000ff, 0);
#endif
}

static void bitmap_init(void)
{
  /* Initialize from built-in font bitmaps. */
  int i;

  for (i = 0; i < MAXCHARS; i++) {
    if (trs_char[0][i]) {
      free(trs_char[0][i]->pixels);
      SDL_FreeSurface(trs_char[0][i]);
    }
    trs_char[0][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
        foreground, background, scale, scale * 2);

    if (trs_char[1][i]) {
      free(trs_char[1][i]->pixels);
      SDL_FreeSurface(trs_char[1][i]);
    }
    trs_char[1][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
        foreground, background, scale * 2, scale * 2);

    if (trs_char[2][i]) {
      free(trs_char[2][i]->pixels);
      SDL_FreeSurface(trs_char[2][i]);
    }
    trs_char[2][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
        background, foreground, scale, scale * 2);

    if (trs_char[3][i]) {
      free(trs_char[3][i]->pixels);
      SDL_FreeSurface(trs_char[3][i]);
    }
    trs_char[3][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
        background, foreground, scale * 2, scale * 2);

    if (trs_char[4][i]) {
      free(trs_char[4][i]->pixels);
      SDL_FreeSurface(trs_char[4][i]);
    }
    /* For the GUI, make sure we have brackets, backslash and block graphics */
    if ((i >= '[' && i <= ']') || i >= 128)
      trs_char[4][i] = CreateSurfaceFromDataScale(trs_char_data[0][i],
          gui_foreground, gui_background, scale, scale * 2);
    else
      trs_char[4][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
          gui_foreground, gui_background, scale, scale * 2);

    if (trs_char[5][i]) {
      free(trs_char[5][i]->pixels);
      SDL_FreeSurface(trs_char[5][i]);
    }
    if ((i >= '[' && i <= ']') || i >= 128)
      trs_char[5][i] = CreateSurfaceFromDataScale(trs_char_data[0][i],
          gui_background, gui_foreground, scale, scale * 2);
    else
      trs_char[5][i] = CreateSurfaceFromDataScale(trs_char_data[trs_charset][i],
          gui_background, gui_foreground, scale, scale * 2);
  }
  boxes_init(foreground, background,
      cur_char_width, TRS_CHAR_HEIGHT * (scale * 2), 0);
  boxes_init(foreground, background,
      cur_char_width * 2, TRS_CHAR_HEIGHT * (scale * 2), 1);
  boxes_init(gui_foreground, gui_background,
      cur_char_width, TRS_CHAR_HEIGHT * (scale * 2), 2);
}

void trs_screen_refresh(void)
{
#if XDEBUG
  debug("trs_screen_refresh\n");
#endif
  if (grafyx_enable && !grafyx_overlay) {
    int const srcx   = cur_char_width * grafyx_xoffset;
    int const srcy   = (scale * 2) * grafyx_yoffset;
    int const dunx   = (G_XSIZE * scale * 8) - srcx;
    int const duny   = (G_YSIZE * scale * 2) - srcy;
    int const height = cur_char_height * col_chars;
    int const width  = cur_char_width  * row_chars;
    SDL_Rect srcRect, dstRect;

    srcRect.x = srcx;
    srcRect.y = srcy;
    srcRect.w = width;
    srcRect.h = height;
    dstRect.x = left_margin;
    dstRect.y = top_margin;
    SDL_BlitSurface(image, &srcRect, screen, &dstRect);
    addToDrawList(&dstRect);
    /* Draw wrapped portions if any */
    if (dunx < width) {
      srcRect.x = 0;
      srcRect.y = srcy;
      srcRect.w = width - dunx;
      srcRect.h = height;
      dstRect.x = left_margin + dunx;
      dstRect.y = top_margin;
      SDL_BlitSurface(image, &srcRect, screen, &dstRect);
      addToDrawList(&dstRect);
    }
    if (duny < height) {
      srcRect.x = srcx;
      srcRect.y = 0;
      srcRect.w = width;
      srcRect.h = height - duny;
      dstRect.x = left_margin;
      dstRect.y = top_margin + duny;
      SDL_BlitSurface(image, &srcRect, screen, &dstRect);
      addToDrawList(&dstRect);
      if (dunx < width) {
        srcRect.x = 0;
        srcRect.y = 0;
        srcRect.w = width - dunx;
        srcRect.h = height - duny;
        dstRect.x = left_margin + dunx;
        dstRect.y = top_margin + duny;
        SDL_BlitSurface(image, &srcRect, screen, &dstRect);
        addToDrawList(&dstRect);
      }
    }
  } else {
    int i;

    for (i = 0; i < screen_chars; i++)
      trs_screen_write_char(i, trs_screen[i]);
  }

  if (trs_show_led) {
    trs_disk_led(-1, 0);
    trs_hard_led(-1, 0);
    trs_turbo_led();
  }
  drawnRectCount = MAX_RECTS; /* Will force redraw of whole screen */
  trs_sdl_flush();
}

void trs_disk_led(int drive, int on_off)
{
  static int countdown[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int i;
  SDL_Rect rect;

  rect.w = 16 * scale;
  rect.h = 4 * scale;
  rect.y = OrigHeight - rect.h;

  if (drive == -1) {
    for (i = 0; i < 8; i++) {
      if (on_off == -1)
        countdown[i] = 0;
      rect.x = border_width + 24 * scale * i;
      SDL_FillRect(screen, &rect, countdown[i] ? bright_red : light_red);
      addToDrawList(&rect);
    }
  }
  else if (on_off) {
    if (countdown[drive] == 0) {
      rect.x = border_width + 24 * scale * drive;
      SDL_FillRect(screen, &rect, bright_red);
      addToDrawList(&rect);
    }
    countdown[drive] = 2 * timer_hz;
  }
  else {
    for (i = 0; i < 8; i++) {
      if (countdown[i]) {
        countdown[i]--;
        if (countdown[i] == 0) {
          rect.x = border_width + 24 * scale * i;
          SDL_FillRect(screen, &rect, light_red);
          addToDrawList(&rect);
        }
      }
    }
  }
}

void trs_hard_led(int drive, int on_off)
{
  static int countdown[4] = { 0, 0, 0, 0 };
  int const drive0_led_x = OrigWidth - border_width - 88 * scale;
  int i;
  SDL_Rect rect;

  rect.w = 16 * scale;
  rect.h = 4 * scale;
  rect.y = OrigHeight - rect.h;

  if (drive == -1) {
    for (i = 0; i < 4; i++) {
      if (on_off == -1)
        countdown[i] = 0;
      rect.x = drive0_led_x + 24 * scale * i;
      SDL_FillRect(screen, &rect, countdown[i] ? bright_red : light_red);
      addToDrawList(&rect);
    }
  }
  else if (on_off) {
    if (countdown[drive] == 0) {
      rect.x = drive0_led_x + 24 * scale * drive;
      SDL_FillRect(screen, &rect, bright_red);
      addToDrawList(&rect);
    }
    countdown[drive] = timer_hz / 2;
  }
  else {
    for (i = 0; i < 4; i++) {
      if (countdown[i]) {
        countdown[i]--;
        if (countdown[i] == 0) {
          rect.x = drive0_led_x + 24 * scale * i;
          SDL_FillRect(screen, &rect, light_red);
          addToDrawList(&rect);
        }
      }
    }
  }
}

void trs_turbo_led(void)
{
  SDL_Rect rect;

  rect.w = 16 * scale;
  rect.h = 4 * scale;
  rect.x = (OrigWidth - border_width) / 2 - 8 * scale;
  rect.y = OrigHeight - rect.h;

  SDL_FillRect(screen, &rect, timer_overclock ? bright_orange : light_orange);
  addToDrawList(&rect);
}

void trs_screen_write_char(unsigned int position, Uint8 char_index)
{
  unsigned int row, col;
  int expanded;
  SDL_Rect srcRect, dstRect;

  if (position >= (unsigned int)screen_chars)
    return;
  trs_screen[position] = char_index;
  if ((currentmode & EXPANDED) && (position & 1))
    return;
  if (grafyx_enable && !grafyx_overlay)
    return;

  if (row_chars == 64) {
    row = position / 64;
    col = position - (row * 64);
  } else {
    row = position / 80;
    col = position - (row * 80);
  }

  expanded = (currentmode & EXPANDED) != 0;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = cur_char_width * (expanded + 1);
  srcRect.h = cur_char_height;
  dstRect.x = col * cur_char_width + left_margin;
  dstRect.y = row * cur_char_height + top_margin;

  if (trs_model == 1 && char_index >= 0xc0) {
    /* On Model I, 0xc0-0xff is another copy of 0x80-0xbf */
    char_index -= 0x40;
  }
  if (char_index >= 0x80 && char_index <= 0xbf && !(currentmode & INVERSE)) {
    /* Use box graphics character bitmap */
    SDL_BlitSurface(trs_box[expanded][char_index - 0x80], &srcRect, screen, &dstRect);
  } else {
    /* Use regular character bitmap */
    if (trs_model > 1 && char_index >= 0xc0 &&
        (currentmode & (ALTERNATE + INVERSE)) == 0) {
      char_index -= 0x40;
    }
    if ((currentmode & INVERSE) && (char_index & 0x80)) {
      expanded += 2;
      char_index &= 0x7f;
    }
    SDL_BlitSurface(trs_char[expanded][char_index], &srcRect, screen, &dstRect);
  }
  addToDrawList(&dstRect);

  /* Overlay grafyx on character */
  if (grafyx_enable) {
    /* assert(grafyx_overlay); */
    int const srcx = ((col + grafyx_xoffset) % G_XSIZE) * cur_char_width;
    int const srcy = (row * cur_char_height + grafyx_yoffset * (scale * 2))
      % (G_YSIZE * (scale * 2));
    int const duny = (G_YSIZE * scale * 2) - srcy;

    srcRect.x = srcx;
    srcRect.y = srcy;
    TrsSoftBlit(image, &srcRect, screen, &dstRect, 1);
    addToDrawList(&dstRect);
    /* Draw wrapped portion if any */
    if (duny < cur_char_height) {
      srcRect.y = 0;
      srcRect.h -= duny;
      dstRect.y += duny;
      TrsSoftBlit(image, &srcRect, screen, &dstRect, 1);
      addToDrawList(&dstRect);
    }
  }

  if (hrg_enable)
    hrg_update_char(position);
}

void trs_screen_update(void)
{
#ifdef SDL2
  SDL_UpdateWindowSurface(window);
#else
  SDL_UpdateRect(screen, 0, 0, 0, 0);
#endif
}

void trs_gui_clear_rect(int x, int y, int w, int h)
{
  SDL_Rect rect;

  /* Add offsets if we are in 80x24 mode */
  if (row_chars == 80) {
    x += 8;
    y += 4;
  }

  rect.x = x * cur_char_width + left_margin;
  rect.y = y * cur_char_height + top_margin;
  rect.w = w * cur_char_width;
  rect.h = h * cur_char_height;

  SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format,
#if defined(big_endian) && !defined(__linux)
      (gui_background & 0xFF),
      (gui_background >> 8) & 0xFF,
      (gui_background >> 16) & 0xFF));
#else
      (gui_background >> 16) & 0xFF,
      (gui_background >> 8) & 0xFF,
      (gui_background & 0xFF)));
#endif
}

void trs_gui_write_char(int col, int row, Uint8 char_index, int invert)
{
  SDL_Rect srcRect, dstRect;

  /* Add offsets if we are in 80x24 mode */
  if (row_chars == 80) {
    row += 4;
    col += 8;
  }

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.w = cur_char_width;
  srcRect.h = cur_char_height;
  dstRect.x = col * cur_char_width + left_margin;
  dstRect.y = row * cur_char_height + top_margin;

  if (trs_model == 1 && char_index >= 0xc0)
    /* On Model I, 0xc0-0xff is another copy of 0x80-0xbf */
    char_index -= 0x40;
  if (char_index >= 0x80 && char_index <= 0xbf && !(currentmode & INVERSE)) {
    /* Use graphics character bitmap instead of font */
    SDL_BlitSurface(trs_box[2][char_index - 0x80], &srcRect, screen, &dstRect);
  } else {
    /* Draw character using a builtin bitmap */
    if (trs_model > 1 && char_index >= 0xc0 &&
        (currentmode & (ALTERNATE + INVERSE)) == 0)
      char_index -= 0x40;
    SDL_BlitSurface(trs_char[invert ? 5 : 4][char_index], &srcRect, screen, &dstRect);
  }
}

static void grafyx_write_byte(int x, int y, char byte)
{
  int const screen_x = ((x - grafyx_xoffset + G_XSIZE) % G_XSIZE);
  int const screen_y = ((y - grafyx_yoffset + G_YSIZE) % G_YSIZE);
  int const on_screen = screen_x < row_chars &&
    screen_y < col_chars * cur_char_height / (scale * 2);
  SDL_Rect srcRect, dstRect;

  if (grafyx_enable && grafyx_overlay && on_screen) {
    srcRect.x = x * cur_char_width;
    srcRect.y = y * (scale * 2);
    srcRect.w = cur_char_width;
    srcRect.h = scale * 2;
    dstRect.x = left_margin + screen_x * cur_char_width;
    dstRect.y = top_margin + screen_y * (scale * 2);
    /* Erase old byte, preserving text */
    TrsSoftBlit(image, &srcRect, screen, &dstRect, 1);
  }

  /* Save new byte in local memory */
  grafyx_unscaled[y][x] = byte;
  grafyx_rescale(y, x, byte);

  if (grafyx_enable && on_screen) {
    /* Draw new byte */
    srcRect.x = x * cur_char_width;
    srcRect.y = y * (scale * 2);
    srcRect.w = cur_char_width;
    srcRect.h = scale * 2;
    dstRect.x = left_margin + screen_x * cur_char_width;
    dstRect.y = top_margin + screen_y * (scale * 2);
    TrsSoftBlit(image, &srcRect, screen, &dstRect, grafyx_overlay);
    addToDrawList(&dstRect);
  }
}

static void grafyx_rescale(int y, int x, char byte)
{
  if (scale == 1) {
    int const p = y * 2 * G_XSIZE + x;

    grafyx[p] = byte;
    grafyx[p + G_XSIZE] = byte;
  } else {
    char exp[MAX_SCALE];
    int i, j;
    int p = y * (scale * 2) * (G_XSIZE * scale) + x * scale;
    int const s = (G_XSIZE * scale) - scale;

    switch (scale) {
      case 2:
        exp[1] =  ((byte & 0x01)       + ((byte & 0x02) << 1)
               +  ((byte & 0x04) << 2) + ((byte & 0x08) << 3)) * 3;
        exp[0] = (((byte & 0x10) >> 4) + ((byte & 0x20) >> 3)
               +  ((byte & 0x40) >> 2) + ((byte & 0x80) >> 1)) * 3;
        break;
      case 3:
        exp[2] =  ((byte & 0x01)            + ((byte & 0x02) << 2)
               +  ((byte & 0x04) << 4)) * 7;
        exp[1] = (((byte & 0x08) >> 2)      +  (byte & 0x10)
               +  ((byte & 0x20) << 2)) * 7 + ((byte & 0x04) >> 2);
        exp[0] = (((byte & 0x40) >> 4)      + ((byte & 0x80) >> 2)) * 7
               +  ((byte & 0x20) >> 5)  * 3;
        break;
      case 4:
        exp[3] =  ((byte & 0x01)       + ((byte & 0x02) << 3)) * 15;
        exp[2] = (((byte & 0x04) >> 2) + ((byte & 0x08) << 1)) * 15;
        exp[1] = (((byte & 0x10) >> 4) + ((byte & 0x20) >> 1)) * 15;
        exp[0] = (((byte & 0x40) >> 6) + ((byte & 0x80) >> 3)) * 15;
        break;
    }

    for (j = 0; j < scale * 2; j++) {
      for (i = 0; i < scale; i++)
        grafyx[p++] = exp[i];
      p += s;
    }
  }
}

void grafyx_write_x(int value)
{
  grafyx_x = value;
}

void grafyx_write_y(int value)
{
  grafyx_y = value;
}

void grafyx_write_data(int value)
{
  grafyx_write_byte(grafyx_x % G_XSIZE, grafyx_y, value);
  if (!(grafyx_mode & G_XNOCLKW)) {
    if (grafyx_mode & G_XDEC)
      grafyx_x--;
    else
      grafyx_x++;
  }
  if (!(grafyx_mode & G_YNOCLKW)) {
    if (grafyx_mode & G_YDEC)
      grafyx_y--;
    else
      grafyx_y++;
  }
}

int grafyx_read_data(void)
{
  int const value = grafyx_unscaled[grafyx_y][grafyx_x % G_XSIZE];

  if (!(grafyx_mode & G_XNOCLKR)) {
    if (grafyx_mode & G_XDEC)
      grafyx_x--;
    else
      grafyx_x++;
  }
  if (!(grafyx_mode & G_YNOCLKR)) {
    if (grafyx_mode & G_YDEC)
      grafyx_y--;
    else
      grafyx_y++;
  }
  return value;
}

void grafyx_write_mode(int value)
{
  const Uint8 old_enable = grafyx_enable;
  const Uint8 old_overlay = grafyx_overlay;

  grafyx_enable = value & G_ENABLE;
  if (grafyx_microlabs)
    grafyx_overlay = (value & G_UL_NOTEXT) == 0;
  grafyx_mode = value;
  trs_screen_640x240((grafyx_enable && !grafyx_overlay) || text80x24);
  if (old_enable != grafyx_enable ||
      (grafyx_enable && old_overlay != grafyx_overlay))
    trs_screen_refresh();
}

void grafyx_write_xoffset(int value)
{
  const Uint8 old_xoffset = grafyx_xoffset;

  grafyx_xoffset = value % G_XSIZE;
  if (grafyx_enable && old_xoffset != grafyx_xoffset)
    trs_screen_refresh();
}

void grafyx_write_yoffset(int value)
{
  const Uint8 old_yoffset = grafyx_yoffset;

  grafyx_yoffset = value;
  if (grafyx_enable && old_yoffset != grafyx_yoffset)
    trs_screen_refresh();
}

void grafyx_write_overlay(int value)
{
  const Uint8 old_overlay = grafyx_overlay;

  grafyx_overlay = value & 1;
  if (grafyx_enable && old_overlay != grafyx_overlay) {
    trs_screen_640x240((grafyx_enable && !grafyx_overlay) || text80x24);
    trs_screen_refresh();
  }
}

int grafyx_get_microlabs(void)
{
  return grafyx_microlabs;
}

void grafyx_set_microlabs(int on_off)
{
  grafyx_microlabs = on_off;
}

/* Model III MicroLabs support */
void grafyx_m3_reset(void)
{
  if (grafyx_microlabs) grafyx_m3_write_mode(0);
}

void grafyx_m3_write_mode(int value)
{
  int const enable = (value & G3_ENABLE) != 0;
  int const changed = (enable != grafyx_enable);

  grafyx_enable = enable;
  grafyx_overlay = enable;
  grafyx_mode = value;
  grafyx_y = G3_YLOW(value);
  if (changed) trs_screen_refresh();
}

int grafyx_m3_write_byte(int position, int byte)
{
  if (grafyx_microlabs && (grafyx_mode & G3_COORD)) {
    grafyx_write_byte(position % 64, (position / 64) * 12 + grafyx_y, byte);
    return 1;
  } else
    return 0;
}

Uint8 grafyx_m3_read_byte(int position)
{
  if (grafyx_microlabs && (grafyx_mode & G3_COORD)) {
    return grafyx_unscaled[(position / 64) * 12 + grafyx_y][position % 64];
  } else
    return trs_screen[position];
}

/*
 *     The Lowe Electronics LE18 is yet another fairly simple
 *     I/O based 384x192 graphics adapter writing 6bits per
 *     TRS80 character
 *
 *     Port EC (R)
 *     7: goes high for blanking - can spin until high to avoid noise
 *     6: on/off status
 *     5-0: pixel data bit 0 is left
 *
 *     Port ED (W)
 *     7-6: unused
 *     5-0: X position (chars)
 *     Port EE (W)
 *     7-0: Y position (lines)
 *
 *     Port EF (W)
 *     7-1: unused
 *     0: hi res (1 = on)
 */

static Uint8 le18_x, le18_y, le18_on;
int lowe_le18;

void lowe_le18_write_x(int value)
{
  /* This really is 0-255. The unit has 16K x 6bit of RAM
     of which only 12K is displayed. You can use the rest
     as a 4K x 6bit area for .. not a lot really */
  le18_x = value & 63;
}

void lowe_le18_write_y(int value)
{
  le18_y = value;
}

static Uint8 pack8to6(Uint8 c)
{
  return ((c & 0x70) >> 1) | (c & 7);
}

static Uint8 expand6to8(Uint8 c)
{
  Uint8 r;

  r = (c & 0x07);
  if (r & 0x04)
    r |= 0x08;
  r |= (c << 1) & 0x70;
  if (r & 0x40)
    r |= 0x80;
  return r;
}

int lowe_le18_read(void)
{
  if (!lowe_le18)
    return 0xFF;
  return pack8to6(grafyx_unscaled[le18_y][le18_x]) | 0x80
          | ((le18_on) ? 0x40 : 0x00);
}

void lowe_le18_write_data(int value)
{
  if (lowe_le18)
    grafyx_write_byte(le18_x, le18_y, expand6to8(value & 0x3F));
}

void lowe_le18_write_control(int value)
{
  if (lowe_le18 && ((le18_on ^ value) & 1)) {
    le18_on = value & 1;
    grafyx_enable = le18_on;
    grafyx_overlay = le18_on;
    trs_screen_refresh();
  }
}

/*
 * Support for Model I HRG1B 384*192 graphics card
 * (sold in Germany for Model I and Video Genie by RB-Elektronik).
 *
 * Assignment of ports is as follows:
 *    Port 0x00 (out): switch graphics screen off (value ignored).
 *    Port 0x01 (out): switch graphics screen on (value ignored).
 *    Port 0x02 (out): select screen memory address (LSB).
 *    Port 0x03 (out): select screen memory address (MSB).
 *    Port 0x04 (in):  read byte from screen memory.
 *    Port 0x05 (out): write byte to screen memory.
 * (The real hardware decodes only address lines A0-A2 and A7, so
 * that there are several "shadow" ports in the region 0x08-0x7d.
 * However, these undocumented ports are not implemented here.)
 *
 * The 16-bit memory address (port 2 and 3) is used for subsequent
 * read or write operations. It corresponds to a position on the
 * graphics screen, in the following way:
 *    Bits 0-5:   character column address (0-63)
 *    Bits 6-9:   character row address (0-15)
 *                (i.e. bits 0-9 are the "PRINT @" position.)
 *    Bits 10-13: address of line within character cell (0-11)
 *    Bits 14-15: not used
 *
 *      <----port 2 (LSB)---->  <-------port 3 (MSB)------->
 * Bit: 0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15
 *      <-column addr.->  <row addr>  <-line addr.->  <n.u.>
 *
 * Reading from port 4 or writing to port 5 will access six
 * neighbouring pixels corresponding (from left to right) to bits
 * 0-5 of the data byte. Bits 6 and 7 are present in memory, but
 * are ignored.
 *
 * In expanded mode (32 chars per line), the graphics screen has
 * only 192*192 pixels. Pixels with an odd column address (i.e.
 * every second group of 6 pixels) are suppressed.
 */

/* Initialize HRG. */
static void
hrg_init(void)
{
  int i;

  /* Precompute arrays of pixel sizes and offsets. */
  for (i = 0; i <= 6; i++) {
    hrg_pixel_x[0][i] = cur_char_width * i / 6;
    hrg_pixel_x[1][i] = cur_char_width * 2 * i / 6;
    if (i) {
      hrg_pixel_width[0][i - 1] = hrg_pixel_x[0][i] - hrg_pixel_x[0][i - 1];
      hrg_pixel_width[1][i - 1] = hrg_pixel_x[1][i] - hrg_pixel_x[1][i - 1];
    }
  }
  for (i = 0; i <= 12; i++) {
    hrg_pixel_y[i] = cur_char_height * i / 12;
    if (i)
      hrg_pixel_height[i - 1] = hrg_pixel_y[i] - hrg_pixel_y[i - 1];
  }
  if (cur_char_width % 6 != 0 || cur_char_height % 12 != 0)
    debug("character size %d*%d not a multiple of 6*12 HRG raster\n",
        cur_char_width, cur_char_height);
}

/* Switch HRG on (1) or off (0). */
void
hrg_onoff(int enable)
{
  static int init = 0;

  if ((hrg_enable!=0) == (enable!=0)) return; /* State does not change. */

  if (!init) {
    hrg_init();
    init = 1;
  }
  hrg_enable = enable;
  trs_screen_refresh();
}

/* Write address to latch. */
void
hrg_write_addr(int addr, int mask)
{
  hrg_addr = (hrg_addr & ~mask) | (addr & mask);
}

/* Write byte to HRG memory. */
void
hrg_write_data(int data)
{
  int old_data;
  int position, line;
  int bits0, bits1;

  if (hrg_addr >= HRG_MEMSIZE) return; /* nonexistent address */
  old_data = hrg_screen[hrg_addr];
  hrg_screen[hrg_addr] = data;

  if (!hrg_enable) return;
  if ((currentmode & EXPANDED) && (hrg_addr & 1)) return;
  if ((data &= 0x3f) == (old_data &= 0x3f)) return;

  position = hrg_addr & 0x3ff; /* bits 0-9: "PRINT @" screen position */
  line = hrg_addr >> 10;       /* vertical offset inside character cell */
  bits0 = ~data & old_data;    /* pattern to clear */
  bits1 = data & ~old_data;    /* pattern to set */

  if (bits0 == 0
      || trs_screen[position] == 0x20
      || trs_screen[position] == 0x80
      /*|| (trs_screen[position] < 0x80 && line >= 8 && !usefont)*/
     ) {
    /* Only additional bits set, or blank text character.
       No need for update of text. */
    int const destx = (position % row_chars) * cur_char_width + left_margin;
    int const desty = (position / row_chars) * cur_char_height + top_margin
      + hrg_pixel_y[line];
    int const *x = hrg_pixel_x[(currentmode & EXPANDED) != 0];
    int const *w = hrg_pixel_width[(currentmode & EXPANDED) != 0];
    int const h = hrg_pixel_height[line];
    int n0 = 0;
    int n1 = 0;
    int flag = 0;
    int i, j, b;
    SDL_Rect rect0[3];    /* 6 bits => max. 3 groups of adjacent "0" bits */
    SDL_Rect rect1[3];

    /* Compute arrays of rectangles to clear and to set. */
    for (j = 0, b = 1; j < 6; j++, b <<= 1) {
      if (bits0 & b) {
        if (flag >= 0) {       /* Start new rectangle. */
          rect0[n0].x = destx + x[j];
          rect0[n0].y = desty;
          rect0[n0].w = w[j];
          rect0[n0].h = h;
          n0++;
          flag = -1;
        }
        else {                 /* Increase width of rectangle. */
          rect0[n0 - 1].w += w[j];
        }
      }
      else if (bits1 & b) {
        if (flag <= 0) {
          rect1[n1].x = destx + x[j];
          rect1[n1].y = desty;
          rect1[n1].w = w[j];
          rect1[n1].h = h;
          n1++;
          flag = 1;
        }
        else {
          rect1[n1 - 1].w += w[j];
        }
      }
      else {
        flag = 0;
      }
    }
    if (n0 != 0) {
      for (i = 0; i < n0; i++) {
        SDL_FillRect(screen, &rect0[i], background);
        addToDrawList(&rect0[i]);
      }
    }
    if (n1 != 0) {
      for (i = 0; i < n1; i++) {
        SDL_FillRect(screen, &rect1[i], foreground);
        addToDrawList(&rect1[i]);
      }
    }
  }
  else {
    /* Unfortunately, HRG1B combines text and graphics with an
       (inclusive) OR. Thus, in the general case, we cannot erase
       the old graphics byte without losing the text information.
       Call trs_screen_write_char to restore the text character
       (erasing the graphics). This function will in turn call
       hrg_update_char and restore 6*12 graphics pixels. Sigh. */
    trs_screen_write_char(position, trs_screen[position]);
  }
}

/* Read byte from HRG memory. */
int
hrg_read_data(void)
{
  if (hrg_addr >= HRG_MEMSIZE) return 0xff; /* nonexistent address */
  return hrg_screen[hrg_addr];
}

/* Update graphics at given screen position.
   Called by trs_screen_write_char. */
static void
hrg_update_char(int position)
{
  int const destx = (position % row_chars) * cur_char_width + left_margin;
  int const desty = (position / row_chars) * cur_char_height + top_margin;
  int const *x = hrg_pixel_x[(currentmode & EXPANDED) != 0];
  int const *w = hrg_pixel_width[(currentmode & EXPANDED) != 0];
  int byte;
  int prev_byte = 0;
  int n = 0;
  int np = 0;
  int i, j, flag;
  SDL_Rect rect[3 * 12];

  /* Compute array of rectangles. */
  for (i = 0; i < 12; i++) {
    if ((byte = hrg_screen[position + (i << 10)] & 0x3f) == 0) {
    }
    else if (byte != prev_byte) {
      np = n;
      flag = 0;
      for (j = 0; j < 6; j++) {
        if (!(byte & 1 << j)) {
          flag = 0;
        }
        else if (!flag) {     /* New rectangle. */
          rect[n].x = destx + x[j];
          rect[n].y = desty + hrg_pixel_y[i];
          rect[n].w = w[j];
          rect[n].h = hrg_pixel_height[i];
          n++;
          flag = 1;
        }
        else {                /* Increase width. */
          rect[n - 1].w += w[j];
        }
      }
    }
    else {                    /* Increase heights. */
      for (j = np; j < n; j++)
        rect[j].h += hrg_pixel_height[i];
    }
    prev_byte = byte;
  }
  if (n != 0) {
    for (i = 0; i < n; i++) {
      SDL_FillRect(screen, &rect[i], foreground);
      addToDrawList(&rect[i]);
    }
  }
}


void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons)
{
  int win_x, win_y;
  Uint8 const mask = SDL_GetMouseState(&win_x, &win_y);

#if MOUSEDEBUG
  debug("get_mouse %d %d 0x%x ->", win_x, win_y, mask);
#endif
  if (win_x >= 0 && win_x < OrigWidth &&
      win_y >= 0 && win_y < OrigHeight) {
    /* Mouse is within emulator window */
    if (win_x < left_margin) win_x = left_margin;
    if (win_x >= OrigWidth - left_margin) win_x = OrigWidth - left_margin - 1;
    if (win_y < top_margin) win_y = top_margin;
    if (win_y >= OrigHeight - top_margin) win_y = OrigHeight - top_margin - 1;
    *x = mouse_last_x = (win_x - left_margin)
      * mouse_x_size
      / (OrigWidth - 2 * left_margin);
    *y = mouse_last_y = (win_y - top_margin)
      * mouse_y_size
      / (OrigHeight - 2 * top_margin);
    mouse_last_buttons = 7;
    /* !!Note: assuming 3-button mouse */
    if (mask & SDL_BUTTON(SDL_BUTTON_LEFT))   mouse_last_buttons &= ~4;
    if (mask & SDL_BUTTON(SDL_BUTTON_MIDDLE)) mouse_last_buttons &= ~2;
    if (mask & SDL_BUTTON(SDL_BUTTON_RIGHT))  mouse_last_buttons &= ~1;
  }
  *x = mouse_last_x;
  *y = mouse_last_y;
  *buttons = mouse_last_buttons;
#if MOUSEDEBUG
  debug("%d %d 0x%x\n",
      mouse_last_x, mouse_last_y, mouse_last_buttons);
#endif
}

void trs_set_mouse_pos(int x, int y)
{
  if (x == mouse_last_x && y == mouse_last_y) {
    /* Kludge: Ignore warp if it says to move the mouse to where we
       last said it was. In general someone could really want to do that,
       but with MDRAW, gratuitous warps to the last location occur frequently.
    */
    return;
  } else {
    int const dest_x = left_margin + x * (OrigWidth - 2 * left_margin) / mouse_x_size;
    int const dest_y = top_margin  + y * (OrigHeight - 2 * top_margin) / mouse_y_size;

#if MOUSEDEBUG
    debug("set_mouse %d %d -> %d %d\n", x, y, dest_x, dest_y);
#endif
#ifdef SDL2
    SDL_WarpMouseInWindow(window, dest_x, dest_y);
#else
    SDL_WarpMouse(dest_x, dest_y);
#endif
  }
}

void trs_get_mouse_max(int *x, int *y, unsigned int *sens)
{
  *x = mouse_x_size - (mouse_old_style ? 0 : 1);
  *y = mouse_y_size - (mouse_old_style ? 0 : 1);
  *sens = mouse_sens;
}

void trs_set_mouse_max(int x, int y, unsigned int sens)
{
  if ((x & 1) == 0 && (y & 1) == 0) {
    /* "Old style" mouse drivers took the size here; new style take
       the maximum. As a heuristic kludge, we assume old style if
       the values are even, new style if not. */
    mouse_old_style = 1;
  }
  mouse_x_size = x + (mouse_old_style ? 0 : 1);
  mouse_y_size = y + (mouse_old_style ? 0 : 1);
  mouse_sens = sens;
}

int trs_get_mouse_type(void)
{
  /* !!Note: assuming 3-button mouse */
  return 1;
}

void trs_main_save(FILE *file)
{
  int i;

  trs_save_int(file, &trs_model, 1);
  trs_save_uchar(file, trs_screen, 2048);
  trs_save_int(file, &screen_chars, 1);
  trs_save_int(file, &col_chars, 1);
  trs_save_int(file, &row_chars, 1);
  trs_save_int(file, &currentmode, 1);
  trs_save_int(file, &text80x24, 1);
  trs_save_int(file, &screen640x240, 1);
  trs_save_int(file, &trs_charset, 1);
  trs_save_int(file, &trs_charset1, 1);
  trs_save_int(file, &trs_charset3, 1);
  trs_save_int(file, &trs_charset4, 1);
  for (i = 0; i < G_YSIZE; i++)
    trs_save_uchar(file, grafyx_unscaled[i], G_XSIZE);
  trs_save_uchar(file, &grafyx_x, 1);
  trs_save_uchar(file, &grafyx_y, 1);
  trs_save_uchar(file, &grafyx_enable, 1);
  trs_save_uchar(file, &grafyx_overlay, 1);
  trs_save_uchar(file, &grafyx_xoffset, 1);
  trs_save_uchar(file, &grafyx_yoffset, 1);
  trs_save_uchar(file, &grafyx_x, 1);
  trs_save_int(file, key_queue, KEY_QUEUE_SIZE);
  trs_save_int(file, &key_queue_head, 1);
  trs_save_int(file, &key_queue_entries, 1);
  trs_save_int(file, &lowe_le18, 1);
  trs_save_int(file, &lowercase, 1);
  trs_save_int(file, &stringy, 1);
}

void trs_main_load(FILE *file)
{
  int i;

  trs_load_int(file, &trs_model, 1);
  trs_load_uchar(file, trs_screen, 2048);
  trs_load_int(file, &screen_chars, 1);
  trs_load_int(file, &col_chars, 1);
  trs_load_int(file, &row_chars, 1);
  trs_load_int(file, &currentmode, 1);
  trs_load_int(file, &text80x24, 1);
  trs_load_int(file, &screen640x240, 1);
  trs_load_int(file, &trs_charset, 1);
  trs_load_int(file, &trs_charset1, 1);
  trs_load_int(file, &trs_charset3, 1);
  trs_load_int(file, &trs_charset4, 1);
  for (i = 0; i < G_YSIZE; i++)
    trs_load_uchar(file, grafyx_unscaled[i], G_XSIZE);
  trs_load_uchar(file, &grafyx_x, 1);
  trs_load_uchar(file, &grafyx_y, 1);
  trs_load_uchar(file, &grafyx_enable, 1);
  trs_load_uchar(file, &grafyx_overlay, 1);
  trs_load_uchar(file, &grafyx_xoffset, 1);
  trs_load_uchar(file, &grafyx_yoffset, 1);
  trs_load_uchar(file, &grafyx_x, 1);
  trs_load_int(file, key_queue, KEY_QUEUE_SIZE);
  trs_load_int(file, &key_queue_head, 1);
  trs_load_int(file, &key_queue_entries, 1);
  trs_load_int(file, &lowe_le18, 1);
  trs_load_int(file, &lowercase, 1);
  trs_load_int(file, &stringy, 1);
}

int trs_sdl_savebmp(const char *filename)
{
  if (SDL_SaveBMP(screen, filename) != 0) {
    error("failed to save Screenshot %s: %s", filename, strerror(errno));
    return -1;
  }
  return 0;
}
