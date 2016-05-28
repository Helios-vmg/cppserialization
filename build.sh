#!/bin/sh
# This is the build command for Unices.
c++ -std=c++11 src/las.cpp src/main.cpp src/parser.cpp src/sha256.c src/typehash.cpp -o LAS.out
