[English](README.md) | 中文版

# OceanBase LogProxy

**OceanBase LogProxy** 是 [OceanBase](https://github.com/oceanbase/oceanbase) 的增量日志代理服务，它可以与 OceanBase 建立连接并进行增量日志读取，为下游服务提供了变更数据捕获（CDC）的能力。

## 使用说明
### 安装

LogProxy 单独占用资源，建议与 OceanBase 数据库分开部署。

您可以安装 LogProxy 的发布版本或从源代码构建它。

#### 安装已发布版本

如果要安装发布版本，首先需要配置 yum 源。

```bash
yum install -y yum-utils
yum-config-manager --add-repo https://mirrors.aliyun.com/oceanbase/OceanBase.repo
```

然后您可以通过以下方式之一安装 rpm 文件：

+ 从 [发布页面](https://github.com/oceanbase/oblogproxy/releases)、 [官方下载页面](https://open.oceanbase.com/softwareCenter/community) 或 [官方仓库](https://mirrors.aliyun.com/oceanbase/community/stable/el/)下载安装
`yum install -y oblogproxy-{version}.{arch}.rpm`
+ 安装方式 `yum install -y oblogproxy-{version}`

安装目录默认为 `/usr/local/oblogproxy`.

#### 从源代码构建

```bash
git clone git@github.com:oceanbase/oblogproxy.git
cd oblogproxy
mkdir build
cmake -S . -B build
cmake --build build
```

### 配置

出于安全考虑，LogProxy 需要配置某个用户的用户名和密码，该用户必须是 OceanBase 的 sys 租户用户才能连接。注意，此处的用户名不应包含集群名称或租户名称。

您可以通过以下方式配置用户名和密码：

- 将其添加到本地 conf 中 `conf/conf.json`.
- 在客户端参数中设置它。有关详细信息，请参阅[客户端文档](https://github.com/oceanbase/oblogclient/blob/master/docs/quickstart/logproxy-client-tutorial.md)。

#### 添加到本地 conf

首先，获取加密的用户名和密码。

```bash
./bin/logproxy -x username
./bin/logproxy -x password
```

然后将结果输出到 `conf/conf.json` 的 `ob_sys_username` 和 `ob_sys_password`。

### 开始

您可以通过以下命令启动该服务。

```bash
bash ./run.sh start
```

然后就可以使用 [oblogclient](https://github.com/oceanbase/oblogclient) 从 LogProxy 中订阅日志数据，该服务默认绑定端口为`2983`

LogProxy 的服务日志位于`logs/`，LogReader (任务进程) 的服务日志位于`run/{client-id}/logs/`。

## 许可

OceanBase 数据库使用 MulanPubL - 2.0 许可。您可以自由复制和使用源代码。当您修改或分发源代码时，请遵守 MulanPubL - 2.0 许可。

## 贡献

我们热烈欢迎并高度赞赏您的贡献。您可以通过以下几种方式做出贡献：

- 向我们提出一个[issue](https://github.com/oceanbase/oblogproxy/issues)。
- 提交请求。

## 支持

如果您在使用 OceanBase LogProxy 时遇到任何问题，欢迎联系我们寻求帮助：

- [GitHub Issue](https://github.com/oceanbase/oblogproxy/issues)
- [官方网站](https://open.oceanbase.com/)
