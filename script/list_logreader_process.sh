#!/usr/bin/env bash

prefix=$(readlink -f $1)

for pid in $(ps ux | grep 'oblogreader' | grep -v 'grep' | grep -v "$0" | awk '{print $2}'); do
    if [ -z "${pid}" ]; then
        continue
    fi
    start_time=$(ls -l --time-style=+%s /proc/${pid}/cwd | awk '{print $6}')
    path=$(readlink -f /proc/${pid}/cwd)
    if [[ $? -eq 0 ]]; then
        path=$(echo ${path} | sed 's/(deleted)//')
        _prefix=$(dirname ${path})
        if [[ $? -eq 0 ]] && [[ ${_prefix} == ${prefix} ]]; then
            echo -e "${pid}\t${path}\t${start_time}"
        fi
    fi
done
