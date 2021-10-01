# 参考手册

## 编译

### 前置条件
0. GCC>=5.2, make, libstdc++-static, libasan(如果开启WITH_ASAN)
1. [CMake](https://cmake.org) >= 3.2
2. 预编译的 [liboblog.so](https://github.com/oceanbase/oceanbase), 其依赖libaio, 需要安装
3. Openssl 1.0.*

（*当前仅支持amd64）

### 编译操作
1. 获取 liboblog.so：https://github.com/oceanbase/oceanbase  
这里假设你成功编译，并把其头文件和so放在了`/path/to/liboblog`。

2. 【可选】创建编译环境目录
```shell
mkdir buildenv
```

3. 执行CMake编译
```shell
cd buildenv
# 设置CMake环境变量从而可以找到预编译的liboblog
CMAKE_INCLUDE_PATH=/path/to/liboblog CMAKE_LIBRARY_PATH=/path/to/liboblog cmake .. 
# 执行
make -j 6 
```

一切正常的话，在当前目录产出了`logproxy`二进制文件，且`oblogmsg.so`也复制到了当前目录。

### 编译选项
上述编译过程的第2步，可以添加编译选项改变默认的行为。例如, 编译出Demo。成功后，当前目录还会产出`demo_client`二进制。
```shell
cd buildenv
CMAKE_INCLUDE_PATH=/path/to/liboblog CMAKE_LIBRARY_PATH=/path/to/liboblog cmake -DWITH_DEMO=ON .. 
make -j 6
```

**全部编译参数**：  

| 选项 | 默认 | 说明 |  
| ------ | -------- | ------- |  
| WITH_DEBUG | ON | 调试模式带 Debug 符号 |   
| WITH_ASAN | OFF | 编译带 [AddressSanitizer](https://github.com/google/sanitizers) |   
| WITH_TEST | OFF | 测试 |   
| WITH_DEMO | OFF | Demo |   
| WITH_GLOG | ON | 使用glog |   
| USE_OBLOGMSG | OFF | 使用预编译的 [oblogmsg](https://github.com/oceanbase/oblogmsg) |   
| USE_CXX11_ABI | ON | 是否使用C++11 ABI。注意如果用了预编译的依赖，需要保持一致，否则会找不到符号 | 

### 编译依赖说明
默认情况下，会自动下载并编译依赖库。但有几个例外：
- **openssl**：当前是去寻找系统安装的，但是1.0.*和1.1.*版本API少量不兼容，当前liboblog和logproxy都是是基于1.0.*实现的，需要版本一致。
- **liboblog**：如前文描述，需要自行编译获取，并由环境变量指定路径，运行时需要指定LD_LIBRARY_PATH。
- **libaio**：liboblog依赖这个，需要提前安装。
- **oblogmsg**：liboblog和logproxy共同依赖 [oblogmsg](https://github.com/oceanbase/oblogmsg) ，自动依赖会采用动态库，并复制到当前编译目录，运行时需要指定LD_LIBRARY_PATH。

## 运行
LogProxy 单一配置文件，用代码目录下的 `conf.json` 即可：
```shell
# 指定liboblog.so目录，然后logproxy可以动态依赖
export LD_LIBRARY_PATH=/path/to/liboblog
./logproxy -f ./conf/conf.json
```
默认监听`2983`端口，修改`conf.json`中的`service_port`字段可更换监听端口。

此时可以使用 [LogProxy Client](https://github.com/oceanbase/oblogclient) 进行OB数据订阅，见 [使用文档](https://github.com/oceanbase/oblogclient/docs/manual.md)

## 链路加密
### Client与LogProxy间TLS通信 
修改`conf.json`中以下字段：
- `channel_type`: "tls"，开启与Client通信的TLS。
- `tls_ca_cert_file`: CA证书文件路径
- `tls_cert_file`: 务器端签名证书路径
- `tls_key_file`: 务器端的私钥路径
- `tls_verify_peer`: true，开启Client验证

对应的Client也需要相应配置，见 [LogProxy Client链路加密](https://github.com/oceanbase/oblogclient/docs/manual.md#链路加密) 。

### LogProxy(liboblog)与ObServer间TLS通信
修改`conf.json`中以下字段：
- `liboblog_tls`: true，开启与ObServer通信的TLS。
- `liboblog_tls_cert_path`: ObServer相关证书文件路径，可以拷贝ObServer部署文件路径下的`wallet`文件夹，需要确保此路径包含：ca.pem，client-cert.pem，client-key.pem"

以上的路径需要使用`绝对路径`。

### 