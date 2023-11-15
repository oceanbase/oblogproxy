#!/usr/bin/env
export PATH=/usr/local/gcc-9.3.0/bin:$PATH
rm -rf buildenv
mkdir buildenv && cd buildenv
cmake -DOBLOGPROXY_INSTALL_PREFIX=$(pwd)/oblogproxy -DWITH_DEBUG=ON -DWITH_TEST=ON -DWITH_ASAN=OFF -DCMAKE_VERBOSE_MAKEFILE=ON -DWITH_DEMO=OFF -DWITH_US_TIMESTAMP=ON ..
make -j $(grep -c ^processor /proc/cpuinfo) install oblogproxy &&
cp -r ../env ./oblogproxy/env &&
mkdir -p ./oblogproxy/deps/lib &&
cp -r ./deps/lib/* ./oblogproxy/deps/lib
