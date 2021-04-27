#include "web_debugger.h"
#include "web_debugger_resources.h"

#include <stdlib.h>
#include <stdbool.h>
#include "SDL.h"  // To create thread for mongoose.
#include "mongoose.h"

static struct mg_mgr www_mgr;
static SDL_Thread* thread;
// static const char* content_type_html = "text/html";
// static const char* content_type_js = "application/javascript";
// static const char* content_type_css = "text/css";

static bool init_webserver(void);

// public
bool init_web_debugger(void) {
	return init_webserver();
}

static void www_handler(struct mg_connection *conn,
                        int ev, void *ev_data, void *fn_data) {
  if (ev != MG_EV_HTTP_MSG) {
    return;
  }
  struct mg_http_message* message = (struct mg_http_message*) ev_data;
  if (mg_http_match_uri(message, "/") || mg_http_match_uri(message, "/index.html")) {
    mg_http_reply(conn, 200, "Content-Type: text/html\r\nConnection: close\r\n", web_debugger_html);
  } else if (mg_http_match_uri(message, "/web_debugger.js")) {
    mg_http_reply(conn, 200, "Content-Type: application/javascript\r\nConnection: close\r\n", web_debugger_js);
  } else if (mg_http_match_uri(message, "/web_debugger.css")) {
    mg_http_reply(conn, 200, "Content-Type: text/css\r\nConnection: close\r\n", web_debugger_css);
  } else {
    // Resource not found.
    mg_http_reply(conn, 404, "Content-Type: text/html\r\nConnection: close\r\n", "");
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

