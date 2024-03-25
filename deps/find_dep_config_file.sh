#!/bin/bash

function compat_centos9() {
  OS_RELEASE=9
}

function compat_centos8() {
  OS_RELEASE=8
}

function compat_centos7() {
  OS_RELEASE=7
}

function not_supported() {
  echo "[ERROR] '$PNAME' is not supported yet."
}

function version_ge() {
  test "$(awk -v v1=$VERSION_ID -v v2=$1 'BEGIN{print(v1>=v2)?"1":"0"}' 2>/dev/null)" == "1"
}

function get_os_release() {
  case "$ID" in
  alinux)
    version_ge "3.0" && compat_centos8 && return
    version_ge "2.0" && compat_centos7 && return
    ;;
  alios)
    version_ge "8.0" && compat_centos8 && return
    version_ge "7.2" && compat_centos7 && return
    ;;
  anolis)
    version_ge "8.0" && compat_centos8 && return
    version_ge "7.0" && compat_centos7 && return
    ;;
  ubuntu)
    version_ge "22.04" && compat_centos9 && return
    version_ge "16.04" && compat_centos7 && return
    ;;
  centos)
    version_ge "8.0" && OS_RELEASE=8 && return
    version_ge "7.0" && OS_RELEASE=7 && return
    ;;
  almalinux)
    version_ge "9.0" && compat_centos9 && return
    version_ge "8.0" && compat_centos8 && return
    ;;
  debian)
    version_ge "9" && compat_centos7 && return
    ;;
  fedora)
    version_ge "33" && compat_centos7 && return
    ;;
  opensuse-leap)
    version_ge "15" && compat_centos7 && return
    ;;
  #suse
  sles)
    version_ge "15" && compat_centos7 && return
    ;;
  uos)
    version_ge "20" && compat_centos7 && return
    ;;
  esac
  not_supported && return 1
}

PWD="$(
  cd $(dirname $0)
  pwd
)"

function locate_deps_file() {
  OS_ARCH="$(uname -p)" || exit 1
  OS_RELEASE="0"

  if [[ ! -f /etc/os-release ]]; then
    echo "[ERROR] os release info not found" 1>&2
  fi

  source /etc/os-release || exit 1
  PNAME=${PRETTY_NAME:-${NAME} ${VERSION}}

  get_os_release || exit 1

  OS_TAG="el$OS_RELEASE.$OS_ARCH"
  DEP_FILE="${PWD}/oblogproxy.${OS_TAG}.deps"
  if [[ ! -f "${DEP_FILE}" ]]; then
    echo "NOT FOUND" 1>&2
    exit 2
  else
    echo ${DEP_FILE}
  fi
}
locate_deps_file
