#!/usr/bin/env sh

set -e

../magic.py config -f build-config2.toml

cmake . ; make;

./test_lr;
#./test_nn;
./test_x;
#./bench_lr;
