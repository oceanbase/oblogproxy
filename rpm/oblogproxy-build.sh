#!/bin/bash

CUR_DIR=$(dirname $(readlink -f "$0"))
PROJECT_DIR=${1:-${CUR_DIR}/../}
PROJECT_NAME=$2
VERSION=$3
RELEASE=$4
CPU_CORES=`grep -c ^processor /proc/cpuinfo`
echo "[BUILD] args: CURDIR=${CUR_DIR} PROJECT_NAME=${PROJECT_NAME} VERSION=${VERSION} RELEASE=${RELEASE}"

# inject env variables
export PROJECT_NAME=${PROJECT_NAME}
export VERSION=${VERSION}
export RELEASE=${RELEASE}

# install openjdk
yum install -y java-11-openjdk

# prepare building env
cd $CUR_DIR
DEP_DIR=$CUR_DIR/deps
mkdir -p $DEP_DIR
OS_ARCH=$(uname -m)
OS_RELEASE=$(grep -Po '(?<=release )\d' /etc/redhat-release)
OS_TAG=${OS_ARCH}/${OS_RELEASE}

CMAKE_COMMAND=cmake
case $OS_TAG in
    x86_64/7)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/7/x86_64/obdevtools-cmake-3.22.1-22022100417.el7.x86_64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el7.x86_64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    x86_64/8)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/8/x86_64/obdevtools-cmake-3.22.1-22022100417.el8.x86_64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el8.x86_64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    x86_64/9)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/8/x86_64/obdevtools-cmake-3.22.1-22022100417.el8.x86_64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el8.x86_64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    aarch64/7)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/7/aarch64/obdevtools-cmake-3.22.1-22022100417.el7.aarch64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el7.aarch64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    aarch64/8)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/8/aarch64/obdevtools-cmake-3.22.1-22022100417.el8.aarch64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el8.aarch64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    aarch64/9)
    wget https://mirrors.aliyun.com/oceanbase/development-kit/el/8/aarch64/obdevtools-cmake-3.22.1-22022100417.el8.aarch64.rpm -P $DEP_DIR
    rpm2cpio ${DEP_DIR}/obdevtools-cmake-3.22.1-22022100417.el8.aarch64.rpm | cpio -idvm
    export PATH=${CUR_DIR}/usr/local/oceanbase/devtools/bin:$PATH
    ;;
    **)
    echo "Unsupported os arch, please prepare the building environment in advance."
    ;;
esac

# build rpm
cd $PROJECT_DIR
rm -rf build_rpm
mkdir build_rpm
cd build_rpm
${CMAKE_COMMAND} .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON -DWITH_DEBUG=OFF -DWITH_US_TIMESTAMP=ON -DOBLOGPROXY_VERSION=$VERSION -DOBLOGPROXY_RELEASEID=$RELEASE
${CMAKE_COMMAND} --build . --target package -j ${CPU_CORES}

# archiving artifacts
cd $CUR_DIR
find ${PROJECT_DIR}/build_rpm -name "*.rpm" -maxdepth 1 -exec mv {} . 2>/dev/null \;
