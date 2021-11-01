# itrustee client

#### 介绍
该目录下文件包含3个功能组件，分别是:libteec.so、teecd、tlogcat.
libteec.so、teecd用于REE与TEE的通信,其中libteec.so是动态链接库，teecd是可执行文件;
tlogcat 用于日志输出存储。


#### 编译依赖
该目录下组件编译，依赖开源库libboundscheck，请从以下链接下载：
https://gitee.com/openeuler/libboundscheck
下载后将文件解压为libboundscheck文件夹，放在当前目录即可，即
libteec_vendor
  |--libboundscheck
          |--- include
	  |--- src
	  |--- Makefile
	  |---


#### 构建方法
同时编译3个组件，运行命令
```
make
```
生成可执行文件teecd、tlogcat，动态库libteec.so，存放在新创建的dist目录下。

只编译某一个组件，运行命令
```
make libteec.so
make teecd
make tlogcat
```
编译的同时，会生成一个动态库 libboundscheck.so，
所有新生成的文件，都存放在新创建的dist目录下。

#### 使用方法
teecd、tlogcat使用方法
使用前，将teecd、tlogcat拷贝到/usr/bin目录，将libteec.so/libboundscheck.so拷贝到/usr/lib64或者usr/lib目录,
设置teecd、tlogcat的权限为700：
chmod 700 teecd
chmod 700 tlogcat

确认tzdriver.ko已正常拉起，确认方法如下：
执行lsmod命令，并搜索关键字tzdriver，如果有显示，则已经正常拉起，示例如下：
lsmod | grep tzdriver

然后执行以下命令将teecd、tlogcat拉起：
/usr/bin/teecd &
/usr/bin/tlogcat -f &
正常拉起后，可以通过 ps -A | grep teecd 、ps -A | grep tlogcat查看到该进程
若显示‘Exit 255’，则是拉起失败，可以在dmesg打印中查看问题原因。
失败的原因可能有以下几类：
1.给teecd/tlogcat配置的权限不对，无法拉起
2.未找到teecd/tlogcat需要使用的动态库
3.teecd/tlogcat的依赖项tzdriver.ko未能正常拉起

然后执行测试用例。

tlogcat存储的日志路径为/var/log/tee。可以直接执行/usr/bin/tlogcat，将日志打印在屏幕上。
获取帮忙信息：/usr/bin/tlogcat -h
打印iTrustee版本信息：/usr/bin/tlogcat -v
只打印最新日志信息：/usr/bin/tlogcat -t
获取cpu中断信息：/usr/bin/tlogcat -e
