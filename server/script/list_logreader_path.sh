#!/usr/bin/env bash

dirname=`readlink -f $1`

for path in $(ls ${dirname} | tr '\t' '\n'); do
    d=${path}
    path=$(readlink -f ${dirname}/${path})
    if [[ $? -ne 0 ]]; then
        continue
    fi

    tm=''
    if [ -f "${path}/log" ]; then
        tm=$(ls -l --time-style=+%s ${path}/log | head -2 | tail -1 | awk '{print $6}')
    fi
    if [ -z "${tm}" ]; then
        tm=$(ls -l --time-style=+%s ${path}/.. | grep "${d}" | tail -1 | awk '{print $6}')
    fi

    echo -e "${tm}\t${path}"
done
