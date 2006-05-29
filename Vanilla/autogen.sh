#!/bin/sh
aclocal
libtoolize --copy
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
echo "autogen.sh completed ok"
