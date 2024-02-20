#!/usr/bin/env bash
cd `dirname "$0"`
if [ -d ".git" ] ; then
    v=`git describe --match '[0-9]*'`
    v=${v/-/.}
    v="${v/-/\~}"
    printf $v | tee version
    if echo $v | grep -qv '~' ; then
        printf $v > release_version
    fi
elif [ - f version ] ; then
    cat version
else
    cat release_version
fi
