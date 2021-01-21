#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
#target = "wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/../magic.py  $PWD/../xcomm $PWD/../xkv_core $PWD/../*.toml  $PWD/../xutils $PWD/../benchs \
      $PWD/../x_ml \
      $PWD/../xcache \
      $PWD/../*.py \
      $PWD/../lib.hh  \
      $PWD/../xcli \
      $PWD/../tests \
      $PWD/../dockerfiles \
      $target:/raid/wxd/xstore-open \
      --exclude 'CMakeCache.txt' \
      --exclude 'scripts/ar.txt'

rsync -i -rtuv $PWD/../deps/port $PWD/../deps/kvs-workload \
      $PWD/../deps/ggflags \
      $PWD/../deps/progress-cpp \
      $PWD/../deps/r2 $PWD/../deps/rlib  \
      $target:/raid/wxd/xstore/deps/
