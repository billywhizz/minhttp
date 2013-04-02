#!/bin/bash
CXXFLAGS="-DNDEBUG -O2 -Wall -fPIC -DPIC -c -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -fno-strict-aliasing -fno-tree-vrp -fno-tree-sink -fno-rtti -fno-exceptions -MMD -I/usr/local/include/node -Ideps"
g++ $CXXFLAGS -o lib/httpd.o src/node_httpd.cc
g++ $CXXFLAGS -o lib/socket.o src/node_socket.cc
g++ $CXXFLAGS -o lib/http_parser.o deps/http_parser.c
g++ -flat_namespace lib/http_parser.o lib/httpd.o -o lib/httpd.node -shared -L/usr/local/lib
g++ -flat_namespace lib/socket.o -o lib/socket.node -shared -L/usr/local/lib
