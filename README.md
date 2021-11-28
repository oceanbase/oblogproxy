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
此时可以使用 [oblogclient](https://github.com/oceanbase/oblogclient) 进行OB数据订阅。 Maven依赖，见： [Maven Repo](https://search.maven.org/search?q=g:com.oceanbase.logclient)
```xml
<dependency>
  <groupId>com.oceanbase.logclient</groupId>
  <artifactId>logproxy-client</artifactId>
  <version>1.0.1</version>
</dependency>
```
编写代码，参考：
```Java
ObReaderConfig config = new ObReaderConfig();
// 设置OceanBase root server 地址列表，格式为（可以支持多个，用';'分隔）：ip1:rpc_port1:sql_port1;ip2:rpc_port2:sql_port2
config.setRsList("127.0.0.1:2882:2881;127.0.0.2:2882:2881");
// 设置用户名和密码（非系统租户）
config.setUsername("root");
config.setPassword("root");
// 设置启动位点（UNIX时间戳，单位s）, 0表示从当前时间启动。
config.setStartTimestamp(0L);
// 设置订阅表白名单，格式为：tenant.db.table, '*'表示通配.
config.setTableWhiteList("sys.*.*");

// 指定oblogproxy服务地址，创建实例.
LogProxyClient client = new LogProxyClient("127.0.0.1", 2983, config);
// 添加 RecordListener
client.addListener(new RecordListener() {
    @Override
    public void notify(LogMessage message){
        // 处理消息
    }

    @Override
    public void onException(LogProxyClientException e) {
        // 处理错误
        if (e.needStop()) {
            // 不可恢复异常，需要停止Client
            client.stop();
        }
    }
});

// 启动
client.start();
client.join();
```

更多详情见 [使用文档](https://github.com/oceanbase/oblogclient)

## 注意
- 配置里面无需指定具体的OB集群信息，理论上oblogproxy能同时订阅多个OB集群，只要与所有observer网络通，且OB集群包含配置中的sys租户账号密码。（需要配置sys租户账号密码由于安全原因，通常不对oblogclient侧的用户暴露）
- 本身是无状态，订阅哪个OB，哪个库表都由oblogclient传入，且增量链路的位点等信息也需要oblogclient端自行保存。断开重连后，对于oblogproxy来讲，是创建全新的链路。
- 消耗内存较多，强烈建议和observer分开部署。

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
