## How to Build

### Preparation

#### Install CMake

Install [CMake](https://cmake.org/download) 3.20 or later.

#### Install Dependencies

Fedora based (including CentOS, Fedora, OpenAnolis, RedHat, UOS, etc.)

```bash
yum install which git wget rpm rpm-build cpio gcc gcc-c++ make glibc-devel glibc-headers libstdc++-static binutils openssl-devel libaio-devel
```

Debian based (including Debian, Ubuntu, etc.)

```bash
apt-get install git wget rpm rpm2cpio cpio gcc make build-essential binutils
```

### Build

Get the source code and execute the following commands in the project directory.

```bash
mkdir buildenv && cd buildenv
cmake .. 
make -j 6 
```

Then there should be a binary output `logproxy` in the current directory. You can set the working directory by the following commands.

```bash
mkdir -p ./oblogproxy/bin ./oblogproxy/run 
cp -r ../conf ./oblogproxy/
cp ../script/run.sh ./oblogproxy/
cp logproxy ./oblogproxy/bin/
```

### Build Options

There are some build options.

| Option        | Default | Description                                                                                                                                                  |  
|---------------|---------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| WITH_DEBUG    | ON      | Debug mode flag.                                                                                                                                             |
| WITH_ASAN     | OFF     | Flag of whether to build with [AddressSanitizer](https://github.com/google/sanitizers).                                                                      |
| WITH_TEST     | OFF     | Flag of whether to build test.                                                                                                                               |
| WITH_DEMO     | OFF     | Flag of whether to build demo.                                                                                                                               |
| WITH_GLOG     | ON      | Flag of whether to build with glog.                                                                                                                          |
| WITH_DEPS     | ON      | Flag of whether to automatically download precompiled dependencies.                                                                                          |
| USE_LIBOBLOG  | OFF     | Flag of whether to build with a customized precompiled libobcdc/liboblog.                                                                                    |
| USE_OBCDC_NS  | ON      | Flag of whether to build with libobcdc, use 'OFF' to build with former liboblog (before 3.1.3).                                                              |
| USE_CXX11_ABI | ON      | Flag of whether to build with C++11 ABI. Note that if precompiled dependencies are used, they need to be consistent, otherwise the symbols will not be found |

For example, if you want to build with precompiled libobcdc, you can use the following commands.

```bash
mkdir buildenv && cd buildenv
CMAKE_INCLUDE_PATH=/path/to/libobcdc CMAKE_LIBRARY_PATH=/path/to/libobcdc cmake -DUSE_LIBOBLOG=ON .. 
make -j 6 
```