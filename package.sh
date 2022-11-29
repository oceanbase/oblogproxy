#!/usr/bin/env

ce=$1
version=$2

file_path=$(dirname $0 | xargs readlink -f)

function compile_ce() {
  liboblog_3x_flag=$1
  name=$2

  cd ${file_path}

#  rm -rf packenv && mkdir packenv

  cd packenv
  rm -rf ./oblogproxy/
  cmake -DOBLOGPROXY_INSTALL_PREFIX=`pwd`/oblogproxy -DUSE_LIBOBLOG_3=${liboblog_3x_flag} ..
  make -j $(grep -c ^processor /proc/cpuinfo) install oblogproxy
  tar -zcf ${name}.tar.gz oblogproxy
}

tm=$(date +%Y%m%d%H%M%S)

compile_ce ON oblogproxy-ce-for-3x-${version}-${tm}

compile_ce OFF oblogproxy-ce-for-4x-${version}-${tm}

