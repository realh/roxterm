#!/usr/bin/env bash
cd `dirname "$0"`
if [ -d ".git" ] ; then
    v=`git describe --match '[0-9]*'`
    v=${v/-/.}
    printf "${v/-/\~}" | tee version
else
    cat version
fi
