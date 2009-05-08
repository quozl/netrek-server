#!/bin/sh
# autogen.sh prepares a source tree after a darcs get, see README.darcs
# you need tools, see README.developers "Setting up a Build Environment"

for file in config.guess config.sub ltmain.sh;
do
    [ -e $file ] && rm $file
done

aclocal
libtoolize --copy
if [ ! -f config.sub ]; then
    # later versions of libtool silently fail to create config.sub
    # unless --install is added, yet --install is not valid on the
    # older versions.
    libtoolize --install --copy
fi
autoconf
cd res-rsa && autoconf && cd ..
chmod +x tools/mktrekon
cd tools/admin && chmod +x `grep ^EXECS Makefile.in | cut -d '=' -f 2` && cd ../..
chmod +x debian/rules debian/postinst debian/postrm
chmod +x debian/netrek-server-vanilla.init
chmod +x tests/build
echo "autogen.sh completed ok"

# to test your build environment, read and run tests/build
