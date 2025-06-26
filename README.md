# iTrustee client

#### 介绍
该目录下文件包含5个功能组件，分别是:libteec.so、teecd、tlogcat、tee_teleport、agentd. libteec.so、teecd，用于REE与TEE的通信。其中libteec.so是动态链接库；teecd是可执行文件; tlogcat 用于日志输出存储；tee_teleport 用于高级语言功能；agentd 用于容器中安全存储等功能的使用。

#### 操作系统
支持ARM服务器，比如鲲鹏920。

#### 编译教程

1）下载client代码。

2）下载libboundscheck库，下载地址：<https://gitee.com/openeuler/libboundscheck>。

3）解压libboundscheck，放到源码目录。

```
itrustee_client
|--include
|--src
|--Makefile
|--......
|--libboundscheck
    |--src
    |--include
    |--Makefile
```

4）cd xxx(client 源码路径) 。

5）make 编译出可执行文件teecd，tlogcat，动态库libteec.so，tee_teleport，agentd，存放在新创建的dist目录下。

6）只编译某一个组件，运行命令：

```
make libteec.so
make teecd
make tlogcat
make agentd
make tee_teleport
```

7）编译的同时，会生成一个动态库 libboundscheck.so， 所有新生成的文件，都存放在新创建的dist目录下。

#### 使用说明
1）确认tzdriver.ko已正常拉起，确认方法如下： 执行lsmod命令，并搜索关键字tzdriver，如果有显示，则已经正常拉起，示例如下： 

```
lsmod | grep tzdriver
```

2）teecd、tlogcat使用方法 使用前，将teecd、tlogcat拷贝到/usr/bin目录，将libteec.so、libboundscheck.so拷贝到/usr/lib64或者usr/lib目录, 设置teecd、tlogcat的权限为700： 

```
chmod 700 teecd
chmod 700 tlogcat
```

3）执行以下命令将teecd、tlogcat拉起：

```
 nohup /usr/bin/teecd &
 nohup /usr/bin/tlogcat -f & 
```

4）正常拉起后，可以通过以下命令查看到该进程：

```
ps -A | grep teecd
ps -A | grep tlogcat
```

5)若拉起进程后显示‘Exit 255’，则是拉起失败，可以在dmesg打印中查看问题原因。 

失败的原因可能有以下几类： 

	1.给teecd/tlogcat配置的权限不对，无法拉起。
	
	2.未找到teecd/tlogcat需要使用的动态库。
	
	3.teecd/tlogcat的依赖项tzdriver.ko未能正常拉起

6）tlogcat存储的日志路径为/var/log/tee。tlogcat的用法如下：

```
/usr/bin/tlogcat #将日志打印在屏幕上。
/usr/bin/tlogcat -h #获取帮忙信息。
/usr/bin/tlogcat -v #打印iTrustee版本信息。
/usr/bin/tlogcat -t #只打印最新日志信息。
```

#### 参与贡献
    如果您想为本仓库贡献代码，请向本仓库任意maintainer发送邮件
    如果您找到产品中的任何Bug，欢迎您提出ISSUE
