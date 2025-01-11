#!/bin/bash

# 设置网络接口名称，例如 eth0
IFACE="eth0"
# 丢包率的百分比列表，单位为 %
LOSS_RATES=25 #10 15 20 25 30  
# 初始化 tc，删除先前的配置（若有）
sudo tc qdisc del dev $IFACE root netem 2>/dev/null
sudo tc qdisc add dev $IFACE root netem loss ${LOSS_RATES}%
sudo tc qdisc show dev eth0
DELAY=500
echo "Duration is ${DELAY}s"
sleep $DELAY
sudo tc qdisc del dev $IFACE root netem 2>/dev/null
# sudo tc qdisc del dev eth0 root netem 
