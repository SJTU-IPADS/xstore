#!/bin/bash
find benchs xcache x_ml xkv_core/ xcomm xutils ! -path '*/deps/*' ! -path '*ccls-cache*' \
    | egrep '\.php|\.as|\.sql|\.css|\.js|\.cc|\.h|.cpp|.c ' \
    | grep -v '\.svn' | xargs cat | sed '/^\s*$/d' \
    | wc -l
