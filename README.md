# minhttp

## about

experimental node.js addon for minimal http/tcp client and server sockets. for
now this is not nearly production ready and has been created to test the
performance limits of https and tcp sockets in node.js.

## stability

this is very much a first cut of the code so expect segfaults and strange errors
when anything unexpected happens. 

## compatibility

should build and run with most recent versions of node.js (0.8.x +). currently
build scripts are only provided for linux and the code makes use of uv handle 
file descriptors which means it will likely only work on linux. has been tested
on fedora 12/gcc 4.4.4/x86_64 against node.js v0.8.4 and v0.10.1.

## performance tests

### minimal httpd

to run the minimal httpd server which only does keepalive HTTP/1.1 responses
with absolute minimum logic:

  node http-min.js
  
and to test performance using apache bench:

  ab -k -c 10 -n 100000 http://0.0.0.0:8080/
  
### httpd with request parsing

  node http-full.js
  
### tcp socket ping-pong

  node echo-server.js
  
  node echo-client.js
  
## performance results

minimal http server can do 67k http responses per seconds using apache bench 
with server and ab client pinned to cpu's that share a CPU cache. in real world 
you won't have this kind of cache sharing but it gives a good idea of the 
maximal throughput. 67k is roughly 5x what the node.js http library can do out
of the box on the same hardware.

## API

TODO
