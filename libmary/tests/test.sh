#!/bin/sh

MY_DEV_BUILD_MODE=sgn

. "$MY_DEV_HOME/common_build"

run ()
{
    if ! "$@"; then
	echo "$* FAILED"
        echo
        echo "FAILURE"
        echo
	exit 1
    fi
}

run make -f Makefile.tests

run ./test__cached_file

echo
echo PASSED
echo

