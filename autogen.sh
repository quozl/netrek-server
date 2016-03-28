#!/bin/sh
# autogen.sh prepares a source tree after a git clone
# you need tools, see README.developers "Setting up a Build Environment"

rm -f config.guess config.sub ltmain.sh;

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

echo "autogen.sh completed ok"

# to test your build environment, read and run tests/build
