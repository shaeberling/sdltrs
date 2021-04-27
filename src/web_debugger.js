function onLoad() {
  var socket = new WebSocket("ws://" + location.host + "/registers");
  socket.onmessage = function(evt) {
    var json = JSON.parse(evt.data);
    console.log(json);
  };
}