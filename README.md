English | [中文版](README_CN.md)

# OceanBase LogProxy

**OceanBase LogProxy** is an incremental log proxy service for [OceanBase](https://github.com/oceanbase/oceanbase) that establishes a connection to OceanBase and performs incremental log reads, providing change data capture (CDC) capabilities for downstream services.

## Instructions use
### Installation

LogProxy occupies resources separately, so it is recommended to deploy it separately from the OceanBase database.

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

```bash
git clone git@github.com:oceanbase/oblogproxy.git
cd oblogproxy
mkdir build
cmake -S . -B build
cmake --build build
```

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

Then you can use [oblogclient](https://github.com/oceanbase/oblogclient) to subscribe the log data from LogProxy, and
the service is bind to port `2983` by default.

The service log of LogProxy is located at `logs/`, and the service log of LogReader (task process) is located
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
