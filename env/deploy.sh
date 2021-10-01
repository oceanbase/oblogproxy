#!/usr/bin/env bash

if [ ! -f /etc/redhat-release ]; then
    echo "Unsupport OS"
    exit -1
fi

if [ "$(cat /etc/redhat-release | xargs)" == "Aliyun Linux release 2.1903 LTS (Hunting Beagle)" ]; then
    v=7
else
    v=$(cat /etc/redhat-release | sed -r 's/.* ([0-9]+)\..*/\1/')
fi
if [ $v -eq 7 ]; then
    # CentOS 7

    # environment
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/gcc49/gcc49-4.9.2-437507.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/ajdk/ajdk-8.10.15_fp3-20200624121627.alios7.x86_64.rpm'
    #    yum install -y 'http://yum.tbsite.net/alios/7/os/x86_64/Packages/openssl-libs-1.0.2k-12.1.alios7.x86_64.rpm'
    #    yum install -y 'http://yum.tbsite.net/alios/7/os/x86_64/Packages/openssl-devel-1.0.2k-12.1.alios7.x86_64.rpm'

    # DRC
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-cryptopp/t-db-congo-cryptopp-1.0.1-617943.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-lz4/t-db-congo-lz4-0.1.2-622572.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-zookeeper/t-db-congo-zookeeper-0.1.1-614881.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-libantlr3c/t-db-congo-libantlr3c-0.1.3-624171.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-libmicrohttpd/t-db-congo-libmicrohttpd-0.1.2-622566.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-zkserver/t-db-congo-zkserver-0.1.0-614895.el7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-drcmessage/t-db-congo-drcmessage-4.1.12-1738714.alios7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-center/t-db-congo-center-4.2.1-1882840.alios7.x86_64.rpm'

    # liboblog
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/oceanbase-devel/oceanbase-devel-2.2.50-20200519105409.el7.x86_64.rpm'

    # logreader
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/test/t-ant-light-share/t-ant-light-share-1.0.0-20200723112915.alios7.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/test/t-ant-light-ObLogReader/t-ant-light-ObLogReader-1.0.0-20200728201733.alios7.x86_64.rpm'

    # logproxy
    yum install -y 'http://yum.tbsite.net/taobao/7/noarch/test/t-ant-light-logproxy/t-ant-light-logproxy-1.0.0-20200901194822.noarch.rpm'

elif [ $v -eq 6 ]; then
    # CentOS 6

    # environment
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/gcc49/gcc49-4.9.2-9.el6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/ajdk/ajdk-8.10.15_fp3-20200624121627.alios6.x86_64.rpm'
    #    yum install -y 'http://yum.tbsite.net/alios/6/os/x86_64/Packages/openssl-devel-1.0.1e-49.alios6.1.x86_64.rpm'

    # DRC
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-cryptopp/t-db-congo-cryptopp-1.0.1-617943.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-lz4/t-db-congo-lz4-0.1.2-622572.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-zookeeper/t-db-congo-zookeeper-0.1.1-614881.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-libantlr3c/t-db-congo-libantlr3c-0.1.3-624171.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-libmicrohttpd/t-db-congo-libmicrohttpd-0.1.2-622566.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-zkserver/t-db-congo-zkserver-0.1.1-614895.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-drcmessage/t-db-congo-drcmessage-4.1.12-1738714.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-center/t-db-congo-center-4.2.1-1882840.alios6.x86_64.rpm'

    # liboblog
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/current/oceanbase-devel/oceanbase-devel-2.2.50-20200519105409.el6.x86_64.rpm'

    # light
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/test/t-ant-light-share/t-ant-light-share-1.0.0-20200723112915.alios6.x86_64.rpm'
    yum install -y 'http://yum.tbsite.net/taobao/6/x86_64/test/t-ant-light-ObLogReader/t-ant-light-ObLogReader-1.0.0-20200728201733.alios6.x86_64.rpm'

    # logproxy
    yum install -y 'http://yum.tbsite.net/taobao/6/noarch/test/t-ant-light-logproxy/t-ant-light-logproxy-1.0.0-20200901194822.noarch.rpm'

else
    echo "Unsupport CentOS: " $(cat /etc/redhat-release)
    exit -1
fi

if [ -z "$(grep '/opt/taobao/java/bin/' ~/.bashrc)" ]; then
    echo 'export PATH=/opt/taobao/java/bin/:${PATH}' >>~/.bashrc && source ~/.bashrc
fi
