#!/usr/bin/env bash

set -ev

if test -z "$1"; then
	BUILD_DIR="emscripten_build"
else
	BUILD_DIR="$1"
fi

WORKSPACE=/root/project

cd $WORKSPACE

mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake \
  -DCMAKE_TOOLCHAIN_FILE=../emscripten_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  ..
make