#!/bin/sh

for file in config.guess config.sub ltmain.sh;
do
    [ -e $file ] && rm $file
done

aclocal
libtoolize --copy
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
chmod +x tools/admin/*
chmod +x debian/rules debian/postinst debian/postrm
chmod +x debian/netrek-server-vanilla.init
chmod +x tests/build
echo "autogen.sh completed ok"
