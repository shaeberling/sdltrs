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
struct mg_connection* status_conn = NULL;

static bool init_webserver(void);
static char* get_registers_json(void);

// public
bool init_web_debugger(void) {
	return init_webserver();
}

static void handle_http_request(struct mg_connection *conn,
                                struct mg_http_message* message) {
  if (mg_http_match_uri(message, "/") || mg_http_match_uri(message, "/index.html")) {
    mg_http_reply(conn, 200, "Content-Type: text/html\r\nConnection: close\r\n", web_debugger_html);
  } else if (mg_http_match_uri(message, "/web_debugger.js")) {
    mg_http_reply(conn, 200, "Content-Type: application/javascript\r\nConnection: close\r\n", web_debugger_js);
  } else if (mg_http_match_uri(message, "/web_debugger.css")) {
    mg_http_reply(conn, 200, "Content-Type: text/css\r\nConnection: close\r\n", web_debugger_css);
  } else if (mg_http_match_uri(message, "/registers")) {
		mg_ws_upgrade(conn, message, NULL);
		status_conn = conn;
  } else if (mg_http_match_uri(message, "/action")) {
    mg_http_reply(conn, 200, "Content-Type: text/plain\r\nConnection: close\r\n", "ACTION!");
		if (status_conn != NULL) {
			char* message = get_registers_json();
			mg_ws_send(status_conn, message, strlen(message), WEBSOCKET_OP_TEXT);
			free(message);
		}
  } else {
    // Resource not found.
    mg_http_reply(conn, 404, "Content-Type: text/html\r\nConnection: close\r\n", "");
  }
}

static char* get_registers_json(void) {
	  cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "a", Z80_A);
    cJSON_AddNumberToObject(json, "b", Z80_B);
    cJSON_AddNumberToObject(json, "c", Z80_C);
    cJSON_AddNumberToObject(json, "d", Z80_D);
    cJSON_AddNumberToObject(json, "e", Z80_E);
    cJSON_AddNumberToObject(json, "f", Z80_F);
    cJSON_AddNumberToObject(json, "h", Z80_H);
    cJSON_AddNumberToObject(json, "l", Z80_L);
    cJSON_AddNumberToObject(json, "ix", Z80_IX);
    cJSON_AddNumberToObject(json, "iy", Z80_IY);
    cJSON_AddNumberToObject(json, "pc", Z80_PC);
    cJSON_AddNumberToObject(json, "sp", Z80_SP);
    cJSON_AddNumberToObject(json, "af", Z80_AF_PRIME);
    cJSON_AddNumberToObject(json, "bc", Z80_BC_PRIME);
    cJSON_AddNumberToObject(json, "de", Z80_DE_PRIME);
    cJSON_AddNumberToObject(json, "hl", Z80_HL_PRIME);
    cJSON_AddNumberToObject(json, "i", Z80_I);
    cJSON_AddNumberToObject(json, "r_1", Z80_R7);
    cJSON_AddNumberToObject(json, "r_2", (Z80_R & 0x7f));

    cJSON_AddNumberToObject(json, "z80_t_state_counter", z80_state.t_count);
    cJSON_AddNumberToObject(json, "z80_clockspeed", z80_state.clockMHz);
		cJSON_AddNumberToObject(json, "z80_iff1", z80_state.iff1);
    cJSON_AddNumberToObject(json, "z80_iff2", z80_state.iff2);
    cJSON_AddNumberToObject(json, "z80_interrupt_mode", z80_state.interrupt_mode);

    cJSON_AddNumberToObject(json, "flag_sign", SIGN_FLAG != 0);
    cJSON_AddNumberToObject(json, "flag_zero", ZERO_FLAG != 0);
    cJSON_AddNumberToObject(json, "flag_undoc5", (Z80_F & UNDOC5_MASK) != 0);
    cJSON_AddNumberToObject(json, "flag_half_carry", HALF_CARRY_FLAG != 0);
    cJSON_AddNumberToObject(json, "flag_undoc3", (Z80_F & UNDOC3_MASK) != 0);
    cJSON_AddNumberToObject(json, "flag_overflow", OVERFLOW_FLAG != 0);
    cJSON_AddNumberToObject(json, "flag_subtract", SUBTRACT_FLAG != 0);
    cJSON_AddNumberToObject(json, "flag_carry", CARRY_FLAG != 0);
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return str;
}


static void www_handler(struct mg_connection *conn,
                        int ev, void *ev_data, void *fn_data) {
  switch(ev) {
	case MG_EV_HTTP_MSG: {
	  handle_http_request(conn, (struct mg_http_message*) ev_data);
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

