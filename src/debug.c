/*
 * Copyright (C) 1992 Clarendon Hill Software.
 *
 * Permission is granted to any individual or institution to use, copy,
 * or redistribute this software, provided this copyright notice is retained.
 *
 * This software is provided "as is" without any expressed or implied
 * warranty.  If this software brings on any sort of damage -- physical,
 * monetary, emotional, or brain -- too bad.  You've got no one to blame
 * but yourself.
 *
 * The software may be modified for your own purposes, but modified versions
 * must retain this notice.
 */

/*
 * Copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef ZBX
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "error.h"
#include "trs.h"
#include "web_debugger.h"
#include "trs_xray_resources.h"

#define MAXLINE		(256)
#define ADDRESS_SPACE	(0x10000)
#define MAX_TRAPS	(100)

#define BREAKPOINT_FLAG		(0x1)
#define TRACE_FLAG		(0x2)
#define DISASSEMBLE_ON_FLAG	(0x4)
#define DISASSEMBLE_OFF_FLAG	(0x8)
#define BREAK_ONCE_FLAG		(0x10)
#define WATCHPOINT_FLAG		(0x20)

static Uint8 *traps;
static int num_traps;
static int print_instructions;
static int stop_signaled;
static unsigned int num_watchpoints = 0;

static struct
{
    int   valid;
    int   address;
    int   flag;
    Uint8 byte; /* used only by watchpoints */
} trap_table[MAX_TRAPS];


static void debug_run(int disable_continuous);

static void help_message(void)
{
    puts("(zbx) commands:\n\
\n\
Running:\n\
    r(un)\n\
        Hard reset the Z80 and devices and commence execution.\n\
    c(ont)\n\
        Continue execution.\n\
    s(tep)\n\
    s(tep)i(nt)\n\
        Execute one instruction, or if the instruction is repeating (such as\n\
        LDIR), execute only one iteration.  With \"step\", an interrupt is\n\
        not allowed to occur after the instruction; with \"stepint\", an\n\
        interrupt is allowed.\n\
    n(ext)\n\
    n(ext)i(nt)\n\
        Execute one instruction.  If the instruction is a CALL, continue\n\
        until the return.  If the instruction is repeating (such as LDIR),\n\
        continue until it finishes.  Interrupts are always allowed during\n\
        execution, but only \"nextint\" allows an interrupt afterwards.\n\
    re(set)\n\
        Hard reset the Z80 and devices.\n\
    s(oft)r(eset)\n\
        Press the system reset button.  On Model I/III, softreset resets the\n\
        devices and posts a nonmaskable interrupt to the CPU; on Model 4/4P,\n\
        softreset is the same as hard reset.\n\
Printing:\n\
    (dum)p\n\
        Print the values of the Z80 registers.\n\
    in <port>\n\
        Read the value of the given I/O port.\n\
    l(ist)\n\
    l(ist) <addr>\n\
    l(ist) <start addr> , <end addr>\n\
        Disassemble 10 instructions at the current pc, 10 instructions at\n\
        the specified hex address, or the instructions in the range of hex\n\
        addresses.\n\
    <start addr> , <end addr> /\n\
    <start addr> / <num bytes>\n\
    <addr> =\n\
        Print the memory values in the specified range.  All values are hex.\n\
    tr(ace)on\n\
        Enable tracing of all instructions.\n\
    tr(ace)off\n\
        Disable tracing.\n\
    d(isk)d(ump)\n\
        Print the state of the floppy disk controller emulation.\n\
Traps:\n\
    st(atus)\n\
        Show all traps (breakpoints, tracepoints, watchpoints).\n\
    cl(ear)\n\
        Delete the trap at the current address.\n\
    d(elete) <n>\n\
    d(elete) *\n\
        Delete trap n, or all traps.\n\
    stop at <address>\n\
    b(reak) <address>\n\
        Set a breakpoint at the specified hex address.\n\
    t(race) <address>\n\
        Set a trap to trace execution at the specified hex address.\n\
    traceon at <address>\n\
    tron <address>\n\
        Set a trap to enable tracing at the specified hex address.\n\
    traceoff at <address>\n\
    troff <address>\n\
        Set a trap to disable tracing at the specified hex address.\n\
    w(atch) <address>\n\
        Set a trap to watch specified hex address for changes.\n\
Miscellaneous:\n\
    a(ssign) $<reg> = <value>\n\
    a(ssign) I<port> = <value>\n\
    a(ssign) <addr> = <value>\n\
    set $<reg> = <value>\n\
    set I<port> = <value>\n\
    set <addr> = <value>\n\
        Change the value of a register, register pair, I/O or memory byte.\n\
    timeroff\n\
    timeron\n\
        Disable/enable the emulated TRS-80 real time clock interrupt.\n\
    diskdebug <hexval>\n\
        Set floppy disk controller debug flags to hexval.\n\
        1=FDC register I/O, 2=FDC commands, 4=VTOS 3.0 JV3 kludges, 8=Gaps,\n\
        10=Phys sector sizes, 20=Readadr timing, 40=DMK, 80=ioctl errors.\n\
    iodebug <hexval>\n\
        Set I/O port debug flags to hexval: 1=port input, 2=port output.\n\
    (zbx)i(nfo)\n\
        Display information about this debugger.\n\
    h(elp)\n\
    ?\n\
        Print this message.\n\
    q(uit)\n\
        Exit from xtrs.");
}

static char *trap_name(int flag)
{
    switch(flag)
    {
      case BREAKPOINT_FLAG:
	return "breakpoint";
      case TRACE_FLAG:
	return "trace";
      case DISASSEMBLE_ON_FLAG:
	return "traceon";
      case DISASSEMBLE_OFF_FLAG:
	return "traceoff";
      case BREAK_ONCE_FLAG:
	return "temporary breakpoint";
      case WATCHPOINT_FLAG:
	return "watchpoint";
      default:
	return "unknown trap";
    }
}

static void show_zbxinfo(void)
{
    puts("zbx: Z80 debugger by David Gingold, Alex Wolman, and Timothy"
         " Mann\n");
    printf("Traps set: %d (maximum %d)\n", num_traps, MAX_TRAPS);
    printf("Size of address space: 0x%x\n", ADDRESS_SPACE);
    printf("Maximum length of command line: %d\n", MAXLINE);
#ifdef READLINE
    puts("GNU Readline library support enabled.");
#else
    puts("GNU Readline library support disabled.");
#endif
}

static void clear_all_traps(void)
{
    int i;
    for(i = 0; i < MAX_TRAPS; ++i)
    {
	if(trap_table[i].valid)
	{
	    traps[trap_table[i].address] &= ~(trap_table[i].flag);
	    trap_table[i].valid = 0;
	}
    }
    num_traps = 0;
    num_watchpoints = 0;
}

static void print_traps(void)
{
    int i;

    if(num_traps)
    {
	for(i = 0; i < MAX_TRAPS; ++i)
	{
	    if(trap_table[i].valid)
	    {
		printf("[%d] %.4x (%s)\n", i, trap_table[i].address,
		       trap_name(trap_table[i].flag));
	    }
	}
    }
    else
    {
	puts("No traps are set.");
    }
}

static void set_trap(int address, int flag)
{
    int i;

    if(num_traps == MAX_TRAPS)
    {
	printf("Cannot set more than %d traps.\n", MAX_TRAPS);
    }
    else
    {
	i = 0;
	while(trap_table[i].valid) ++i;

	trap_table[i].valid = 1;
	trap_table[i].address = address;
	trap_table[i].flag = flag;
	if (trap_table[i].flag == WATCHPOINT_FLAG) {
	    /* Initialize the byte field to current memory contents. */
	    trap_table[i].byte = mem_read(address);
	    /* Increment number of set watchpoints. */
	    num_watchpoints++;
	}
	traps[address] |= flag;
	num_traps++;

	printf("Set %s [%d] at %.4x\n", trap_name(flag), i, address);
    }
}

static void clear_trap(int i)
{
    if((i < 0) || (i > MAX_TRAPS) || !trap_table[i].valid)
    {
	printf("[%d] is not a valid trap.\n", i);
    }
    else
    {
	traps[trap_table[i].address] &= ~(trap_table[i].flag);
	trap_table[i].valid = 0;
	if (trap_table[i].flag == WATCHPOINT_FLAG) {
	    /* Decrement number of set watchpoints. */
	    num_watchpoints--;
	}
	num_traps--;
	printf("Cleared %s [%d] at %.4x\n",
	       trap_name(trap_table[i].flag), i, trap_table[i].address);
    }
}

static void clear_trap_address(int address, int flag)
{
    int i;
    for(i = 0; i < MAX_TRAPS; ++i)
    {
	if(trap_table[i].valid && (trap_table[i].address == address)
	   && ((flag == 0) || (trap_table[i].flag == flag)))
	{
	    clear_trap(i);
	}
    }
}

static void debug_print_registers(void)
{
    puts("\n       S Z - H - PV N C   IFF1 IFF2 IM");
    printf("Flags: %d %d %d %d %d  %d %d %d     %d    %d   %d\n\n",
	   (SIGN_FLAG != 0),
	   (ZERO_FLAG != 0),
	   (Z80_F & UNDOC5_MASK) != 0,
	   (HALF_CARRY_FLAG != 0),
	   (Z80_F & UNDOC3_MASK) != 0,
	   (OVERFLOW_FLAG != 0),
	   (SUBTRACT_FLAG != 0),
	   (CARRY_FLAG != 0),
	   z80_state.iff1, z80_state.iff2, z80_state.interrupt_mode);

    printf("A F: %.2x %.2x    IX: %.4x    AF': %.4x\n",
	   Z80_A, Z80_F, Z80_IX, Z80_AF_PRIME);
    printf("B C: %.2x %.2x    IY: %.4x    BC': %.4x\n",
	   Z80_B, Z80_C, Z80_IY, Z80_BC_PRIME);
    printf("D E: %.2x %.2x    PC: %.4x    DE': %.4x\n",
	   Z80_D, Z80_E, Z80_PC, Z80_DE_PRIME);
    printf("H L: %.2x %.2x    SP: %.4x    HL': %.4x\n",
	   Z80_H, Z80_L, Z80_SP, Z80_HL_PRIME);
    printf("I R: %.2x %.2x\n", Z80_I, Z80_R7 | (Z80_R & 0x7f));

    printf("\nT-state counter: %" TSTATE_T_LEN "", z80_state.t_count);
    printf("\nZ80 Clock Speed: %.2f MHz\n", z80_state.clockMHz);
}

void soft_reset() {
	puts("Pressing reset button.");
	trs_reset(0);
}

void hard_reset() {
	puts("Performing hard reset and running.");
	trs_reset(1);
	debug_run(0);
}

void run_emulation() {
	// Forces non-continuous run, while handling traps.
	debug_run(/* disable_continuous= */ 1);
}

void halt_emulation() {
	puts("Halting Emulation");
	set_trap(Z80_PC, BREAK_ONCE_FLAG);
}

void on_trx_control_callback(TRX_CONTROL_TYPE type) {
	if (type == TRX_CONTROL_TYPE_STEP) z80_run(-1);
	else if (type == TRX_CONTROL_TYPE_CONTINUE) run_emulation();
	else if (type == TRX_CONTROL_TYPE_HALT) halt_emulation();
	else if (type == TRX_CONTROL_TYPE_SOFT_RESET) soft_reset();
	else if (type == TRX_CONTROL_TYPE_HARD_RESET) hard_reset();
}

void on_trx_add_breakpoint(int bp_id, uint16_t addr, TRX_BREAK_TYPE type) {
  int flag = BREAKPOINT_FLAG;  // TRX_BREAK_PC
  if (type == TRX_BREAK_MEMORY) flag = WATCHPOINT_FLAG;
  set_trap(addr, flag);
}

void on_trx_remove_breakpoint(int bp_id) {
  clear_trap(bp_id);
}

char* on_trx_get_resource(TRX_RESOURCE_TYPE type) {
	switch(type) {
		case TRX_RES_MAIN_HTML:
		  return trs_xray_html;
		case TRX_RES_MAIN_JS:
		  return trs_xray_js;
		case TRX_RES_MAIN_CSS:
		  return trs_xray_css;
		case TRX_RES_TRS_FONT:
		  // FIXME
		  return trs_xray_html;
		case TRX_RES_JQUERY:
		  // FIXME
		  return trs_xray_html;
		default:
		  printf("ERROR: Unknown resource type.");
		  return trs_xray_html;
	}
}

void on_trx_get_state_update(TRX_SystemState* state) {
  state->registers.pc = Z80_PC;
  state->registers.sp = Z80_SP;
  state->registers.af = Z80_AF;
  state->registers.bc = Z80_BC;
  state->registers.de = Z80_DE;
  state->registers.hl = Z80_HL;
  state->registers.af_prime = Z80_AF_PRIME;
  state->registers.bc_prime = Z80_BC_PRIME;
  state->registers.de_prime = Z80_DE_PRIME;
  state->registers.hl_prime = Z80_HL_PRIME;
  state->registers.ix = Z80_IX;
  state->registers.iy = Z80_IY;
  state->registers.i = Z80_I;
  state->registers.r7 = Z80_R7;
  state->registers.r = Z80_R;
  state->registers.t_count = z80_state.t_count;
  state->registers.clock_mhz = z80_state.clockMHz;
  state->registers.iff1 = z80_state.iff1;
  state->registers.iff2 = z80_state.iff2;
  state->registers.interrupt_mode = z80_state.interrupt_mode;
}

uint8_t trx_read_memory(uint16_t addr) {
  return (uint8_t)mem_read(addr);
}

void trs_debug(void)
{
    stop_signaled = 1;
    if (trs_continuous > 0) trs_continuous = 0;
}

void debug_init(void)
{
    int i;

    traps = (Uint8 *) malloc(ADDRESS_SPACE * sizeof(Uint8));
    if (traps == NULL)
      fatal("debug_init: failed to allocate traps");

    memset(traps, 0, ADDRESS_SPACE * sizeof(Uint8));

    for(i = 0; i < MAX_TRAPS; ++i) trap_table[i].valid = 0;

    puts("Type \"h(elp)\" for a list of commands.");

		// FIXME: Add a flag to choose between CLI debugger UI and TRX.
    static TRX_Context ctx;

		ctx.system_name = "sdlTRS";
		ctx.model = trs_model;
		ctx.rom_version = 0;

		ctx.capabilities.memory_range.start = 0;
		ctx.capabilities.memory_range.length = 0xFFFF;
    ctx.control_callback = &on_trx_control_callback;
    ctx.read_memory = &trx_read_memory;
		ctx.breakpoint_callback = &on_trx_add_breakpoint;
		ctx.remove_breakpoint_callback = &on_trx_remove_breakpoint;
		ctx.get_resource = &on_trx_get_resource;
		ctx.get_state_update = &on_trx_get_state_update;
		init_trs_xray(&ctx);
}

static void print_memory(Uint16 address, int num_bytes)
{
    int bytes_to_print, i;
    int byte;

    while(num_bytes > 0)
    {
	bytes_to_print = 16;
	if(bytes_to_print > num_bytes) bytes_to_print = num_bytes;

	printf("%.4x:\t", address);
	for(i = 0; i < bytes_to_print; ++i)
	{
	    printf("%.2x ", mem_read(address + i));
	}
	for(i = bytes_to_print; i < 16; ++i)
	{
	    printf("   ");
	}
	printf("    ");
	for(i = 0; i < bytes_to_print; ++i)
	{
	    byte = mem_read(address + i);
	    if(isprint(byte))
	    {
		putchar(byte);
	    }
	    else
	    {
		putchar('.');
	    }
	}
	putchar('\n');
	num_bytes -= bytes_to_print;
	address += bytes_to_print;
    }
}

static void debug_run(int disable_continuous)
{
    Uint8 t;
    Uint8 byte;
    int continuous;
    int i;
    int watch_triggered = 0;

    stop_signaled = 0;

    t = traps[Z80_PC];
    while(!stop_signaled)
    {
	if(t)
	{
	    if(t & TRACE_FLAG)
	    {
		printf("Trace: ");
		disassemble(Z80_PC);
	    }
	    if(t & DISASSEMBLE_ON_FLAG)
	    {
		print_instructions = 1;
	    }
	    if(t & DISASSEMBLE_OFF_FLAG)
	    {
		print_instructions = 0;
	    }
	}

	if(print_instructions) disassemble(Z80_PC);

	continuous = !disable_continuous && (!print_instructions && num_traps == 0);
	if (z80_run(continuous)) {
	  puts("emt_debug instruction executed.");
	  stop_signaled = 1;
	}

	// printf("Step: %.4x\n", Z80_PC);

	t = traps[Z80_PC];
	if(t & BREAKPOINT_FLAG)
	{
	    stop_signaled = 1;
	}
	if(t & BREAK_ONCE_FLAG)
	{
	    stop_signaled = 1;
	    clear_trap_address(Z80_PC, BREAK_ONCE_FLAG);
	}

	/*
	 * Iterate over the trap list looking for watchpoints only if we
	 * know there are any to be found.
	 */
	if (num_watchpoints)
	{
	    for (i = 0; i < MAX_TRAPS; ++i)
	    {
		if (trap_table[i].valid &&
		    trap_table[i].flag == WATCHPOINT_FLAG)
		{
		    byte = mem_read(trap_table[i].address);
		    if (byte != trap_table[i].byte)
		    {
			/*
			 * If a watched memory location has changed, report
			 * it, update the watch entry in the trap table to
			 * reflect the new value, and set the
			 * watch_triggered flag so that we stop after all
			 * watchpoints have been processed.
			 */
			printf("Memory location 0x%.4x changed value from "
			       "0x%.2x to 0x%.2x.\n", trap_table[i].address,
			       trap_table[i].byte, byte);
			trap_table[i].byte = byte;
			watch_triggered = 1;
		    }
		}
	    }
	    if (watch_triggered)
	    {
		stop_signaled = 1;
	    }
	}

    }
    printf("Stopped at %.4x\n", Z80_PC);
}

void debug_shell(void)
{
    // FIXME: Add a flag to enable TRX support
		if (true) {
			puts("TRS X-ray is enabled, not enabling CLI prompt.");
	    trx_waitForExit();
			return;
    }

    char input[MAXLINE];
    char command[MAXLINE];
    int done = 0;

#ifdef READLINE
    char *line;
    char history_file[MAXLINE];
    char *home = (char *)getenv ("HOME");
    if (!home) home = ".";
    snprintf(history_file, MAXLINE - 1, "%s/.zbx-history", home);
    read_history(history_file);
#endif

// FIXME: Disable this with a flag.
//     while(!done)
//     {

// 	putchar('\n');
// 	disassemble(Z80_PC);

// #ifdef READLINE
// 	/*
// 	 * Use the way cool gnu readline() utility.  Get completion,
// 	 * history, way way cool.
//          */
//         {

// 	    line = readline("(zbx) ");
// 	    if(line)
// 	    {
// 		if(strlen(line) > 0)
// 		{
// 		    add_history(line);
// 		}
// 		strncpy(input, line, MAXLINE - 1);
// 		free(line);
// 	    }
// 	    else
// 	    {
// 		break;
// 	    }
// 	}
// #else
// 	printf("(zbx) ");  fflush(stdout);

// 	if (fgets(input, MAXLINE, stdin) == NULL) break;
// #endif

// 	if(sscanf(input, "%s", command))
// 	{
// 	    if(!strcmp(command, "help") || !strcmp(command, "?") ||
// 	       !strcmp(command, "h"))
// 	    {
// 		help_message();
// 	    }
// 	    else if (!strcmp(command, "zbxinfo") || !strcmp(command, "i"))
// 	    {
// 		show_zbxinfo();
// 	    }
// 	    else if(!strcmp(command, "clear") || !strcmp(command, "cl"))
// 	    {
// 		clear_trap_address(Z80_PC, 0);
// 	    }
// 	    else if(!strcmp(command, "cont") || !strcmp(command, "c"))
// 	    {
// 		debug_run(0);
// 	    }
// 	    else if(!strcmp(command, "dump") || !strcmp(command, "p"))
// 	    {
// 		debug_print_registers();
// 	    }
// 	    else if(!strcmp(command, "delete") || !strcmp(command, "d"))
// 	    {
// 		int i;

// 		if(!strncmp(input, "delete *", 8) || !strncmp(input, "d *", 3))
// 		{
// 		    clear_all_traps();
// 		}
// 		else if(sscanf(input, "%*s %d", &i) != 1)
// 		{
// 		    puts("A trap must be specified.");
// 		}
// 		else
// 		{
// 		    clear_trap(i);
// 		}
// 	    }
// 	    else if(!strcmp(command, "list") || !strcmp(command, "l"))
// 	    {
// 		unsigned int x, y;
// 		Uint16 start, old_start;
// 		int bytes = 0;
// 		int lines = 0;

// 		if(sscanf(input, "%*s %x , %x", &x, &y) == 2)
// 		{
// 		    start = x;
// 		    bytes = (y - x) & 0xffff;
// 		}
// 		else if(sscanf(input, "%*s %x", &x) == 1)
// 		{
// 		    start = x;
// 		    lines = 10;
// 		}
// 		else
// 		{
// 		    start = Z80_PC;
// 		    lines = 10;
// 		}

// 		if(lines)
// 		{
// 		    while(lines--)
// 		    {
// 			start = disassemble(start);
// 		    }
// 		}
// 		else
// 		{
// 		    while (bytes >= 0) {
// 			start = disassemble(old_start = start);
// 			bytes -= (start - old_start) & 0xffff;
// 		    }
// 		}
// 	    }
// 	    else if(!strcmp(command, "in"))
// 	    {
// 		unsigned int port;

// 		if(sscanf(input, "in %x", &port) == 1)
// 			printf("in %x = %x\n", port, z80_in(port));
// 		else
// 			puts("A port must be specified.");
// 	    }
// 	    else if(!strcmp(command, "next") || !strcmp(command, "nextint") ||
// 		    !strcmp(command, "n") || !strcmp(command, "ni"))
// 	    {
// 		int is_call = 0, is_rst = 0, is_rep = 0;
// 		switch(mem_read(Z80_PC)) {
// 		  case 0xCD:	/* call address */
// 		    is_call = 1;
// 		    break;
// 		  case 0xC4:	/* call nz, address */
// 		    is_call = !ZERO_FLAG;
// 		    break;
// 		  case 0xCC:	/* call z, address */
// 		    is_call = ZERO_FLAG;
// 		    break;
// 		  case 0xD4:	/* call nc, address */
// 		    is_call = !CARRY_FLAG;
// 		    break;
// 		  case 0xDC:	/* call c, address */
// 		    is_call = CARRY_FLAG;
// 		    break;
// 		  case 0xE4:	/* call po, address */
// 		    is_call = !PARITY_FLAG;
// 		    break;
// 		  case 0xEC:	/* call pe, address */
// 		    is_call = PARITY_FLAG;
// 		    break;
// 		  case 0xF4:	/* call p, address */
// 		    is_call = !SIGN_FLAG;
// 		    break;
// 		  case 0xFC:	/* call m, address */
// 		    is_call = SIGN_FLAG;
// 		    break;
// 		  case 0xC7:
// 		  case 0xCF:
// 		  case 0xD7:
// 		  case 0xDF:
// 		  case 0xE7:
// 		  case 0xEF:
// 		  case 0xF7:
// 		  case 0xFF:
// 		    is_rst = 1;
// 		    break;
// 		  case 0xED:
// 		    switch(mem_read(Z80_PC + 1)) {
// 		      case 0xB0: /* ldir */
// 		      case 0xB8: /* lddr */
// 		      case 0xB1: /* cpir */
// 		      case 0xB9: /* cpdr */
// 		      case 0xB2: /* inir */
// 		      case 0xBA: /* indr */
// 		      case 0xB3: /* otir */
// 		      case 0xBB: /* otdr */
// 		        is_rep = 1;
// 		        break;
// 		      default:
// 		        break;
// 		    }
// 		  default:
// 		    break;
// 		}
// 		if (is_call) {
// 		    set_trap((Z80_PC + 3) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
// 		    debug_run(0);
// 		} else if (is_rst) {
// 		    set_trap((Z80_PC + 1) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
// 		    debug_run(0);
// 		} else if (is_rep) {
// 		    set_trap((Z80_PC + 2) % ADDRESS_SPACE, BREAK_ONCE_FLAG);
// 		    debug_run(0);
// 		} else {
// 		    z80_run((!strcmp(command, "nextint") || !strcmp(command, "ni")) ? 0 : -1);
// 		}
// 	    }
// 	    else if(!strcmp(command, "quit") || !strcmp(command, "q"))
// 	    {
// 		done = 1;
// 	    }
// 	    else if(!strcmp(command, "reset") || !strcmp(command, "re"))
// 	    {
// 		puts("Performing hard reset.");
// 		trs_reset(1);
// 	    }
// 	    else if(!strcmp(command, "softreset") || !strcmp(command, "sr"))
// 	    {
// 				soft_reset();
// 	    }
// 	    else if(!strcmp(command, "run") || !strcmp(command, "r"))
// 	    {
// 				hard_reset();
// 	    }
// 	    else if(!strcmp(command, "status") || !strcmp(command, "st"))
// 	    {
// 		print_traps();
// 	    }
// 	    else if(!strcmp(command, "set") || !strcmp(command, "assign") ||
// 		    !strcmp(command, "a"))
// 	    {
// 		char regname[MAXLINE];
// 		unsigned int addr, value;

// 		if(sscanf(input, "%*s $%[a-zA-Z] = %x", regname, &value) == 2)
// 		{
// 		    if(!strcasecmp(regname, "a")) {
// 			Z80_A = value;
// 		    } else if(!strcasecmp(regname, "f")) {
// 			Z80_F = value;
// 		    } else if(!strcasecmp(regname, "b")) {
// 			Z80_B = value;
// 		    } else if(!strcasecmp(regname, "c")) {
// 			Z80_C = value;
// 		    } else if(!strcasecmp(regname, "d")) {
// 			Z80_D = value;
// 		    } else if(!strcasecmp(regname, "e")) {
// 			Z80_E = value;
// 		    } else if(!strcasecmp(regname, "h")) {
// 			Z80_H = value;
// 		    } else if(!strcasecmp(regname, "l")) {
// 			Z80_L = value;
// 		    } else if(!strcasecmp(regname, "sp")) {
// 			Z80_SP = value;
// 		    } else if(!strcasecmp(regname, "pc")) {
// 			Z80_PC = value;
// 		    } else if(!strcasecmp(regname, "af")) {
// 			Z80_AF = value;
// 		    } else if(!strcasecmp(regname, "bc")) {
// 			Z80_BC = value;
// 		    } else if(!strcasecmp(regname, "de")) {
// 			Z80_DE = value;
// 		    } else if(!strcasecmp(regname, "hl")) {
// 			Z80_HL = value;
// 		    } else if(!strcasecmp(regname, "af'")) {
// 			Z80_AF_PRIME = value;
// 		    } else if(!strcasecmp(regname, "bc'")) {
// 			Z80_BC_PRIME = value;
// 		    } else if(!strcasecmp(regname, "de'")) {
// 			Z80_DE_PRIME = value;
// 		    } else if(!strcasecmp(regname, "hl'")) {
// 			Z80_HL_PRIME = value;
// 		    } else if(!strcasecmp(regname, "ix")) {
// 			Z80_IX = value;
// 		    } else if(!strcasecmp(regname, "iy")) {
// 			Z80_IY = value;
// 		    } else if(!strcasecmp(regname, "i")) {
// 			Z80_I = value;
// 		    } else if(!strcasecmp(regname, "r")) {
// 			Z80_R = value;
// 			Z80_R7 = value & 0x80;
// 		    } else {
// 			printf("Unrecognized register name %s.\n", regname);
// 		    }
// 		}
// 		else if(sscanf(input, "%*s I%x = %x", &addr, &value) == 2)
// 		{
// 		    z80_out(addr, value);
// 		}
// 		else if(sscanf(input, "%*s %x = %x", &addr, &value) == 2)
// 		{
// 		    mem_write(addr, value);
// 		}
// 		else
// 		{
// 		    puts("Syntax error.  (Type \"h(elp)\" for commands.)");
// 		}
// 	    }
// 	    else if(!strcmp(command, "step") || !strcmp(command, "s"))
// 	    {
// 		z80_run(-1);`
// 	    }
// 	    else if(!strcmp(command, "stepint") || !strcmp(command, "si"))
// 	    {
// 		z80_run(0);
// 	    }
// 	    else if(!strcmp(command, "stop") || !strcmp(command, "break") ||
// 		    !strcmp(command, "b"))
// 	    {
// 		unsigned int address;

// 		if(sscanf(input, "stop at %x", &address) != 1 &&
// 		   sscanf(input, "%*s %x", &address) != 1)
// 		{
// 		    address = Z80_PC;
// 		}
// 		address %= ADDRESS_SPACE;
// 		set_trap(address, BREAKPOINT_FLAG);
// 	    }
// 	    else if(!strcmp(command, "trace") || !strcmp(command, "t"))
// 	    {
// 		unsigned int address;

// 		if(sscanf(input, "%*s %x", &address) != 1)
// 		{
// 		    address = Z80_PC;
// 		}
// 		address %= ADDRESS_SPACE;
// 		set_trap(address, TRACE_FLAG);
// 	    }
// 	    else if(!strcmp(command, "traceon") || !strcmp(command, "tron"))
// 	    {
// 		unsigned int address;

// 		if(sscanf(input, "traceon at %x", &address) == 1 ||
// 		   sscanf(input, "tron %x", &address) == 1)
// 		{
// 		    set_trap(address, DISASSEMBLE_ON_FLAG);
// 		}
// 		else
// 		{
// 		    print_instructions = 1;
// 		    puts("Tracing enabled.");
// 		}
// 	    }
// 	    else if(!strcmp(command, "traceoff") || !strcmp(command, "troff"))
// 	    {
// 		unsigned int address;

// 		if(sscanf(input, "traceoff at %x", &address) == 1 ||
// 		   sscanf(input, "troff %x", &address) == 1)
// 		{
// 		    set_trap(address, DISASSEMBLE_OFF_FLAG);
// 		}
// 		else
// 		{
// 		    print_instructions = 0;
// 		    puts("Tracing disabled.");
// 		}
// 	    }
// 	    else if(!strcmp(command, "watch") || !strcmp(command, "w"))
// 	    {
// 		unsigned int address;

// 		if(sscanf(input, "%*s %x", &address) == 1)
// 		{
// 		    address %= ADDRESS_SPACE;
// 		    set_trap(address, WATCHPOINT_FLAG);
// 		}
// 	    }
// 	    else if(!strcmp(command, "timeroff"))
// 	    {
// 	        /* Turn off emulated real time clock interrupt */
// 	        trs_timer_off();
//             }
// 	    else if(!strcmp(command, "timeron"))
// 	    {
// 	        /* Turn off emulated real time clock interrupt */
// 	        trs_timer_on();
//             }
// 	    else if(!strcmp(command, "diskdump") || !strcmp(command, "dd"))
// 	    {
// 		trs_disk_debug();
// 	    }
// 	    else if(!strcmp(command, "diskdebug"))
// 	    {
// 		trs_disk_debug_flags = 0;
// 		sscanf(input, "diskdebug %x", (unsigned int *)&trs_disk_debug_flags);
// 	    }
// 	    else if(!strcmp(command, "iodebug"))
// 	    {
// 		trs_io_debug_flags = 0;
// 		sscanf(input, "iodebug %x", (unsigned int *)&trs_io_debug_flags);
// 	    }
// 	    else
// 	    {
// 		unsigned int start_address, end_address, num_bytes;

// 		if(sscanf(input, "%x , %x / ", &start_address, &end_address) == 2)
// 		{
// 		    print_memory(start_address, end_address - start_address);
// 		}
// 		else if(sscanf(input, "%x / %x ", &start_address, &num_bytes) == 2)
// 		{
// 		    print_memory(start_address, num_bytes);
// 		}
// 		else if(sscanf(input, "%x = ", &start_address) == 1)
// 		{
// 		    print_memory(start_address, 1);
// 		}
// 		else
// 		{
// 		    puts("Syntax error.  (Type \"h(elp)\" for commands.)");
// 		}
// 	    }
// 	}
//     }
#ifdef READLINE
    write_history(history_file);
#endif
}
#endif

