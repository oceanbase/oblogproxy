#!/usr/bin/env bash

##
## make metal-bare RPM environment available
##

cp taobao.repo /etc/yum.repos.d/

yum clean all
yum makecache
yum-config-manager --enable taobao.*

dep_dir=/home/ds
yum install -y dep_create -b current

mkdir ${dep_dir} && cp dep.deps ${dep_dir}/ && cd ${dep_dir} && dep_create ./dep.deps
