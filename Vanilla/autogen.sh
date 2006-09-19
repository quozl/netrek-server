#!/bin/sh
aclocal
libtoolize --copy
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
chmod +x tools/admin/*
chmod +x debian/rules debian/postinst debian/postrm debian/init
echo "autogen.sh completed ok"
