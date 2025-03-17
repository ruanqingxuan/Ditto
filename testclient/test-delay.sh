# 本文档用于测试尾时延
# 0.开启丢包率30%
# 1.测试并记录100次访问30k文件的时延
# 1.1开启Ditto
cd py
# 添加丢包率
ssh -p 2220 fzchen@udpcc-shh2.dfshan.net <<EOF
    sudo tc qdisc add dev eth0 root netem loss 30%
    tc qdisc show dev eth0
    exit
EOF
for i in {1..50}; do
    python3 client.py https://udpcc-shh2.dfshan.net:8000/30k
    sleep 1
done
# 删除丢包率
ssh -p 2220 fzchen@udpcc-shh2.dfshan.net <<EOF
    sudo tc qdisc del dev eth0 root netem 
    tc qdisc show dev eth0
    exit
EOF
ssh qnwang@udpcc.dfshan.net <<EOF
    sudo tc qdisc add dev eth0 root netem loss 10%
    tc qdisc show dev eth0
    exit
EOF
# 1.2不开启Ditto
for i in {1..50}; do
    python3 client.py https://udpcc.dfshan.net:8443/30k
    sleep 1
done
ssh qnwang@udpcc.dfshan.net <<EOF
    sudo tc qdisc del dev eth0 root netem 
    tc qdisc show dev eth0
    exit
EOF
# 2.比较并获得99%分位数的时延
python3 99ca.py
cd ..
# 3.开启Ditto的尾时延提升50%
