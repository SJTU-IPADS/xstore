#!/usr/bin/env sh

../magic.py config -f build-config.toml

cmake . ; make;

./test_array

./test_alloc

./test_tree

./test_node

./test_tree_con

./test_iter