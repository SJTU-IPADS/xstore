#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/src $PWD/tests $PWD/CMakeLists.txt  \
      $PWD/examples \
      $target:/raid/wxd/r2

rsync -i -rtuv  $PWD/deps/rlib $PWD/deps/deps.cmake  $target:/raid/wxd/r2/deps/
