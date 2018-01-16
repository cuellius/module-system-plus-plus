#!/bin/bash
CFLAGS=$(python3-config --includes)
LDFLAGS=$(python3-config --ldflags)
g++ -std=c++14 -O2 -Wall cMS.cpp StringUtils.cpp ModuleSystem.cpp CPyObject.cpp OptUtils.cpp -o ms-pp-linux $CFLAGS $LDFLAGS 
chmod 755 ms-pp-linux
