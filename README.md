# OceanBase LogProxy

OceanBase LogProxy (CE) is a proxy service of [OceanBase CE](https://github.com/oceanbase/oceanbase). It is a part
of [OMS](https://www.oceanbase.com/product/oms), and it can establish and manage connections with OceanBase for
incremental log reading even with a isolated network.

## Instructions before use

### Version compatibility

LogProxy is based on [libobcdc](https://github.com/oceanbase/oceanbase/tree/master/src/logservice/libobcdc) (
former `liboblog`), so you should install the corresponding version of it firstly. The libobcdc is packaged
in `oceanbase-ce-devel` before 4.0.0, and is packaged in `oceanbase-ce-cdc` in 4.0.0 and the later version, both of
which can be found in the [official download page](https://open.oceanbase.com/softwareCenter/community)
or [official mirror](https://mirrors.aliyun.com/oceanbase/community/stable/el/).

| libobcdc | oblogproxy |
|----------|------------|
| 3.1.1    | 1.0.0      |
| 3.1.2    | 1.0.1      |
| 3.1.3    | 1.0.2      |
| 3.1.4    | 1.0.3      |
| 4.0.0    | 1.1.0      |

### Installation

LogProxy service doesn't need params about OceanBase cluster to get started, one LogProxy can subscribe to multiple
OceanBase clusters at the same time, and the connection configuration is passed from the client.

LogProxy will use a lot of memory, so it is strongly recommended to deploy it separately from the OceanBase server.

## Getting started

### Install

You can install a released version of LogProxy or build it from the source code.

#### Install a released version

If you want to install a released version, firstly you need to configure the yum repo.

```bash
yum install -y yum-utils
yum-config-manager --add-repo https://mirrors.aliyun.com/oceanbase/OceanBase.repo
```

Then you can install the rpm file by one of the following way:

+ Download from [release page](https://github.com/oceanbase/oblogproxy/releases)
  , [official download page](https://open.oceanbase.com/softwareCenter/community)
  or [official mirror](https://mirrors.aliyun.com/oceanbase/community/stable/el/), and install it
  with `yum install -y oblogproxy-{version}.{arch}.rpm`
+ Install it with `yum install -y oblogproxy-{version}`

The installation directory is `/usr/local/oblogproxy` by default.

#### Build from source code

See [How to build](docs/build.md).

### Configure

For security reasons, LogProxy needs to configure the username and password of a certain user, which must be a sys
tenant user of the OceanBase to connect with. Note that the username here should not contain cluster name or tenant
name.

You can configure the username and password by one of the following ways:

- Add it to local conf at `conf/conf.json`.
- Set it in the client params. See
  the [client doc](https://github.com/oceanbase/oblogclient/blob/master/docs/quickstart/logproxy-client-tutorial.md) for
  details.

#### Add it to local conf

Firstly, get the encrypted username and password.

```bash
./bin/logproxy -x username
./bin/logproxy -x password
```

Then add the outputs to `ob_sys_username` and `ob_sys_password` at `conf/conf.json`.

### Start

You can start the service by the following command.

```bash
bash ./run.sh start
```

You can also start LogProxy with customized libobcdc by executing the following command.

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/libobcdc
bash ./run.sh start
```

Then you can use [oblogclient](https://github.com/oceanbase/oblogclient) to subscribe the log data from LogProxy, and
the service is bind to port `2983` by default.

The service log of LogProxy is located at `logs/`, and the service log of LogReader (task thread) is located
at `run/{client-id}/logs/`.

## Licencing

OceanBase Database is under MulanPubL - 2.0 license. You can freely copy and use the source code. When you modify or
distribute the source code, please obey the MulanPubL - 2.0 license.

## Contributing

Contributions are warmly welcomed and greatly appreciated. Here are a few ways you can contribute:

- Raise us an [issue](https://github.com/oceanbase/oblogproxy/issues).
- Submit Pull Requests.

## Support

In case you have any problems when using OceanBase LogProxy, welcome reach out for help:

- [GitHub Issue](https://github.com/oceanbase/oblogproxy/issues)
- [Official Website](https://open.oceanbase.com/)
