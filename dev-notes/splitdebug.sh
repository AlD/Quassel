#!/bin/sh
for i in quasselclient quasselcore; do
  [ -e $i ] && objcopy --only-keep-debug $i ${i}.dbg && objcopy --strip-debug $i && objcopy --add-gnu-debuglink=${i}.dbg $i
done
