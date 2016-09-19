#!/bin/bash

BUILD_PATH="/tmp/zmq-build"

#make libzmq.a
rm -rf $BUILD_PATH
mkdir $BUILD_PATH
cd zeromq4-x-4.0.7
./autogen.sh
./configure --prefix=$BUILD_PATH 
make
make install
make clean

#copy
cd ..
mkdir -p ../../public/include/zmq
cp -rf $BUILD_PATH/include/* ../../public/include/zmq/
cp -rf $BUILD_PATH/lib/libzmq.a ../../public/a
cp -rf $BUILD_PATH/lib/libzmq.so* ../../public/so

#clean
rm -rf $BUILD_PATH
exit 0

