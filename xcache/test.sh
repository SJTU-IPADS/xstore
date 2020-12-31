#!/usr/bin/env bash
set -e

../magic.py config -f build-config2.toml

cmake . ; make;

./test_rmi_t
./test_rmi
./test_dispatcher
./test_sampler
