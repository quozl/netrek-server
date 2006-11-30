#!/bin/csh
set LIBDIR=/usr/local/games/netrek-server-vanilla/lib
set ROOT=`${LIBDIR}/tools/getpath`
setenv PATH ${ROOT}/bin:${ROOT}/lib:${ROOT}/lib/tools:${PATH}
