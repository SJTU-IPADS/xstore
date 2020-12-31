#!/usr/bin/env sh

../magic.py config -f build-config.toml

cmake . ; make;

./test_remote_con_rw;
