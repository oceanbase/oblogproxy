#!/usr/bin/env

edition='business'
version=$(strings rpm/oblogproxy-version.txt)
docker_build=''
cmake_build_type=''

file_path=$(dirname $0 | xargs readlink -f)
export PATH=${file_path}/packenv/deps/usr/local/oceanbase/devtools/bin:$PATH
usage() {
  echo "Usage: $0 -e <edition> -t <cmake build type> -d <Whether to enable docker image compilation, ON means enabled, OFF means disabled>"
  exit 0
}
while getopts "e:v:t:d:h" o; do
  case "${o}" in
  e)
    edition=${OPTARG}
    ;;
  d)
    docker_build=${OPTARG}
    ;;
  t)
    cmake_build_type=${OPTARG}
    ;;
  h)
    usage
    ;;
  *)
    usage
    ;;
  esac
done

function compile() {
  name=$1
  build_type=$2
  if [ -z "${build_type}" ]; then
    build_type="RelWithDebInfo"
  fi
  echo "building and packaging: ${name}, cmake build type: ${build_type}"

  cd ${file_path} &&
    rm -rf packenv && mkdir -p packenv &&
    cd packenv &&
    rm -rf ./oblogproxy/ &&
    cmake -DOBLOGPROXY_INSTALL_PREFIX=$(pwd)/oblogproxy -DWITH_TEST=OFF -DWITH_ASAN=OFF -DWITH_DEMO=OFF -DCMAKE_VERBOSE_MAKEFILE=ON -DWITH_DEBUG=OFF -DWITH_US_TIMESTAMP=ON -DCMAKE_BUILD_TYPE=${build_type} .. &&
    make -j $(grep -c ^processor /proc/cpuinfo) install oblogproxy &&
    tar -zcf ${name}.tar.gz oblogproxy
}

if [ -z "${version}" ]; then
  echo "The compilation parameter -v must be specified, specifying the compiled version number."
fi

tm=$(date +%Y%m%d%H%M%S)
version=$(strings rpm/oblogproxy-version.txt)
compile oblogproxy-"${version}"-"${tm}" ${cmake_build_type}

if [ "${docker_build}" == "ON" ]; then
  cd "${file_path}" &&
    rm -rf docker/oblogproxy
  cp -r packenv/oblogproxy docker/oblogproxy
  sudo docker build -t oblogproxy:"${version}" -f docker/Dockerfile docker
  echo "Compile the docker image"
fi