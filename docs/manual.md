# 参考手册

## 编译

### 前置条件

#### 1. 安装CMake
要求版本大于等于3.20, 安装包见：[https://cmake.org/download](https://cmake.org/download)

#### 2. 安装依赖

Fedora based (including CentOS, Fedora, OpenAnolis, RedHat, UOS, etc.)
```bash
yum install which git wget rpm rpm-build cpio gcc gcc-c++ make glibc-devel glibc-headers libstdc++-static binutils openssl-devel libaio-devel
```

Debian based (including Debian, Ubuntu, etc.)
```bash
apt-get install git wget rpm rpm2cpio cpio gcc make build-essential binutils
```

### 编译操作

#### 执行CMake编译
```shell
mkdir buildenv && cd buildenv
# 生成
cmake .. 
# 执行
make -j 6 
```

一切正常的话，在当前目录产出了`logproxy`二进制文件。

### 编译选项
上述编译过程，可以添加编译选项改变默认的行为。例如, 编译出Demo。成功后，当前目录还会产出`demo_client`二进制。
```shell
mkdir buildenv && cd buildenv
cmake -DWITH_DEMO=ON .. 
make -j 6
```

#### 自定义 liboblog.so
您可以自己指定编译所使用的liboblog，下载预编译的包或者自行编译二选一。

**下载预编译包：**

预编译的产出在：[Release](https://github.com/oceanbase/oceanbase/releases) ，liboblog的包名是"oceanbase-ce-devel-xxxx.系统版本.x86_64.rpm"，根据自己的系统选取，解压：
```bash
rpm2cpio oceanbase-ce-devel-xxxx.系统版本.x86_64.rpm | cpio -div && mv ./usr ./liboblog
```
解压后：
- liboblog.so: 在 `./liboblog/lib` 目录下
- liboblog.h: 在 `./liboblog/include` 目录下

**编译liboblog：**

下面参考了 [liboblog编译说明](https://open.oceanbase.com/docs/community/oceanbase-database/V3.1.1/abyu9b) ，编译 [OceanBase社区版](https://github.com/oceanbase/oceanbase) 时，添加参数 OB_BUILD_LIBOBLOG=ON。
```bash
git clone https://github.com/oceanbase/oceanbase.git && cd oceanbase
bash ./build.sh release --init -DOB_BUILD_LIBOBLOG=ON --make
```

获得编译产出：
- liboblog.so: 在 `build_release/src/liboblog/src/` 目录下。
- liboblog.h: 在 `src/liboblog/src/` 目录下。

**编译oblogproxy：**

这里假设您把`liboblog.so`和`liboblog.h`都放在了`/path/to/liboblog`。

需要在编译命令中打开自定义liboblog的开关并指定路径：
```shell
mkdir buildenv && cd buildenv
# 设置CMake环境变量从而可以找到预编译的liboblog
CMAKE_INCLUDE_PATH=/path/to/liboblog CMAKE_LIBRARY_PATH=/path/to/liboblog cmake -DUSE_LIBOBLOG=ON .. 
# 执行
make -j 6 
```

#### 全部编译参数

| 选项 | 默认  | 说明                                                                                        |  
| ------ |-----|-------------------------------------------------------------------------------------------|  
| WITH_DEBUG | ON  | 调试模式带 Debug 符号                                                                            |   
| WITH_ASAN | OFF | 编译带 [AddressSanitizer](https://github.com/google/sanitizers)                              |   
| WITH_TEST | OFF | 测试                                                                                        |   
| WITH_DEMO | OFF | Demo                                                                                      |   
| WITH_GLOG | ON  | 使用glog                                                                                    |   
| WITH_DEPS | ON  | 自动下载预编译依赖                                                                                 |   
| USE_LIBOBLOG | OFF | 使用自定义预编译的liboblog                                                                         |
| USE_OBCDC_NS | ON  | 是否使用obcdc进行编译。注意oblogproxy 对 obcdc (原liboblog) 有版本依赖,USE_OBCDC_NS=OFF时，兼容obcdc 3.1.2及之前版本 |
| USE_CXX11_ABI | ON  | 是否使用C++11 ABI。注意如果用了预编译的依赖，需要保持一致，否则会找不到符号                                                | 

### 编译依赖说明

默认情况下，会自动下载并编译依赖库。有几个点这里说明下：
- **openssl**：当前使用的版本是：1.0.1e，1.0.*和1.1.*版本API少量不兼容，当前liboblog和logproxy都是是基于1.0.*实现的，需要版本一致。
- **liboblog**：如前文描述，您也可以自行获取或编译，并由环境变量指定路径，运行时需要指定LD_LIBRARY_PATH。
- **libaio**：liboblog依赖。

## 运行
oblogproxy 单一配置文件，用代码目录下的 `conf.json`。

### 1. 配置系统租户
获得observer的sys租户账号密码，通常在创建observer集群时创建，也可以单独创建。oblogproxy需要加密的配置，执行以下命令即可得到：
```bash
# 这里假设账号密码分别为：user，pswd
./logproxy -x user
# 会输出 4B9C75F64934174F4E77EE0E9A588118
./logproxy -x pswd
# 会输出 DCE2AF09D006D6A440816880B938E7B3
```
把获得账号密码密文分别配置到`conf.json`中的`ob_sys_username`和`ob_sys_password`字段，例如：
```json
{
  "ob_sys_username": "4B9C75F64934174F4E77EE0E9A588118",
  "ob_sys_password": "DCE2AF09D006D6A440816880B938E7B3"
}
```

### 2. 组织程序目录
```bash
# 创建程序目录
mkdir -p ./oblogproxy/bin ./oblogproxy/run 
# 复制配置文件
cp -r ../conf ./oblogproxy/
# 复制起停脚本
cp ../script/run.sh ./oblogproxy/
# 复制程序二进制
cp logproxy ./oblogproxy/bin/
```


### 3. 启动oblogproxy
```shell
cd ./oblogproxy
# 指定liboblog.so目录，让oblogproxy可以动态依赖
export LD_LIBRARY_PATH=/path/to/liboblog
bash ./run.sh start
```
默认监听`2983`端口，修改`conf.json`中的`service_port`字段可更换监听端口。

此时可以使用 [oblogclient](https://github.com/oceanbase/oblogclient) 进行OB数据订阅，见 [使用文档](https://github.com/oceanbase/oblogclient)

## 链路加密
### oblogclient与oblogproxy间TLS通信
修改`conf.json`中以下字段：
- `channel_type`: "tls"，开启与oblogclient通信的TLS。
- `tls_ca_cert_file`: CA证书文件路径（绝对路径）
- `tls_cert_file`: 服务器端签名证书路径（绝对路径）
- `tls_key_file`: 服务器端的私钥路径（绝对路径）
- `tls_verify_peer`: true，开启oblogclient验证（绝对路径）

对应的oblogclient也需要相应配置，见 [oblogclient链路加密](https://github.com/oceanbase/oblogclient) 。

### oblogproxy(liboblog)与ObServer间TLS通信
修改`conf.json`中以下字段：
- `liboblog_tls`: true，开启与ObServer通信的TLS。
- `liboblog_tls_cert_path`: ObServer相关证书文件路径（绝对路径），可以拷贝ObServer部署文件路径下的`wallet`文件夹，需要确保此路径包含：ca.pem，client-cert.pem，client-key.pem

## 配置

通常，您只需要关心前文描述过的参数。对于其他参数，在不完全了解参数用途的情况下，不建议修改。

| 字段 | 默认值      | 说明 |  
| ---- |----------| ---------- |
| service_port | 2983     | 服务端口 |  
| encode_threadpool_size | 8        | 编码线程池初始化大小 |  
| encode_queue_size | 20000    | 编码线程队列长度 |  
| max_packet_bytes | 67108864 | 最大数据包字节数 |  
| record_queue_size | 512      | 数据发送队列长度 |  
| read_timeout_us | 2000000  | 数据读取队列批次等待周期，单位微秒 |  
| read_fail_interval_us | 1000000  | 数据读取队列重试等待周期，单位微秒 |  
| read_wait_num | 20000    | 数据读取队列批次等待数量 |  
| send_timeout_us | 2000000  | 发送数据包超时，单位微秒 |  
| send_fail_interval_us | 1000000  | 发送数据包失败重试等待周期，单位微秒 |  
| command_timeout_s | 10       | 命令执行超时，单位微妙 |  
| log_quota_size_mb | 5120     | 日志文件总大小阈值，单位MB |  
| log_quota_day | 30       | 日志文件存储时间阈值，单位天 |  
| log_gc_interval_s | 43200    | 日志文件清理周期，单位秒 |  
| oblogreader_path_retain_hour | 168      | oblogreader子进程目录保留时间，单位小时 |  
| oblogreader_lease_s | 300      | oblogreader子进程启动探测时间，单位秒 |  
| oblogreader_path | ./run    | oblogreader子进程上下文目录根路径 |  
| allow_all_tenant | true     | 是否允许订阅所有租户 |  
| auth_user | true     | 是否鉴权连接用户 |  
| auth_use_rs | false    | 是否通过root server鉴权用户 |  
| auth_allow_sys_user | true     | 是否允许订阅系统租户 |  
| ob_sys_username | ""       | 【必须自行配置】系统租户用户名密文，用来订阅增量 |  
| ob_sys_password | ""       | 【必须自行配置】系统租户密码密文，用来订阅增量 |  
| counter_interval_s | 2        | 计数器周期，单位秒 |  
| debug | false    | 打印debug信息 |  
| verbose | false    | 打印更多debug信息 |  
| verbose_packet | false    | 打印数据包信息 |  
| readonly | false    | 只读模式 |  
| channel_type | plain    | 链路类型 |  
| tls_ca_cert_file | ""       | CA证书文件路径（绝对路径） |  
| tls_cert_file | ""       | 服务器端签名证书路径（绝对路径） |  
| tls_key_file | ""       | 服务器端的私钥路径（绝对路径） |  
| tls_verify_peer | true     | 开启oblogclient验证 |  
| liboblog_tls | false    | 开启与ObServer通信的TLS |  
| liboblog_tls_cert_path | ""       | ObServer相关证书文件路径（绝对路径）|  
