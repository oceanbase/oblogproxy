#!/usr/bin/env bash

pid=$1
path=`readlink -f $2`

_path=$(readlink -f /proc/${pid}/cwd)
if [[ ${?} -eq 0 ]] && [[ ${_path} == ${path} ]]; then
    exit 0
fi
exit -1
