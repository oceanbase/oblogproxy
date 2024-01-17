<p align="center">
    <a href="https://github.com/oceanbase/oblogproxy/blob/dev/LICENSE">
        <img alt="license" src="https://img.shields.io/badge/license-MulanPubL--2.0-blue" />
    </a>
    <a href="https://github.com/oceanbase/oblogproxy/releases/latest">
        <img alt="license" src="https://img.shields.io/badge/dynamic/json?color=blue&label=release&query=tag_name&url=https%3A%2F%2Fapi.github.com%2Frepos%2Foceanbase%2Foblogproxy%2Freleases%2Flatest" />
    </a>
    <a href="https://www.oceanbase.com/docs/oblogproxy-doc">
        <img alt="Chinese doc" src="https://img.shields.io/badge/文档-简体中文-blue" />
    </a>
    <a href="https://github.com/oceanbase/oblogproxy/commits/dev">
        <img alt="last commit" src="https://img.shields.io/github/last-commit/oceanbase/oblogproxy/dev" />
    </a>
</p>

# OBLogProxy

[OBLogProxy](https://github.com/oceanbase/oblogproxy) 是 [OceanBase](https://github.com/oceanbase/oceanbase) 的增量日志代理服务，它可以与 OceanBase 建立连接并进行增量日志读取，为下游服务提供了变更数据捕获（CDC）的能力。

## OBLogProxy 功能特点

OBLogProxy 有 2 种模式，分别是 Binlog 模式和 CDC 模式。

### Binlog 模式

Binlog 模式为 OceanBase 兼容 MySQL binlog 而推出，支持现有的 MySQL binlog 增量解析工具实时同步 OceanBase，使 MySQL binlog 增量解析工具可以平滑切换到 OceanBase 数据库。

### CDC 模式

CDC 模式用于解决数据同步，CDC 模式下 OBLogProxy 可以订阅 OceanBase 数据库中的数据变更，并将这些数据变更实时同步至下游服务，实现数据的实时或准实时复制和同步。

**有关于 OBLogProxy 的更多内容，请参考 [OBLogProxy 文档](https://www.oceanbase.com/docs/oblogproxy-doc) 。**

## 源码构建 OBLogProxy

### 前提条件

安装 CMake，要求版本 3.20 及以上。下载安装，请参考 [CMake 官方网站](https://cmake.org/download) 。

### 安装依赖

基于 Fedora （包括 CentOS，Fedora，OpenAnolis，RedHat，UOS 等）

```bash
yum install -y git wget rpm rpm-build gcc gcc-c++ make glibc-devel glibc-headers libstdc++-static binutils zlib zlib-devel bison flex java-11-openjdk
```

### 源码编译

```shell
git clone https://github.com/oceanbase/oblogproxy.git
cd oblogproxy
mkdir build
cd build
cmake ..
cmake --build . -j 8
```
### 编译选项

在执行 CMake 编译时，可以添加编译选项改变默认的行为。例如, 编译出 Demo，成功后，当前目录还会产出 `demo_client` 二进制。

```bash
mkdir build
cd build
cmake -DWITH_DEMO=ON ..
cmake --build . -j 8 
```

项目中的其他编译选项。

| 选项         | 默认  | 说明                                                                                 |
|------------|-----|------------------------------------------------------------------------------------|
| WITH_DEBUG | ON  | 调试模式带 Debug 符号                                                                     |
| WITH_ASAN  | OFF | 编译带 [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer) |
| WITH_TEST  | OFF | 测试                                                                                 |
| WITH_DEMO  | OFF | Demo                                                                               |
| WITH_DEPS  | ON  | 自动下载预编译依赖                                                                          |

## 许可

OBLogProxy 使用 MulanPubL-2.0 许可。当您修改或分发源代码时，请遵守 [MulanPubL-2.0](http://license.coscl.org.cn/MulanPubL-2.0) 。

## 贡献

我们热烈欢迎并高度赞赏您的贡献。您可以通过以下几种方式做出贡献：

- 向我们提出 [issue](https://github.com/oceanbase/oblogproxy/issues) 。
- 向我们提交请求 [pull request](https://github.com/oceanbase/oblogproxy/pulls) 。

## 支持

如果您在使用 OBLogProxy 时遇到任何问题，欢迎联系我们寻求帮助：

- [GitHub Issue](https://github.com/oceanbase/oblogproxy/issues)
- [官方网站](https://open.oceanbase.com)
