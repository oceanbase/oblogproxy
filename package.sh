#!/usr/bin/env

ce=$1
version=$2
if [ -z "${ce}" ] || [ -z "${version}" ]; then
    echo "Invalid input arguments"
    exit -1
fi

file_path=$(dirname $0 | xargs readlink -f)

function compile_ce() {
  liboblog_3x_flag=$1
  name=$2

  echo "building and packaging ${name}"

  cd ${file_path} && \
  mkdir -p packenv && cd packenv && \
  ls | grep -v *.tar.gz | xargs rm -rf && \
  cmake -DOBLOGPROXY_INSTALL_PREFIX=`pwd`/oblogproxy -DUSE_LIBOBLOG_3=${liboblog_3x_flag} .. && \
  make -j $(grep -c ^processor /proc/cpuinfo) install oblogproxy && \
  tar -zcf ${name}.tar.gz oblogproxy
}

tm=$(date +%Y%m%d%H%M%S)

compile_ce ON oblogproxy-ce-for-3x-${version}-${tm}

compile_ce OFF oblogproxy-ce-for-4x-${version}-${tm}
