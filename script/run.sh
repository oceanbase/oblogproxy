#!/usr/bin/env bash

BIN='logproxy'

GPID=0
function is_running()
{
    DEPLOY_PATH=$1
    PROC_BIN_NAME=$2
    for pid in `ps ux | grep ${PROC_BIN_NAME} | awk '{print $2}'`; do
        _path=$(readlink -f /proc/${pid}/cwd)
        if [[ ${?} -eq 0 ]] && [[ ${_path} = ${DEPLOY_PATH} ]]; then
            GPID=${pid}
            echo "is_running : (${GPID})${DEPLOY_PATH} ${PROC_BIN_NAME} is running !"
            return 1
        fi
    done
#    echo "is_running : ${DEPLOY_PATH} ${PROC_BIN_NAME} ready !"
    return 0
}

function kill_proc_9()
{
    DEPLOY_PATH=$1
    PROC_BIN_NAME=$2
    force=$3

    [[ ! -d ${DEPLOY_PATH} ]] && {
        echo "@@@@@@@@@@@@@@@ kill_proc : ${DEPLOY_PATH} invalid !"
        return 0
    }

    retry=0
    is_running ${DEPLOY_PATH} ${PROC_BIN_NAME}
    status=$?
    while [[ ${status} -eq 1 ]]; do
        if [ ! -z ${force} ]; then
            kill -9 ${GPID}
            echo "kill_proc force : (${GPID})${DEPLOY_PATH} succ !"
        else
            kill ${GPID}
            echo "kill_proc : (${GPID})${DEPLOY_PATH} succ !"
        fi
        sleep 1
        retry=`expr ${retry} + 1`
        if [ ${retry} -gt 15 ]; then
            force=1
        fi
        is_running ${DEPLOY_PATH} ${PROC_BIN_NAME}
        status=$?
    done

    return 0
}

start()
{
    stop

    log_path="./log"
    if [ ! -d ${log_path} ]; then
        mkdir ${log_path}
    fi
    log_path=`readlink -f ${log_path}`

    chmod u+x ./bin/${BIN} && ./bin/${BIN} -f ./conf/conf.json &> ${log_path}/out.log&
    if [ $? -ne 0 ]; then
        exit -1
    fi

    is_running ${deploy_path} ${BIN}
}

stop()
{
    cd `dirname $0`
    echo "work path : "`pwd`
    deploy_path=`pwd`

    kill_proc_9 ${deploy_path} ${BIN}
}

case C"$1" in
Cstop)
    stop
    echo "${BIN} stoped!"
    ;;
Cstart)
    start
    echo "${BIN} started!"
    ;;
Cdebug)
    debug
    echo "${BIN} started!"
    ;;
Cstatus)
    cd `dirname $0`
    echo "work path : "`pwd`
    deploy_path=`pwd`

    is_running ${deploy_path} ${BIN}
    status=$?
    echo "status : ${status}"
    exit ${status}
    ;;
C*)
    echo "Usage: $0 {start|stop|status}"
    ;;
esac
