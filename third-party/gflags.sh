#!/bin/bash

cd gflags
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=on
make
make install
