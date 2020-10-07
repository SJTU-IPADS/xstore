#!/bin/bash
find src   -path '../' \
            -prune -o -path '../src/app/tpce/egen' \
            -prune -o -path '../src/port' \
            -prune -o -path '../src/tests' \
            -prune -o -path '../src/app'   \
            -prune -o -print \
    | egrep '\.php|\.as|\.sql|\.css|\.js|\.cc|\.h|.cpp|.c ' \
    | grep -v '\.svn' | xargs cat | sed '/^\s*$/d' \
    | wc -l
