var Socket = require("./lib/httpd").Socket;
var Buffer = process.binding("buffer").SlowBuffer;
var ka = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nServer: node\r\nConnection: Keep-Alive\r\n\r\n";
var kalen = ka.length;
var server;
function onRequest(fd) {
  server.write(fd, 0, kalen);
}
server = new Socket();
server.onRequest = onRequest;
server.setCallbacks();
server.in = new Buffer(4 * 1024);
server.out = new Buffer(kalen);
server.out.asciiWrite(ka, 0, kalen);
server.listen("0.0.0.0", 8080);