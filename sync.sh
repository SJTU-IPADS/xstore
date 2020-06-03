#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/magic.py  $PWD/xcomm $PWD/*.toml \
      $PWD/*.py \
      $target:/raid/wxd/xstore

rsync -i -rtuv  $PWD/deps/r2 $PWD/deps/rlib $target:/raid/wxd/xstore/deps/
