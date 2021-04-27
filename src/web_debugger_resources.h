char* web_debugger_html = "<!DOCTYPE html>\
<html>\
  <head>\
    <title>TRS Debugger</title>  \
    <link rel=\"stylesheet\" href=\"web_debugger.css\">\
    <script src=\"web_debugger.js\"></script>\
  </head>\
  <body onload=\"onLoad();\">\
    <h1>TRS Debugger</h1>\
  </body>\
</html>";
char* web_debugger_js = "function onLoad() {\
  var socket = new WebSocket(\"ws://\" + location.host + \"/registers\");\
  socket.onmessage = function(evt) {\
    var json = JSON.parse(evt.data);\
    console.log(json);\
  };\
}";
char* web_debugger_css = "body {\
  font-family: 'Courier New', Courier, monospace;\
  background: yellow;\
}";
