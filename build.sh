#!/bin/bash

cd PmemKV2/third-party/oneTBB/
make -j
cp -r build/linux_*_release/ ../tbb
cd ../pmdk_source/
#export CFLAGS="-Wno-error"
#git checkout stable-1.9
#git checkout stable-1.3
#make clean -j
make install DESTDIR=/home/dujianglin/code/PmemKV2/third-party/pmdk_lib -j
cd ..
cp -r pmdk_lib/usr/local/lib64/ pmdk
cd ..
mkdir build
cd build
cmake ..
make -j
cd ../..
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/dujianglin/code/PmemKV2/third-party/tbb:/home/dujianglin/code/PmemKV2/third-party/pmdk
cd eRPC
./scripts/hugepages.sh
cmake . -DPERF=on -DROCE=on -DTRANSPORT=infiniband -DCONFIG_IS_AZURE=0 
make -j
cd ../test
make
