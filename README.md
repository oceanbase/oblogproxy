# OceanBase Migration Serivce LogProxy

OceanBase增量日志代理服务，是 [OMS](https://www.oceanbase.com/product/oms) 的一部分。基于 [liboblog](https://github.com/oceanbase/oceanbase), 以服务的形式，提供实时增量链路接入和管理能力，方便应用接入OceanBase增量日志；能够解决网络隔离的情况下，订阅增量日志的需求；并提供多种链路接入方式：
 - Client
 - Canal
 - More...

## Quick start
### Compile
Install CMake 3.2 or above.
```shell
mkdir buildir && cd buildir && cmake .. && make
```

### Play with it
```shell
# run server
./logproxy -f ./conf/conf.json
# run demo client
./demo_client -h127.0.0.1 -P2983 
```

## Documentation
- [Compile](./docs/manual.md#编译)
- [Run](./docs/manual.md#运行)
- [Configuration](./docs/manual.md#配置)

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
