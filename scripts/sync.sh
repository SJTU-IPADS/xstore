#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/../magic.py  $PWD/../xcomm $PWD/../xkv_core $PWD/../*.toml  $PWD/../xutils $PWD/../benchs \
      $PWD/../x_ml \
      $PWD/../*.py \
      $PWD/../lib.hh  \
      $target:/raid/wxd/xstore \
      --exclude 'CMakeCache.txt'

rsync -i -rtuv $PWD/../deps/port $PWD/../deps/kvs-workload $PWD/../deps/r2 $PWD/../deps/rlib $target:/raid/wxd/xstore/deps/
