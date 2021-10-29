# OceanBase Migration Serivce LogProxy

OceanBase增量日志代理服务，是 [OMS](https://www.oceanbase.com/product/oms) 的一部分。基于 [liboblog](https://github.com/oceanbase/oceanbase), 以服务的形式，提供实时增量链路接入和管理能力，方便应用接入OceanBase增量日志；能够解决网络隔离的情况下，订阅增量日志的需求；并提供多种链路接入方式：
 - [oblogclient](https://github.com/oceanbase/oblogclient)
 - Canal
 - More...

## Quick start

### 1. 配置OceanBase社区版yum源
```bash
yum install -y yum-utils
yum-config-manager --add-repo https://mirrors.aliyun.com/oceanbase/OceanBase.repo
```

### 2. 下载预编译包

预编译的产出在：[Release](http://pub.mirrors.aliyun.com/oceanbase/community/stable/el/7/x86_64/) ，oblogproxy的包名是"oblogproxy-xxxx.系统版本.x86_64.rpm"，根据自己的系统选取，安装：
```bash
yum install -y oblogproxy-xxxx.系统版本.x86_64.rpm
```

oblogproxy会安装在目录 `/usr/local/oblogproxy` 。

### 3. 配置系统租户
获得observer的sys租户账号密码，通常在创建observer集群时创建，也可以单独创建。oblogproxy需要加密的配置，执行以下命令即可得到：
```bash
# 这里假设账号密码分别为：user，pswd
./logproxy -x user
# 会输出 4B9C75F64934174F4E77EE0E9A588118
./logproxy -x pswd
# 会输出 DCE2AF09D006D6A440816880B938E7B3
```
把获得账号密码密文分别配置到`/usr/local/oblogproxy/conf/conf.json`中的`ob_sys_username`和`ob_sys_password`字段，例如：
```json
{
  "ob_sys_username": "4B9C75F64934174F4E77EE0E9A588118",
  "ob_sys_password": "DCE2AF09D006D6A440816880B938E7B3"
}
```

### 4. 运行
```bash
cd /usr/local/oblogproxy
bash ./run.sh start
```

### 5. 用oblogclient订阅
此时可以使用 [oblogclient](https://github.com/oceanbase/oblogclient) 进行OB数据订阅，见 [使用文档](https://github.com/oceanbase/oblogclient)


## 文档
- [编译](./docs/manual.md#编译)
- [运行](./docs/manual.md#运行)
- [配置](./docs/manual.md#配置)

## Licencing
OceanBase Database is under MulanPubL - 2.0 license. You can freely copy and use the source code. When you modify or distribute the source code, please obey the MulanPubL - 2.0 license.

## Contributing
Contributions are warmly welcomed and greatly appreciated. Here are a few ways you can contribute:
- Raise us an [issue](https://github.com/oceanbase/oblogproxy/issues).
- Submit Pull Requests. 

## Support
In case you have any problems when using OceanBase Database, welcome reach out for help:
- [GitHub Issue](https://github.com/oceanbase/oblogproxy/issues)
- [Official Website](https://open.oceanbase.com/)
