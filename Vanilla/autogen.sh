#!/bin/sh
aclocal
libtoolize --copy
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
chmod +x tools/admin/*
echo "autogen.sh completed ok"
