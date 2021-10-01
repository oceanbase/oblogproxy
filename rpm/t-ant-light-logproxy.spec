##############################################################
# http://baike.corp.taobao.com/index.php/%E6%B7%98%E5%AE%9Drpm%E6%89%93%E5%8C%85%E8%A7%84%E8%8C%83 #
# http://www.rpm.org/max-rpm/ch-rpm-inside.html              #
##############################################################
Name: t-ant-light-logproxy
Version: 1.0.0
Release: %(echo $RELEASE)
# if you want use the parameter of rpm_create on build time,
# uncomment below
Summary: Ant Light Logproxy.
Group: alibaba/application
License: Commercial
AutoReqProv: none
%define _prefix /home/ds/logproxy

BuildArch:noarch

# uncomment below, if depend on other packages

#Requires: package_name = 1.0.0


%description
# if you want publish current svn URL or Revision use these macros
Ant Light Logproxy.

# support debuginfo package, to reduce runtime package size

%build
cd $OLDPWD/..
mvn clean install -Pdev -Dmaven.test.skip=true -s ./rpm/settings.xml

# prepare your files
%install
# OLDPWD is the dir of rpm_create running
# _prefix is an inner var of rpmbuild,
# can set by rpm_create, default is "/home/a"
# _lib is an inner var, maybe "lib" or "lib64" depend on OS

# create dirs
mkdir -p .%{_prefix}/bin .%{_prefix}/conf .%{_prefix}/data .%{_prefix}/log
cp -pr $OLDPWD/../server/script/run.sh .%{_prefix}/run.sh
cp -pr $OLDPWD/../server/target/logproxy-jar-with-dependencies.jar .%{_prefix}/bin/logproxy.jar
cp -pr $OLDPWD/../server/script/list_logreader_process.sh .%{_prefix}/bin/
cp -pr $OLDPWD/../server/script/list_logreader_path.sh .%{_prefix}/bin/
cp -pr $OLDPWD/../server/script/logreader_status.sh .%{_prefix}/bin/
cp -pr $OLDPWD/../server/conf/* .%{_prefix}/conf/

# create a crontab of the package
#echo "
#* * * * * root /home/a/bin/every_min
#3 * * * * ads /home/a/bin/every_hour
#" > %{_crontab}

# package infomation
%files
# set file attribute here
%defattr(-,root,root)
# need not list every file here, keep it as this
%{_prefix}
## create an empy dir

# %dir %{_prefix}/var/log

## need bakup old config file, so indicate here

# %config %{_prefix}/etc/sample.conf

## or need keep old config file, so indicate with "noreplace"

# %config(noreplace) %{_prefix}/etc/sample.conf

## indicate the dir for crontab

# %attr(644,root,root) %{_crondir}/*

%post
#define the scripts for post install
%postun
#define the scripts for post uninstall

%changelog
* Fri Jun 12 2020 yuqi.fy
- add spec of t-ant-light-logproxy
