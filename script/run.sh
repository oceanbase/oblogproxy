#!/usr/bin/env bash

cd $(dirname $0)
DEPLOY_PATH=$(pwd)
echo "DEPLOY_PATH : "${DEPLOY_PATH}

LIB_PATH=${DEPLOY_PATH}/liboblog

BIN='logproxy'
GPID=0
function is_running() {
  for pid in $(ps ux | grep ${BIN} | grep -v 'bash ' | grep -v grep | awk '{print $2}'); do
    _path=$(readlink -f /proc/${pid}/cwd)
    if [[ ${?} -eq 0 ]] && [[ ${_path} == ${DEPLOY_PATH} ]]; then
      GPID=${pid}
      echo "is_running : (${GPID})${DEPLOY_PATH} ${BIN} is running!"
      return 1
    fi
  done
  return 0
}

function kill_proc_9() {
  force=$1

  if [[ ! -d ${DEPLOY_PATH} ]]; then
    echo "kill_proc : ${DEPLOY_PATH} invalid!"
    return 0
  fi

  retry=0
  is_running
  status=$?
  while [[ ${status} -eq 1 ]]; do
    if [ ! -z ${force} ]; then
      kill -9 ${GPID}
      echo "kill_proc force : (${GPID})${DEPLOY_PATH} succ!"
    else
      kill ${GPID}
      echo "kill_proc : (${GPID})${DEPLOY_PATH} succ!"
    fi
    sleep 1
    retry=$(expr ${retry} + 1)
    if [ ${retry} -gt 15 ]; then
      force=1
    fi
    is_running
    status=$?
  done

  return 0
}

start() {
  stop

  if [ ! -d "./run" ]; then
    mkdir ./run
  fi

  log_path="./log"
  if [ ! -d ${log_path} ]; then
    mkdir ${log_path}
  fi
  log_path=$(readlink -f ${log_path})

  if [ ! -z "${LIB_PATH}" ]; then
    export LD_LIBRARY_PATH=${LIB_PATH}:${LD_LIBRARY_PATH}
  fi
  chmod u+x ./bin/${BIN} && ./bin/${BIN} -f ./conf/conf.json &>${log_path}/out.log &
  if [ $? -ne 0 ]; then
    exit -1
  fi

  is_running
}

stop() {
  kill_proc_9
}

do_config_sys() {
  username=$1
  password=$2
  if [[ -z "${username}" ]] || [[ -z "${password}" ]]; then
    echo "No input sys username or password"
    exit -1
  fi

  if [ ! -z "${LIB_PATH}" ]; then
    export LD_LIBRARY_PATH=${LIB_PATH}:${LD_LIBRARY_PATH}
  fi
  username_x=`./bin/${BIN} -x ${username}`
  password_x=`./bin/${BIN} -x ${password}`

  cp ./conf/conf.json ./conf/conf.json.new
  sed -r -i 's/"ob_sys_username"[ ]*:[ ]*"[0-9a-zA-Z]*/"ob_sys_username": "'${username_x}'/' ./conf/conf.json.new
  sed -r -i 's/"ob_sys_password"[ ]*:[ ]*"[0-9a-zA-Z]*/"ob_sys_password": "'${password_x}'/' ./conf/conf.json.new
  diff ./conf/conf.json ./conf/conf.json.new
  echo ""

  read -r -p "!!DANGER!! About to update logproxy conf/conf.json, Please confirm? [Y/n] " response
  if [ "${response}" != "y" ] && [ "${response}" != "Y" ]; then
      echo "Cancel!"
      rm -rf ./conf/conf.json.new
      exit 0
  fi

  cp ./conf/conf.json ./conf/conf.json.bak
  mv ./conf/conf.json.new ./conf/conf.json
}

case C"$1" in
Cstop)
  stop
  echo "${BIN} stopped!"
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
  is_running
  status=$?
  echo "status : ${status}"
  exit ${status}
  ;;
Cconfig_sys)
  do_config_sys $2 $3
  ;;
C*)
  echo "Usage: $0 {start|stop|status}"
  ;;
esac
