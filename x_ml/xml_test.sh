#!/usr/bin/env sh

../magic.py config -f build-config.toml

cmake. ; make;

./test_lr;
./test_nn;
./test_x;
