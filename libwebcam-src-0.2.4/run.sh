#!/bin/sh
cd build
CPPFLAGS="-I/usr/include" CFLAGS="-I/usr/include -O2 -mtune=core2" CXXFLAGS="-I/usr/include -O2 -mtune=core2" LDFLAGS="-L/usr/lib" cmake .. -DCMAKE_INSTALL_PREFIX=/usr
cd ..
