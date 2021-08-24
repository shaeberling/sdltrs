#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint16_t af;
  uint16_t bc;
  uint16_t de;
  uint16_t hl;
  uint16_t af_prime;
  uint16_t bc_prime;
  uint16_t de_prime;
  uint16_t hl_prime;

  uint16_t pc;
  uint16_t sp;
  uint16_t ix;
  uint16_t iy;

  uint16_t i;
  uint16_t iff1;
  uint16_t iff2;
} TRX_StatusRegistersAndFlags;

typedef enum {
  TRX_SCREEN_NORMAL = 0,
  TRX_SCREEN_EXPANDED = 1,
  TRX_SCREEN_ALTERNATE = 2,
  TRX_SCREEN_INVERTED = 3
} TRX_ScreenMode;

typedef struct {
  bool paused;
  TRX_StatusRegistersAndFlags registers;
  TRX_ScreenMode screen_mode;
} TRX_SystemState;

typedef enum {
  UNDEFINED = 0,
  MODEL_I = 1,
  MODEL_II = 2,
  MODEL_III = 3,
  MODEL_IV = 4,
  MODEL_IV_P = 5,
} TRX_ModelType;

typedef struct {
  uint32_t start;
  uint32_t length;
} TRX_MemoryRange;

typedef struct {
  bool control_step;
  bool control_step_over;
  bool control_continue;
  bool control_pause;
  bool pc_breakpoints;
  bool memory_breakpoints;
  bool io_breakpoints;
  TRX_MemoryRange memory_range;
  // TOOD: Supported Memory Segment. (A single segment should suffice)
  //       Model 1 demo will only work on address 32k and up.
  //       (For read and write)
} TRX_Capabilities;

typedef void (*TRX_UpdateState)(const TRX_SystemState* state);

typedef enum {
  TRX_CONTROL_TYPE_STEP = 0,
  TRX_CONTROL_TYPE_STEP_OVER = 1,
  TRX_CONTROL_TYPE_CONTINUE = 2,
  TRX_CONTROL_TYPE_HALT = 3,
  TRX_CONTROL_TYPE_PAUSE = 4,
  TRX_CONTROL_TYPE_SOFT_RESET = 5,
  TRX_CONTROL_TYPE_HARD_RESET = 6
} TRX_CONTROL_TYPE;
typedef void (*TRX_ControlCallback)(TRX_CONTROL_TYPE type);

typedef enum {
  TRX_BREAK_PC = 0,
  TRX_BREAK_MEMORY = 1,
  TRX_BREAK_IO = 2
} TRX_BREAK_TYPE;
typedef void (*TRX_SetBreakPointCallback)(int bp_id, uint8_t addr, TRX_BREAK_TYPE type);
typedef void (*TRX_RemoveBreakPointCallback)(int bp_id);

typedef struct {
  TRX_MemoryRange range;
  uint8_t* data;
} TRX_MemorySegment;
typedef void (*TRX_GetMemorySegment)(int start, int length, TRX_MemorySegment* segment);
typedef void (*TRX_SetMemorySegment)(TRX_MemorySegment* segment);

typedef struct {
  // Descriptive name of the system under test (SUT).
  char* system_name;

  // TRS model type.
  TRX_ModelType model;

  // Original = 0 or 1.
  uint8_t rom_version;

  // Lets the frontend know about the SUT's capabilities.
  TRX_Capabilities capabilities;

  // Callbacks invoked from the frontend.
  TRX_ControlCallback control_callback;
  TRX_SetBreakPointCallback breakpoint_callback;
  TRX_RemoveBreakPointCallback remove_breakpoint_callback;
  TRX_GetMemorySegment get_memory_segment;
  TRX_SetMemorySegment set_memory_segment;

  // Tells the frontend about state changes of the SUT.
  const TRX_UpdateState update_state;

} TRX_Context;

// Initialize the web debugger.
bool init_trs_xray(TRX_Context* ctx);

// Initialize the web debugger.
void trx_waitForExit();

// Initialize the web debugger.
void trx_shutdown();

// Questions:
// - When are the different opcodes being used? http://z80-heaven.wikidot.com/opcode-reference-chart
//   - minor: CB, CC, ED, FD, DDCB, FDCB etc.
// - To know how to get N lines of code, one would need to know the width of the opcodes/arguments.
//  - Could just ask for memory blocks