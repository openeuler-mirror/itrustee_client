# client

#### Description
There are 3 modules in the directory: libteec.so、teecd、tlogcat.
libteec.so & teecd are used in comunications between REE & TEE,
tlogcat is used for saving logs.

#### Depend on
These modules depend on open source libboundscheck, please download it here:
https://gitee.com/openeuler/libboundscheck ,
and unzip it in current directory like this.
libteec_vendor
  |--libboundscheck
          |--- include
          |--- src
          |--- Makefile
          |--- ...

#### Build
compile all 3 files, do like this
```
make
```

if only need one file, compile like this
```
make libteec.so
make teecd
make tlogcat
```
at the same time, we can get a library libboundscheck.so.
All the generated files are stored in the newly created directory dist.

#### How to use
Run teecd or tlogcat
1) Copy teecd or tlogcat to usr/bin, copy libteec.so & libboundscheck.so to usr/lib[64].
   Change teecd or tlogcat mode to 700:
   chmod 700 teecd
   chmod 700 tlogcat
2) Make sure tzdriver.ko is already running, execute the cmd:
   lsmod | grep tzdriver
   If tzdriver is found, means tzdriver,ko is running.
3) Execute the cmd to start teecd or tlogcat:
   /usr/bin/teecd &
   /usr/bin/tlogcat -f &
   If it started succseccfully, we can find it here:
   ps -A | grep teecd
   ps -A | grep tlogcat
   If printed 'Exit 255', it means something wrong, you can find infos in dmesg logs.
   Nomorly, reasons for failure may be like these:
     i)teecd or tlogcat dose not have the right permission;
     ii)system dose not find the libaries which teecd or tlogcat includes;
     iii)tzdriver,ko dose not start succseccfully.
4) Run your test case.


The log path stored by tlogcat is /var/log/tee. You can also directly execute /usr/bin/tlogcat to print the log on the screen.
Get help information: /usr/bin/tlogcat -h
Print iTrustee version: /usr/bin/tlogcat -v
Only print the new log: /usr/bin/tlogcat -t
Print cpu irq information: /usr/bin/tlogcat -h
