var Socket = require("./lib/socket").Socket;
var BUF_SIZE = 64 * 1024;
var recv = 0;
var chunks = 0;
function onConnect(fd) {
  s.setNoDelay(fd, false);
}
function onChunk(fd, len) {
  s.write(fd, 0, len);
  recv += len;
  chunks++;
}
var s = new Socket();
s.onChunk = onChunk;
s.onConnect = onConnect;
s.setCallbacks();
s.in = s.out = new Buffer(BUF_SIZE);
s.listen("0.0.0.0", 8080);
setInterval(function() {
  console.log("recv: " + recv + ", chunk: " + chunks);
  recv = chunks = 0;
}, 1000);