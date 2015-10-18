#!/bin/sh

if test `uname` = 'Darwin'; then
    export FEATURES_HOST_OSX='yes'
fi

if test -z "$SGN_PREFIX"; then
    echo "SGN_PREFIX is not set"
    exit 1
fi

if [ "x$FEATURES_HOST_OSX" = "xyes" ]; then
    echo "Mac OS X"
    LDFLAGS="-L$SGN_PREFIX/lib -Wl,-L$SGN_PREFIX/lib"
else
    if [ "`uname -m`" = "x86_64" ]; then
        echo "64bit"
        LDFLAGS="-L$SGN_PREFIX/lib -Wl,--dynamic-linker=$SGN_PREFIX/lib/ld-linux-x86-64.so.2 -Wl,-L$SGN_PREFIX/lib -Wl,-rpath=$SGN_PREFIX/lib"
    else
        echo "32bit"
        LDFLAGS="-L$SGN_PREFIX/lib -Wl,--dynamic-linker=$SGN_PREFIX/lib/ld-linux.so.2 -Wl,-L$SGN_PREFIX/lib -Wl,-rpath=$SGN_PREFIX/lib"
    fi
fi

g++ -o features                                                 \
    features.cpp                                                \
    -I ..                                                       \
    -std=gnu++0x                                                \
    `pkg-config --libs --cflags libmoment-1.0 nettle`           \
    -lgmp                                                       \
    -DMOMENT_NETTLE                                             \
    -DMOMENT_STANDALONE_FEATURES                                \
    $LDFLAGS

#    -L/opt/moment/lib                                           \
#    -Wl,--dynamic-linker=/opt/moment/lib/ld-linux-x86-64.so.2   \
#    -Wl,-L/opt/moment/lib                                       \
#    -Wl,-rpath=/opt/moment/lib

