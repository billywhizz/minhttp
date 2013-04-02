var Socket = require("./lib/httpd.node").Socket;
var fs = process.binding("fs");
var constants = process.binding("constants");
var mimeTypes = require("./mime.js").mimeTypes;

var openfiles = {};
var INBUFMAX = 4 * 1024; // 4k maximum input buffer
var OUTBUFMAX = 16 * 1024 * 1024; // 16 MB maximum output buffer
var r404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nServer: node\r\nConnection: Keep-Alive\r\n\r\n";
var httpd;
var bin = new Buffer(INBUFMAX);
var bout = new Buffer(OUTBUFMAX);
var off = 0;
var path = "/source/db/static/tests/";

function readFile(fd, size, cb) {
  var off = 0;
  var buf = new Buffer(OUTBUFMAX);
  function chunk(err, bytesRead) {
    if(err) return cb(err);
    off += bytesRead;
    if(bytesRead > 0) {
      fs.read(fd, buf, off, size - off, off, chunk);
    }
    else {
      cb(null, buf, off);
    }
  }
  fs.read(fd, buf, off, size - off, 0, chunk);
}

function loadFile(fn, sfd) {
  var expires = new Date(Date.now() + 86400000);
  expires = expires.toUTCString();
  var fd;
  var fstat;
  function readHandler(err, buf, len) {
    if(err) {
      httpd.write(sfd, 0, r404.length);
      return;
    }
    var extension = fn.split(".").pop();
    var cachecontrol = cachecontrol || "public, max-age=86400, s-maxage=86400";
    var file = {
      path: fn,
      size: fstat.size,
      fd: fd,
      mime: mimeTypes[extension] || "application/octet-stream",
      modified: Date.parse(fstat.mtime),
      stat: fstat,
      etag: [fstat.ino.toString(16), fstat.size.toString(16), Date.parse(fstat.mtime).toString(16)].join("-")
    };
    file.headers = "HTTP/1.1 200 OK\r\nServer: minode\r\nConnection: Keep-Alive\r\nContent-Length: " + fstat.size + "\r\nLast-Modified: " + new(Date)(fstat.mtime).toUTCString() + "\r\nContent-Type: " + file.mime + "\r\n\r\n";
    file.body = buf.asciiSlice(0, len);
    file.start = off;
    bout.asciiWrite(file.headers, off, file.headers.length);
    off += file.headers.length;
    bout.asciiWrite(file.body, off, file.body.length);
    off += file.body.length;
    file.length = off - file.start;
    openfiles[fn] = file;
    fs.close(fd);
    httpd.write(sfd, file.start, file.length);
  }
  function statHandler(err, ffstat) {
    if(err) {
      httpd.write(sfd, 0, r404.length);
      return;
    }
    fstat = ffstat;
    readFile(fd, fstat.size, readHandler);
  }
  function openHandler(err, ffd) {
    if(err) {
      httpd.write(sfd, 0, r404.length);
      return;
    }
    fd = ffd;
    fs.fstat(ffd, statHandler);
  }
  fs.open(fn, constants.O_RDONLY, 0x1ED, openHandler);
}
var url, fname, file;
function onHeaders(fd) {
  url = bin.asciiSlice(12, (bin[8] << 24) + (bin[9] << 16) + (bin[10] << 8) + (bin[11]) + 12);
  if(url[url.length-1] === "/") {
    url += "index.html";
  }
  fname = path + url;
  if(fname in openfiles) {
    file = openfiles[fname];
    httpd.write(fd, file.start, file.length);
  }
  else {
    loadFile(path + url, fd);
  }
}
httpd = new Socket();
httpd.onHeaders = onHeaders;
httpd.setCallbacks();
httpd.in = bin;
httpd.out = bout;
httpd.listen("0.0.0.0", 8080);
bout.asciiWrite(r404, 0, r404.length);
off += r404.length;