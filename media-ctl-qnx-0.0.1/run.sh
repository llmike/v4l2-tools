#!/bin/sh
CPPFLAGS="-I/usr/include" CFLAGS="-I/usr/include -O3 -march=core2" CXXFLAGS="-I/usr/include -O3 -march=core2" LDFLAGS="-L/usr/lib" ./configure --prefix=/usr
