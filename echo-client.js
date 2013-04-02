var Socket = require("./lib/socket").Socket;
var BUF_SIZE = 64 * 1024;
function onChunk(fd, len) {
  s.write(fd, 0, len);
}
function onConnect(fd) {
  s.setNoDelay(fd, false);
  s.write(fd, 0, 64 * 1024);
}
function Client() {
  s.connect("0.0.0.0", 8080);
}
var s = new Socket();
s.onChunk = onChunk;
s.onConnect = onConnect;
s.setCallbacks();
s.in = s.out = new Buffer(BUF_SIZE);
var clients = parseInt(process.argv[2] || 10);
while(clients--) {
  new Client();
}