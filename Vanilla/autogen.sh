#!/bin/sh
# autogen.sh prepares a source tree after a git clone
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
    cd res-rsa && autoconf
    cd ..
fi

echo "autogen.sh completed ok"

# to test your build environment, read and run tests/build
