#!/bin/sh

get_listing() {
    find src -name "$1" | sed 's/src\/\(.\+\)/$(SRC_DIR)\/\1 \\/' | sed -z 's/\(.*\)\\/\1/'
}

echo '===== SOURCE LISTING ====='
get_listing '*.c' sources.mk
echo '===== HEADER LISTING ====='
get_listing '*.h' headers.mk 
echo '=====       END      ====='
