#!/bin/bash

cd googletest
cmake CMakeLists.txt
cp libgtest*.a /usr/lib
cp -a include/gtest/ /usr/include/
