var Socket = require("./lib/httpd").Socket;
var ka = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nServer: node\r\nConnection: Keep-Alive\r\n\r\n";
var kalen = ka.length;
var server;
var request = {};
function parseMessage(b) {
  var nheaders;
  var len;
  var key, val;
  request.major = b[0];
  request.minor = b[1];
  request.headers = nheaders = b[2];
  request.method = b[3];
  request.upgrade = b[4];
  request.keepalive = b[5];
  request.headers = [];
  len = (b[8] << 24) + (b[9] << 16) + (b[10] << 8) + (b[11]);
  var off = 12;
  request.url = b.asciiSlice(off, off + len);
  off += len;
  while(nheaders--) {
    len = (b[off] << 8) + b[off + 1];
    off += 2;
    key = b.asciiSlice(off, off + len);
    off += len;
    len = (b[off] << 8) + b[off + 1];
    off += 2;
    val = b.asciiSlice(off, off + len);
    off += len;
    request.headers.push([key, val]);
  }
  return request;
}
function onHeaders(fd) {
  var request = parseMessage(server.in);
}
function onRequest(fd) {
  server.write(fd, 0, kalen);
}
server = new Socket();
server.onHeaders = onHeaders;
server.onRequest = onRequest;
server.setCallbacks();
server.in = new Buffer(4 * 1024);
server.out = new Buffer(kalen);
server.out.asciiWrite(ka, 0, kalen);
server.listen("0.0.0.0", 8080);