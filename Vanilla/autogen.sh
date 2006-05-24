#!/bin/sh
libtoolize
aclocal
autoconf
chmod +x tools/mktrekon
echo "autogen.sh completed ok"
