#!/bin/bash

#clear env
unalias -a

PWD="$(cd $(dirname $0); pwd)"
TARGET_DIR=${PWD}
if [[ ! -z "$1" ]]; then
  TARGET_DIR=$1
  mkdir -p ${TARGET_DIR}
fi

echo "dep_create.sh in ${PWD}, target dir: ${TARGET_DIR}"

OS_ARCH="$(uname -p)" || exit 1
OS_RELEASE="0"

if [[ ! -f /etc/os-release ]]; then
  echo "[ERROR] os release info not found" 1>&2 && exit 1
fi

source /etc/os-release || exit 1

PNAME=${PRETTY_NAME:-${NAME} ${VERSION}}

function compat_centos8() {
  echo "[NOTICE] '$PNAME' is compatible with CentOS 8, use el8 dependencies list"
  OS_RELEASE=8
}

function compat_centos7() {
  echo "[NOTICE] '$PNAME' is compatible with CentOS 7, use el7 dependencies list"
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
      version_ge "16.04" && compat_centos7 && return
      ;;
    centos)
      version_ge "8.0" && OS_RELEASE=8 && return
      version_ge "7.0" && OS_RELEASE=7 && return
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

get_os_release || exit 1

OS_TAG="el$OS_RELEASE.$OS_ARCH"
DEP_FILE="${PWD}/oblogproxy.${OS_TAG}.deps"

echo -e "check dependencies profile for ${OS_TAG}... \c"

if [[ ! -f "${DEP_FILE}" ]]; then
    echo "NOT FOUND" 1>&2
    exit 2
else
    echo "FOUND"
fi

mkdir "${TARGET_DIR}/pkg" >/dev/null 2>&1

echo -e "check repository address in profile... \c"
REPO="$(grep -Po '(?<=kit_repo=).*' "${DEP_FILE}" 2>/dev/null)"
if [[ $? -eq 0 ]]; then
    echo "$REPO"
else
    echo "NOT FOUND" 1>&2
    exit 3
fi

STABLE_REPO="$(grep -Po '(?<=stable_repo=).*' "${DEP_FILE}" 2>/dev/null)"
if [[ $? -eq 0 ]]; then
    echo "$STABLE_REPO"
else
    echo "NOT FOUND" 1>&2
    exit 4
fi

echo "download dependencies..."
RPMS="$(grep '\.rpm' "${DEP_FILE}" | grep -Pv '^#')"

for pkg in $RPMS
do
  if [[ -f "${TARGET_DIR}/pkg/${pkg}" ]]; then
    echo "find package <${pkg}> in cache"
  else
    echo -e "download package <${pkg}>... \c"
    TEMP=$(mktemp -p "/" -u ".${pkg}.XXXX")

    if [[ -z `echo "${pkg}" | grep 'oceanbase-ce-devel'` ]]; then
      wget "$REPO/${pkg}" -q -O "${TARGET_DIR}/pkg/${TEMP}"
    else
      wget "$STABLE_REPO/${pkg}" -q -O "${TARGET_DIR}/pkg/${TEMP}"
    fi

    if [[ $? -eq 0 ]]; then
      mv -f "${TARGET_DIR}/pkg/$TEMP" "${TARGET_DIR}/pkg/${pkg}"
      echo "SUCCESS"
    else
      rm -rf "${TARGET_DIR}/pkg/$TEMP"
      echo "FAILED" 1>&2
      exit 5
    fi

    echo -e "unpack package <${pkg}>... \c"
    cd ${TARGET_DIR} && rpm2cpio "${TARGET_DIR}/pkg/${pkg}" | cpio -di -u --quiet
  fi

  if [[ $? -eq 0 ]]; then
    echo "SUCCESS"
  else
    echo "FAILED" 1>&2
    exit 6
  fi
done