# F1 - "Systematic Byzantine Fault Tolerante Load Balancer"

## Description

Last piece of code I found of my Master thesis, probably it's imcomplete but it is the most that I've found.

Maybe it helps someone who may be looking for soem answers.

## Install

### Configure and Make the LBs

```
ifconfig eth0 hw ether 001aa0b8132c
ifconfig eth0 inet 10.101.233.153 netmask 255.255.255.0
gcc -Wall -o dropper dropper.c -lnfnetlink -lnetfilter_queue
```

### Install httpd:

```
tar xvf clean-src-httpd.tar.gz
cd httpd-2.4.3
apt-get install libpcre3-dev
./configure --prefix=/path/to/httpd --with-include-apr
make
make install
cd ..
cp balancers/* httpd-2.4.3/modules/proxy/balancers/
cd httpd-2.4.3/modules/proxy/balancers/
make clean
make
make install
cd ../../../..
cp conf/*.conf /path/to/httpd/conf
```

## How to Configure the Network

### 1. Forwarder IPTABLES: 

* Tomcat Servers (10.10.5.126, 10.10.5.221):

```
sh ./tomcat/bin/catalina.sh start
```

* Forwarder (10.10.5.201):

```
sysctl -e net.ipv4.ip_forward=1
arp -s 10.10.5.100 ff:ff:ff:ff:ff:ff
iptables -t nat -A PREROUTING -p tcp --dport 5555 -d 10.10.5.201 -j DNAT --to 10.10.5.100:5555
iptables -t nat -A POSTROUTING -p tcp -d 10.10.5.100 --dport 5555 -j MASQUERADE
```

* LB Servers (10.10.5.153, 10.10.5.172):

```
ifconfig eth0:1 10.10.5.100 netmask 255.255.255.0
iptables -t raw -A PREROUTING -p tcp --dport 5555 -d 10.10.5.100 -j NFQUEUE --queue-num 1
iptables -t mangle -A POSTROUTING -p tcp --sport 5555 -j NFQUEUE --queue-num 1
gcc -Wall -O3 -o dropSendMask dropSendMask.c -lnfnetlink -lnetfilter_queue
./dropSendMask
```

### 1.a Forwarder IPTABLES: (Dif Clients)

* Tomcat Servers (10.10.5.126, 10.10.5.221):

```
sh ./tomcat/bin/catalina.sh start
```

* Forwarder (10.10.5.201):

```
sysctl -e net.ipv4.ip_forward=1
arp -s 10.10.5.100 ff:ff:ff:ff:ff:ff
iptables -A INPUT -p tcp --dport 5555 -d 10.10.5.201 -j TEE --gateway 10.10.5.100
iptables -A OUTPUT -p tcp --sport 5555 -j DROP
```

* LB Servers (10.10.5.153, 10.10.5.172):

```
iptables -t raw -A PREROUTING -p tcp --dport 5555 -d 10.10.5.201 -j NFQUEUE --queue-num 1
iptables -t mangle -A POSTROUTING -p tcp --sport 5555 -j NFQUEUE --queue-num 1
gcc -Wall -O3 -o dropSendMask dropSendMask.c -lnfnetlink -lnetfilter_queue
./dropSendMask
```

### 2. Multicast Loop:

* Tomcat Servers (10.10.5.126, 10.10.5.221):

```
sh ./tomcat/bin/catalina.sh start
```

* Router (10.10.5.201):

```
iptables -A OUTPUT -p tcp --sport 5555 --tcp-flags RST RST -j DROP
gcc -Wall -o forwarder forwarder.c
./forwarder
```

* LB Servers (10.10.5.153, 10.10.5.172):

```
./httpd/bin/httpd -k start
iptables -t raw -A PREROUTING -p tcp --dport 5555 -j NFQUEUE --queue-num 1
iptables -t mangle -A POSTROUTING -p tcp --sport 5555 -j NFQUEUE --queue-num 1
gcc -Wall -O3 -o dropmask dropmask.c -lnfnetlink -lnetfilter_queue
./dropmask
```

### 3. HUB:

* Tomcat Servers (10.10.5.126, 10.10.5.221):

```
sh ./tomcat/bin/catalina.sh start
```

* LB Servers (10.10.5.201, 201):

```
ifconfig eth0 hw ether 00:23:ae:a4:7e:53
ifconfig eth0 10.10.5.201
iptables -t raw -A PREROUTING -p tcp --dport 5555 -j NFQUEUE --queue-num 1
iptables -A OUTPUT -p tcp --dport 8080 --tcp-flags RST RST -j DROP
gcc -Wall -O3 -o dropper dropper.c -lnfnetlink -lnetfilter_queue
./dropper
```

### 4. OPENFLOW: (incomplete)

* Flows:

```
ovs-ofctl add-flow tcp:192.168.2.254:6634 ip,idle_timeout=0,nw_dst=192.168.7.111,actions=output:11
ovs-ofctl add-flow tcp:192.168.2.254:6634 ip,idle_timeout=0,nw_dst=192.168.7.103,actions=output:3
ovs-ofctl add-flow tcp:192.168.2.254:6634 arp,idle_timeout=0,actions=NORMAL
```

* On the Switch:

```
config
vlan 7 openflow listener ptcp:6634 enable
vlan 7 untagged 1-12
```

## Notes:

* Clear iptables:

```
iptables -F -t nat
iptables -F -t mangle
iptables -F -t raw
iptables -F
```

* Useful libs:

`sudo apt-get install libnetfilter-queue-dev libaprutil1-dev`

* Clear route:

```
route del -net 10.101.233.161 netmask 255.255.255.255 gw 10.101.233.104
route del -net 192.168.1.0 netmask 255.255.255.0 gw 10.101.233.104
```

* Default Gateway:

`route add default gw 10.101.233.104`

* Delete arp:

```
ifconfig eth0:1 192.168.1.1 netmask 255.255.255.128
arp -d 192.168.1.3
ifconfig eth0:1 down
``