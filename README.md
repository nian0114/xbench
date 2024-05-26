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


# 使用方法
## 客户端
1. 通过ethtool查看网卡对应的PCI总线地址 ethtool -i xxx。
```
#如ens224为发包网卡
ethtool -i ens224
```

2. 绑定DPDK网卡
```
# PCI总线地址为0000:0b:00.0， 网卡驱动为uio_pci_generic
# https://doc.dpdk.org/guides/linux_gsg/linux_drivers.html
sudo ./dpdk/dpdk-22.03/usertools/dpdk-devbind.py -b uio_pci_generic 0000:0b:00.0
```

3. 启动APP
```
# --allow= PCI总线地址 如0000:0b:00.0
# -a 绑定IP 如10.0.0.2
# -g 网关地址 如10.0.0.1
# -m 子网掩码 如255.255.255.0
# -l Content-Length 如1
# -p 监听端口 默认为10000 如80
# -l 响应体最大长度 默认为1024，一般情况下默认会按照最大填充，考虑到JSON/XML格式固定，故可能小于你设置的值
# -r 响应内容格式 默认为0随机
## 响应内容格式
 1: application/json
 2: application/xml
 3: text/plain
 其他任意值: text/html
# -d 敏感数据范围 默认为63全开，需要开启的内容做或运算，此功能对性能影响较大
## 敏感数据内容
 1: 身份证
 2: 手机号
 4: 车牌号
 8: 银行卡号
 16: 工商注册号
 32: 邮箱地址
LD_LIBRARY_PATH=./dpdk/install/lib/x86_64-linux-gnu ./app -l 0-1 --proc-type=primary --file-prefix=pmd1 --allow=0000:0b:00.0 -- -a 10.0.0.2 -g 10.0.0.1 -m 255.255.255.0 -l 1 -p 10000 -d 3 -r 0
```

## 服务端
1. 通过ethtool查看网卡对应的PCI总线地址 ethtool -i xxx。
```
#如ens224为收包网卡
ethtool -i ens224
```

2. 绑定DPDK网卡
```
# PCI总线地址为0000:0b:00.0， 网卡驱动为uio_pci_generic
# https://doc.dpdk.org/guides/linux_gsg/linux_drivers.html
sudo ./dpdk/dpdk-22.03/usertools/dpdk-devbind.py -b uio_pci_generic 0000:0b:00.0
```

3. 启动APP
```
# --allow= PCI总线地址 如0000:0b:00.0
# -a 绑定IP 如10.0.0.4
# -g 网关地址 如10.0.0.1
# -m 子网掩码 如255.255.255.0
# -s 服务器IP 如10.0.0.3
# -c 客户端数量 如128 (1-128)
# -u（可选） URL地址 默认值为/ 如/aaaaa，如果需要随机字符，则'/\`\`\`\`\`'， \`数量代表随机字符串的长度
# -h（可选） Host 默认值为X 如www.taobao.com 
# -p 服务器端口 默认为10000 如80

LD_LIBRARY_PATH=./dpdk/install/lib/x86_64-linux-gnu ./app -l 0-1 --proc-type=primary --file-prefix=pmd1 --allow=0000:0b:00.0 -- -a 10.0.0.4 -g 10.0.0.1 -m 255.255.255.0 -p 80 -s 10.0.0.3 -c 128 -u '/`````' -h www.baidu.com
```

