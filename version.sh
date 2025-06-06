#!/usr/bin/env bash
cd `dirname "$0"`
if [ -d ".git" ] ; then
    v=`git describe`
    echo "$v" | grep -qv '^beta-' && v=`git describe --match '[0-9]*'`
    [ -z "$v" ] && v=`git describe --match 'beta-[0-9]*'`
    v=${v/-/.}
    v="${v/-/\~}"
    printf $v > version
    if echo $v | grep -qv '~' ; then
        printf $v > release_version
    fi
elif [ -f version ] ; then
    v=`cat version`
else
    v=`cat release_version`
fi
printf "${v/beta./}"
