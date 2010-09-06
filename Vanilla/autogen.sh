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
    # older versions.  later versions also support --no-warn to reduce
    # output volume
    libtoolize --install --copy --no-warn
fi
autoconf
if [ -d res-rsa ]; then
    cd res-rsa && autoconf && cd ..
fi
cd tools/admin && chmod +x `grep ^EXECS Makefile.in | cut -d '=' -f 2` && cd ../..

if [ -d debian ]; then
    chmod +x debian/rules debian/postinst debian/netrek-server-vanilla.init
fi

if [ -f tests/build ]; then
    chmod +x tests/build tests/build-cmake
fi

echo "autogen.sh completed ok"

# to test your build environment, read and run tests/build
