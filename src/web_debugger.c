#include "web_debugger.h"
#include "web_debugger_resources.h"

#include <stdlib.h>
#include <stdbool.h>

#include "cJSON.h"
#include "mongoose.h"
#include "SDL.h"
#include "trs.h"

static struct mg_mgr www_mgr;
static SDL_Thread* thread;
static SDL_Thread* emu_run_thread;
static struct mg_connection* status_conn = NULL;
static TRX_Context* ctx = NULL;
static bool trx_running = true;

static bool init_webserver(void);
static char* get_registers_json(void);

static void handleDynamicUpdate();
static int emu_run_looper(void *ptr);
static bool emulation_running = false;
static int emulation_is_halting = false;
static uint32_t last_update_sent = 0;

typedef struct {
  uint16_t address;
  TRX_BREAK_TYPE type;
  bool enabled;
} TRX_Breakpoint;

#define MAX_BREAKPOINTS 128
static TRX_Breakpoint breakpoints[MAX_BREAKPOINTS];

static TRX_CONTROL_TYPE next_async_action = TRX_CONTROL_TYPE_NOOP;

// public
bool init_trs_xray(TRX_Context* ctx_param) {
	if (!init_webserver()) {
    puts("[TRX] ERROR: Aborting initialization.");
    return false;
  }
  last_update_sent = clock();
  ctx = ctx_param;

  for (int id = 0; id < MAX_BREAKPOINTS; ++id) {
    breakpoints[id].address = 0;
    breakpoints[id].type = 0;
    breakpoints[id].enabled = false;
  }

  emu_run_thread =
      SDL_CreateThread(emu_run_looper, "TRX Emu Run Thread", (void *)NULL);
  return true;
}

// public
void trx_waitForExit() {
  int threadReturnValue;
  SDL_WaitThread(thread, &threadReturnValue);
}

// public
void trx_shutdown() {
  puts("[TRX] Shutting down Web debugger");
  trx_running = false;
}

static void send_update_to_web_debugger() {
  if (status_conn == NULL) return;

  // Send registers.
  char* message = get_registers_json();
  mg_ws_send(status_conn, message, strlen(message), WEBSOCKET_OP_TEXT);
  free(message);
}

// Params: [start]/[length], e.g. "0/65536"
static void send_memory_segment(const char* params) {
  if (status_conn == NULL) return;
  // printf("TRX: MemorySegment request: '%s'.", params);
  // FIXME: Parse and use parameters!
  TRX_MemorySegment segment;
  ctx->get_memory_segment(0, 0xFFFF, &segment);
  // Send registers.
  mg_ws_send(status_conn, (const char*)segment.data, segment.range.length, WEBSOCKET_OP_BINARY);
}

// Params: [address in decimal]. e.g. "1254"
static void add_breakpoint(const char* params, TRX_BREAK_TYPE type) {
  int addr = atoi(params);
  if (addr == 0 && strcmp("0", params) != 0) {
    puts("[TRX] Error: Cannot parse address.");
    return;
  }

  int id = 0;
  for (id = 0; id < MAX_BREAKPOINTS; ++id) {
    if (!breakpoints[id].enabled) break;
  }
  breakpoints[id].address = addr;
  breakpoints[id].type = type;
  breakpoints[id].enabled = true;
  ctx->breakpoint_callback(id, addr, type);
  send_update_to_web_debugger();
}

static void remove_breakpoint(const char* params) {
  int id = atoi(params);
  if (id == 0 && strcmp("0", params) != 0) {
    puts("[TRX] Error: Cannot parse breakpoint ID.");
    return;
  }
  if (id >= MAX_BREAKPOINTS) {
    puts("[TRX] Error: Breakpoint ID too large.");
    return;
  }
  breakpoints[id].enabled = false;
  ctx->remove_breakpoint_callback(id);
  send_update_to_web_debugger();
}

static void handle_http_request(struct mg_connection *conn,
                                struct mg_http_message* message) {
  if (mg_http_match_uri(message, "/") || mg_http_match_uri(message, "/index.html")) {
    mg_http_reply(conn, 200, "Content-Type: text/html\r\nConnection: close\r\n", web_debugger_html);
  } else if (mg_http_match_uri(message, "/web_debugger.js")) {
    mg_http_reply(conn, 200, "Content-Type: application/javascript\r\nConnection: close\r\n", web_debugger_js);
  } else if (mg_http_match_uri(message, "/web_debugger.css")) {
    mg_http_reply(conn, 200, "Content-Type: text/css\r\nConnection: close\r\n", web_debugger_css);
  } else if (mg_http_match_uri(message, "/channel")) {
		mg_ws_upgrade(conn, message, NULL);
		status_conn = conn;
  } else {
    // Resource not found.
    mg_http_reply(conn, 404, "Content-Type: text/html\r\nConnection: close\r\n", "");
  }
}

static char* get_registers_json(void) {
	  cJSON* json = cJSON_CreateObject();

    cJSON* context = cJSON_CreateObject();
    cJSON_AddStringToObject(context, "system_name", ctx->system_name);
    cJSON_AddNumberToObject(context, "model", ctx->model);
    cJSON_AddItemToObject(json, "context", context);

    cJSON* breaks = cJSON_CreateArray();
    for (int i = 0; i < MAX_BREAKPOINTS; ++i) {
      if (!breakpoints[i].enabled) continue;
      cJSON* breakpoint = cJSON_CreateObject();
      cJSON_AddNumberToObject(breakpoint, "id", i);
      cJSON_AddNumberToObject(breakpoint, "address", breakpoints[i].address);
      cJSON_AddNumberToObject(breakpoint, "type", breakpoints[i].type);
      cJSON_AddItemToArray(breaks, breakpoint);
    }
    cJSON_AddItemToObject(json, "breakpoints", breaks);

    cJSON* registers = cJSON_CreateObject();
    cJSON_AddNumberToObject(registers, "pc", Z80_PC);
    cJSON_AddNumberToObject(registers, "sp", Z80_SP);
    cJSON_AddNumberToObject(registers, "af", Z80_AF);
    cJSON_AddNumberToObject(registers, "bc", Z80_BC);
    cJSON_AddNumberToObject(registers, "de", Z80_DE);
    cJSON_AddNumberToObject(registers, "hl", Z80_HL);
    cJSON_AddNumberToObject(registers, "af_prime", Z80_AF_PRIME);
    cJSON_AddNumberToObject(registers, "bc_prime", Z80_BC_PRIME);
    cJSON_AddNumberToObject(registers, "de_prime", Z80_DE_PRIME);
    cJSON_AddNumberToObject(registers, "hl_prime", Z80_HL_PRIME);
    cJSON_AddNumberToObject(registers, "ix", Z80_IX);
    cJSON_AddNumberToObject(registers, "iy", Z80_IY);
    cJSON_AddNumberToObject(registers, "i", Z80_I);
    cJSON_AddNumberToObject(registers, "r_1", Z80_R7);
    cJSON_AddNumberToObject(registers, "r_2", (Z80_R & 0x7f));

    cJSON_AddNumberToObject(registers, "z80_t_state_counter", z80_state.t_count);
    cJSON_AddNumberToObject(registers, "z80_clockspeed", z80_state.clockMHz);
		cJSON_AddNumberToObject(registers, "z80_iff1", z80_state.iff1);
    cJSON_AddNumberToObject(registers, "z80_iff2", z80_state.iff2);
    cJSON_AddNumberToObject(registers, "z80_interrupt_mode", z80_state.interrupt_mode);

    cJSON_AddItemToObject(json, "registers", registers);

    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return str;
}

// Waits until it gets the signal to initiate continuous running.
static int emu_run_looper(void *ptr) {
  while(trx_running) {
    SDL_Delay(50);
    if (next_async_action != TRX_CONTROL_TYPE_NOOP) {
      emulation_running = true;
      ctx->control_callback(next_async_action);
      emulation_running = false;
      emulation_is_halting = true;
      next_async_action = TRX_CONTROL_TYPE_NOOP;
    }
  }
  return 0;
}

static void on_frontend_message(const char* msg) {
  if (strcmp("action/refresh", msg) == 0) {
    send_update_to_web_debugger();
  } else if (strcmp("action/step", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_STEP);
    send_update_to_web_debugger();
  } else if (strcmp("action/step-over", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_STEP_OVER);
    send_update_to_web_debugger();
  } else if (strcmp("action/continue", msg) == 0) {
    // Running is done asynchronously to not block the main TRX thread.
    next_async_action = TRX_CONTROL_TYPE_CONTINUE;
  } else if (strcmp("action/stop", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_HALT);
  } else if (strcmp("action/pause", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_PAUSE);
    send_update_to_web_debugger();
  } else if (strcmp("action/soft_reset", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_SOFT_RESET);
  } else if (strcmp("action/hard_reset", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_HARD_RESET);
  } else if (strncmp("action/get_memory", msg, 17) == 0) {
    send_memory_segment(msg + 18);
  } else if (strncmp("action/add_breakpoint/pc", msg, 24) == 0) {
    add_breakpoint(msg + 25, TRX_BREAK_PC);
  } else if (strncmp("action/add_breakpoint/mem", msg, 25) == 0) {
    add_breakpoint(msg + 26, TRX_BREAK_MEMORY);
  } else if (strncmp("action/remove_breakpoint", msg, 24) == 0) {
    remove_breakpoint(msg + 25);
  } else {
    printf("[TRX] WARNING: Unknown message: '%s'\n", msg);
  }
}

static void www_handler(struct mg_connection *conn,
                        int ev, void *ev_data, void *fn_data) {
  switch(ev) {
	case MG_EV_HTTP_MSG: {
	  handle_http_request(conn, (struct mg_http_message*) ev_data);
		break;
	}
  case MG_EV_WS_MSG: {
    static char message[50];
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    strncpy(message, wm->data.ptr, wm->data.len);
    message[wm->data.len] = '\0';
    on_frontend_message(message);
    break;
  }
	case MG_EV_CLOSE: {
		if (conn == status_conn) {
			status_conn = NULL;
		}
		break;
	}
	}
}

static void handleDynamicUpdate() {
  // We want to send one last update when the running emulation shut down so
  // that the frontend has the latest state.
  if (!emulation_is_halting && !emulation_running) return;

  if (emulation_is_halting) {
    puts("[TRX] Haling emulation; sending one more update.");
  }
  uint32_t now_millis = SDL_GetTicks();
  uint32_t diff_millis = now_millis - last_update_sent;

  if (diff_millis < 40 && !emulation_is_halting) return;
  send_update_to_web_debugger();
  send_memory_segment("0/65536");
  last_update_sent = now_millis;
  emulation_is_halting = false;
}

static int www_looper(void *ptr) {
  while (trx_running) {
    mg_mgr_poll(&www_mgr, 40);
    handleDynamicUpdate();
  }
  mg_mgr_free(&www_mgr);
  return 0;
}

static bool init_webserver(void) {
  mg_mgr_init(&www_mgr);
  struct mg_connection *conn = mg_http_listen(
      &www_mgr, "0.0.0.0:8080", &www_handler, NULL /*fn_data args */);
  if (conn == NULL) {
    puts("[TRX] ERROR(Mongoose): Cannot set up listener!");
    return false;
  }
  thread = SDL_CreateThread(www_looper, "Mongoose thread", (void *)NULL);
  if (thread == NULL) {
    puts("[TRX] ERROR(Mongoose/SDL): Unable to create thread!");
    return false;
  }
  return true;
}

