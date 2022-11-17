## 依赖说明

默认情况下，会自动下载并编译依赖库。有几个点这里说明下：
- **openssl**：当前使用的版本是：1.0.1e，1.0.*和1.1.*版本API少量不兼容，当前liboblog和logproxy都是是基于1.0.*实现的，需要版本一致。
- **liboblog**：如前文描述，您也可以自行获取或编译，并由环境变量指定路径，运行时需要指定LD_LIBRARY_PATH。
- **libaio**：liboblog依赖。


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
