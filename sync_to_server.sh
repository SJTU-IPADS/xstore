#!/usr/bin/env bash

#target="$1"
target="wxd@cube1"
## this script will sync the project to the remote server
rsync -i -rtuv \
      $PWD/src $PWD/tests  $PWD/xcli  $PWD/cli $PWD/benchs $PWD/magic.py  $PWD/server $PWD/*.toml \
      $PWD/*.py \
      $PWD/xcli \
      $PWD/ae_scripts \
      $target:/raid/wxd/fstore

rsync -i -rtuv $PWD/deps/masstree $PWD/deps/mousika   $PWD/deps/r2 $PWD/deps/rlib $PWD/deps/new_cache $target:/raid/wxd/fstore/deps
