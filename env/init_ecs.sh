##
## make metal-bare
##

cp taobao.repo /etc/yum.repos.d/

yum clean all
yum makecache
yum-config-manager --enable taobao.*

# java && gcc
yum install -y java

# deps
dep_method=manual
os=os7

if [ ${dep_method} == "manual" ]; then

    if [ ${os} == "os7" ]; then
        yum install -y 'http://yum.tbsite.net/alios/7/os/x86_64/Packages/java-1.8.0-openjdk-1.8.0.65-3.b17.1.alios7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/alios/7/os/x86_64/Packages/openssl-devel-1.0.2k-12.1.alios7.x86_64.rpm'

        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-cryptopp/t-db-congo-cryptopp-1.0.1-617943.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-lz4/t-db-congo-lz4-0.1.2-622572.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-zookeeper/t-db-congo-zookeeper-0.1.1-614881.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-libantlr3c/t-db-congo-libantlr3c-0.1.3-624171.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-libmicrohttpd/t-db-congo-libmicrohttpd-0.1.2-622566.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-zkserver/t-db-congo-zkserver-0.1.0-614895.el7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-drcmessage/t-db-congo-drcmessage-4.1.12-1738714.alios7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/t-db-congo-center/t-db-congo-center-4.2.1-1882840.alios7.x86_64.rpm'
        yum install -y 'http://yum.tbsite.net/taobao/7/x86_64/current/oceanbase-devel/oceanbase-devel-2.2.50-20200519105409.el7.x86_64.rpm'

    elif [ ${os} == "os6" ]; then
        yum install -y http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-drcmessage/t-db-congo-drcmessage-4.1.12-1738714.alios6.x86_64.rpm
        yum install -y http://yum.tbsite.net/taobao/6/x86_64/current/t-db-congo-center/t-db-congo-center-4.1.12-1444701.alios6.x86_64.rpm
        yum install -y http://yum.tbsite.net/taobao/6/x86_64/current/oceanbase-devel/oceanbase-devel-2.2.50-20200519105409.el6.x86_64.rpm

    fi

else
    dep_dir=/home/ds
    yum install -y dep_create -b current

    mkdir ${dep_dir} && cp dep.deps ${dep_dir}/ && cd ${dep_dir} && dep_create ./dep.deps

fi
