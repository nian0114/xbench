# XBench

这个项目包含一个非常轻量级的 Web 服务器/客户端应用程序，建立在 lwIP 和 DPDK 之上。

这个项目的主要目的是快速进行性能测试，以查看例如NTA/WAF等网络设备性能，工作原理就是进行简单的TCP Ping-Pong。

# 测试设备

CPU：Intel(R) Xeon(R) CPU E5-2696 v4 @ 2.20GHz

系统： Ubuntu 20.04.6 LTS with 5.15.0-67-generic

DPDK： 22.03

LWIP： 2.1.3

CONTRIB： 2.1.0

# 环境要求

```
# dpdk
apt-get install meson
apt install python3-pyelftools
apt-get install pkg-config

# develop tools
apt install gcc
apt install make   

# debug
apt install ethtool
```

