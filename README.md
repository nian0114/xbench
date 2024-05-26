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

# 编译方法
## DPDK
```
DPDK_VER=22.03

PROJ_PWD=`pwd`
DPDK_DIR=${PROJ_PWD}/dpdk/
DPDK_SRC_DIR=${PROJ_PWD}/dpdk-${DPDK_VER}
DPDK_INSTALL_DIR=${PROJ_PWD}/dpdk/install

wget -P ${DPDK_DIR} https://fast.dpdk.org/rel/dpdk-${DPDK_VER}.tar.xz
cd dpdk
tar xvf dpdk-${DPDK_VER}.tar.xz

cd $DPDK_SRC_DIR
meson --prefix=${DPDK_INSTALL_DIR} --libdir=lib/x86_64-linux-gnu ${DPDK_SRC_DIR}/build ${DPDK_SRC_DIR}
ninja -C ${DPDK_SRC_DIR}/build
ninja -C ${DPDK_SRC_DIR}/build install
```

## lwip
```
LWIP_VER=2.1.3
CONTRIB_VER=2.1.0

PROJ_PWD=`pwd`
LWIP_DIR=${PROJ_PWD}/lwip/
LWIP_SRC_DIR = ${LWIP_DIR}/lwip-${LWIP_VER}
CONTRIB_SRC_DIR = ${LWIP_DIR}/contrib-${CONTRIB_VER}

wget -P ${LWIP_DIR} http://download.savannah.nongnu.org/releases/lwip/contrib-${CONTRIB_VER}.zip
unzip contrib-${CONTRIB_VER}.zip -d ${CONTRIB_SRC_DIR}

wget -P ${LWIP_DIR} http://download.savannah.nongnu.org/releases/lwip/lwip-${LWIP_VER}.zip
unzip lwip-${LWIP_VER}.zip -d ${LWIP_SRC_DIR}
```

## app
```
make
```
