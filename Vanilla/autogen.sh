#!/bin/sh
libtoolize
aclocal
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
echo "autogen.sh completed ok"
