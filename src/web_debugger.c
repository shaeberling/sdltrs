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
static struct mg_connection* status_conn = NULL;
static TRX_Context* ctx = NULL;

static bool init_webserver(void);
static char* get_registers_json(void);

// public
bool init_trs_xray(TRX_Context* ctx_param) {
	if (!init_webserver()) {
    puts("ERROR(TRX): Aborting initialization.");
    return false;
  }
  ctx = ctx_param;
  return true; // TODO
}

// public
void send_update_to_web_debugger() {
  if (status_conn == NULL) {
    return;
  }
  // Send registers.
  char* message = get_registers_json();
  mg_ws_send(status_conn, message, strlen(message), WEBSOCKET_OP_TEXT);
  free(message);
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
  } else if (mg_http_match_uri(message, "/action")) {
    mg_http_reply(conn, 200, "Content-Type: text/plain\r\nConnection: close\r\n", "ACTION!");
    send_update_to_web_debugger();
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

    cJSON_AddNumberToObject(registers, "flag_sign", SIGN_FLAG != 0);
    cJSON_AddNumberToObject(registers, "flag_zero", ZERO_FLAG != 0);
    cJSON_AddNumberToObject(registers, "flag_undoc5", (Z80_F & UNDOC5_MASK) != 0);
    cJSON_AddNumberToObject(registers, "flag_half_carry", HALF_CARRY_FLAG != 0);
    cJSON_AddNumberToObject(registers, "flag_undoc3", (Z80_F & UNDOC3_MASK) != 0);
    cJSON_AddNumberToObject(registers, "flag_overflow", OVERFLOW_FLAG != 0);
    cJSON_AddNumberToObject(registers, "flag_subtract", SUBTRACT_FLAG != 0);
    cJSON_AddNumberToObject(registers, "flag_carry", CARRY_FLAG != 0);
    cJSON_AddItemToObject(json, "registers", registers);

    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return str;
}

static void on_frontend_message(const char* msg) {
  if (strcmp(msg, "action/refresh") == 0) {
    send_update_to_web_debugger();
  } else if (strcmp(msg, "action/step") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_STEP);
  } else if (strcmp(msg, "action/step-over") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_STEP_OVER);
  } else if (strcmp(msg, "action/continue") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_CONTINUE);
  } else if (strcmp(msg, "action/pause") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_PAUSE);
  } else if (strcmp(msg, "action/soft_reset") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_SOFT_RESET);
  } else if (strcmp(msg, "action/hard_reset") == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_HARD_RESET);
  } else {
    printf("WARNING(TRX): Unknown message: '%s'", msg);
  }
  send_update_to_web_debugger();
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

static int www_looper(void *ptr) {
  for (;;) {
      mg_mgr_poll(&www_mgr, 1000);
  }
  mg_mgr_free(&www_mgr);
  return 0;
}

static bool init_webserver(void) {
  mg_mgr_init(&www_mgr);
  struct mg_connection *conn = mg_http_listen(
      &www_mgr, "0.0.0.0:8080", &www_handler, NULL /*fn_data args */);
  if (conn == NULL) {
    puts("ERROR(Mongoose): Cannot set up listener!");
    return false;
  }
  thread = SDL_CreateThread(www_looper, "Mongoose thread", (void *)NULL);
  if (thread == NULL) {
    puts("ERROR(Mongoose): Unable to create thread!");
    return false;
  }
  return true;
}

