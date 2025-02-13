#!/bin/bash

# Call this script with 4 parameters to make the rpm package
##  $1: TOP_DIR, i.e. the parent dir of this script
##  $2: PACKAGE_NAME
##  $3: VERSION, we will ignore this parameter, and use the project VERSION in ${PROJECT_DIR}/CMakeLists.txt instead
##  $4: RELEASE
##  $5: RPM or TAR
##  $6: COMMUNITY_BUILD or not
##  $7: BUILD_TYPE
##  $8: CONCURRENT

#set -e
set -x

# parse args
if [ $# -lt 4 ]; then
    exit 1
fi

CUR_DIR=$(dirname $(readlink -f "$0"))
PROJECT_DIR=${1:-${CUR_DIR}/../}
PACKAGE_NAME=$2
VERSION=$(strings ${PROJECT_DIR}/rpm/oboms-logproxy-VER.txt)
RELEASE=$4
TYPE=${5:-RPM}
COMMUNITY_BUILD=${6:-OFF}
BUILD_TYPE=${7:-RelWithDebInfo}
CONCURRENT=${8:-$(grep -c ^processor /proc/cpuinfo)}

ENV_DIR=${PROJECT_DIR}/packenv
ENV_DEP_DIR=${ENV_DIR}/deps

source "${PROJECT_DIR}"/script/get_os_release.sh
OS_ARCH=$(uname -m)
OS_TAG=${OS_ARCH}/${OS_RELEASE}

mkdir -p "${ENV_DEP_DIR}" && cd "${ENV_DEP_DIR}"
echo "[BUILD] args: PROJECT_DIR=${PROJECT_DIR} PACKAGE_NAME=${PACKAGE_NAME} VERSION=${VERSION} RELEASE=${RELEASE} TYPE=${TYPE} COMMUNITY_BUILD=${COMMUNITY_BUILD} CONCURRENT=${CONCURRENT}"
echo "OS_TAG=${OS_TAG}"
echo "ENV_DEP_DIR=${ENV_DEP_DIR}"

yum install -y pigz swig bison

# java
yum install -y java-11-openjdk-devel
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk
export JRE_HOME=/usr/lib/jvm/jre-11-openjdk
export CLASSPATH=${JAVA_HOME}/lib:${JRE_HOME}/lib:$CLASSPATH

function download_and_extract_devtool() {
  local file_name=$1
  local done_file="${ENV_DEP_DIR}/${file_name}.done"

  if [ ! -f "${done_file}" ]; then
    cd "${ENV_DEP_DIR}" || exit 1
    wget -q "https://mirrors.aliyun.com/oceanbase/development-kit/el/${OS_RELEASE}/${OS_ARCH}/${file_name}"
    rpm2cpio "${file_name}" | cpio -idvm
    touch "${done_file}"
  fi
}

download_and_extract_devtool "obdevtools-cmake-3.22.1-22022100417.el${OS_RELEASE}.${OS_ARCH}.rpm"
CMAKE_COMMAND=${ENV_DEP_DIR}/usr/local/oceanbase/devtools/bin/cmake

download_and_extract_devtool "obdevtools-flex-2.6.1-32023111015.el${OS_RELEASE}.${OS_ARCH}.rpm"
export FLEX=${ENV_DEP_DIR}/usr/local/oceanbase/devtools/bin/flex


export PATH=${ENV_DEP_DIR}/usr/local/oceanbase/devtools/bin:${JAVA_HOME}/bin:${JRE_HOME}/bin:$PATH

gcc_done_file=${ENV_DEP_DIR}/gcc.done
if [ ! -f "${gcc_done_file}" ]; then
  chmod +x "${PROJECT_DIR}"/deps/dep_create.sh
  bash "${PROJECT_DIR}"/deps/dep_create.sh OFF tool ${ENV_DEP_DIR} obdevtools-gcc9
  cp ${ENV_DEP_DIR}/usr/local/oceanbase/devtools/lib64/libstdc++.so.6.0.28 /usr/lib64
  cd /usr/lib64 && rm -rf libstdc++.so.6 && ln -sf libstdc++.so.6.0.28 libstdc++.so.6
  touch "${gcc_done_file}"
fi

sqlite_done_file=${ENV_DEP_DIR}/sqlite.done
if [ ! -f "${sqlite_done_file}" ]; then
  cd "${ENV_DEP_DIR}" || exit 1
  wget -q --no-check-certificate https://sqlite.org/2023/sqlite-autoconf-3440200.tar.gz
  tar -zxvf sqlite-autoconf-3440200.tar.gz && cd sqlite-autoconf-3440200/ || exit 1
  ./configure && make && make install
  rm -f /usr/bin/sqlite3 && ln -sf /usr/local/bin/sqlite3 /usr/bin/sqlite3
  rm -f /usr/lib64/llibsqlite3.so.0.8.6 && ln -sf /usr/local/lib/libsqlite3.so.0.8.6 /usr/lib64/libsqlite3.so.0.8.6
  rm -f /usr/lib64/libsqlite3.so && ln -sf /usr/local/lib/libsqlite3.so /usr/lib64/libsqlite3.so
  echo "/usr/local/lib" > /etc/ld.so.conf.d/sqlite3.conf && ldconfig
  if [ $? -ne 0 ]; then
      echo "Failed to install sqlite"
      exit 3
  fi
  touch "${sqlite_done_file}"
fi

which make
which cmake
which gcc
which cc
which c++
which g++
which flex
which bison
which sqlite3

ls -l /usr/lib/jvm/
sqlite3_version=$(sqlite3 --version)
bison_version=$(bison --version)

echo "CMAKE_COMMAND=${CMAKE_COMMAND}, JAVA_HOME=${JAVA_HOME}, FLEX=$FLEX, SQLITE3_VERSION=${sqlite3_version}, BISON_VERSION=${bison_version}"

# build and package
if [[ -f ${PROJECT_DIR}/env/version-compatible.json ]]; then
  sed -i "/logproxy-version/c\  \"logproxy-version\": \"${VERSION}-${RELEASE}\"," ${PROJECT_DIR}/env/version-compatible.json
fi
cd ${ENV_DIR}

if [ "${TYPE}" = "TAR" ]; then
    mkdir -p ${PROJECT_DIR}/ob_artifacts
    ${CMAKE_COMMAND} -DOBLOGPROXY_INSTALL_PREFIX=$(pwd)/oblogproxy -DCOMMUNITY_BUILD=${COMMUNITY_BUILD} -DCMAKE_VERBOSE_MAKEFILE=ON -DWITH_TEST=OFF -DWITH_ASAN=OFF -DWITH_DEBUG=OFF  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -D OBLOGPROXY_PACKAGE_NAME=${PROJECT_NAME} -D OBLOGPROXY_PACKAGE_RELEASE=${RELEASE} ..
    make -j ${CONCURRENT} install oblogproxy
    cp -r ${PROJECT_DIR}/env/ ./oblogproxy/env
    mkdir -p ./oblogproxy/deps/lib && cp -r ./deps/lib/* ./oblogproxy/deps/lib
    tar --use-compress-program=pigz -cvpf ${PACKAGE_NAME}-"${VERSION}"-"${RELEASE}"."${OS_ARCH}".tar.gz oblogproxy

    mv ${PACKAGE_NAME}-"${VERSION}"-"${RELEASE}"."${OS_ARCH}".tar.gz ${PROJECT_DIR}/ob_artifacts/

else
    ${CMAKE_COMMAND} -DCOMMUNITY_BUILD=${COMMUNITY_BUILD} -DCMAKE_VERBOSE_MAKEFILE=ON -DWITH_DEBUG=OFF -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -D OBLOGPROXY_PACKAGE_NAME=${PROJECT_NAME} -D OBLOGPROXY_PACKAGE_RELEASE=${RELEASE} ..
    ${CMAKE_COMMAND} --build . --target package -j ${CONCURRENT}
    ls -l
    cd ${CUR_DIR}
    find ${ENV_DIR} -name "*.rpm" -maxdepth 1 -exec mv {} . 2>/dev/null \;
fi
