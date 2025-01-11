#!/bin/bash
#本文件用于开启整个AR测试2，包括执行，收集数据
TIME="100" # expect_time
RTT="100"  #RTT
LOSS="change"
FLAG="0"
N="1"
TOKEN="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" # 在GitHub上生成的Personal Access Token git push -f origin main
#将需要重复执行的命令存储在 cmd 变量中 sudo tc qdisc add dev eth0 root netem loss ${LOSS}%
cmd="python3 client.py https://udpcc.dfshan.net:8443/10M" # --ca-certs /home/qnwang/worknew/cert/fullchain.pem
cd py
echo "Running iteration loss change"
# 先清除之前的python进程
# pid=$(ps aux | grep 'python3' | grep -v 'grep' | awk '{print $2}')
# if [ -n "$pid" ]; then
#   echo "Terminating Python process with PID: $pid"
#   sudo kill -9 $pid
# else
#   echo "No Python process found."
# fi
# # 在执行之前打开server ./tengine 设置变化的丢包率，并存入loss_distributed.txt，loss使用和send、recv一样的时间，方便对其时间轴
# ssh -T qnwang@udpcc.dfshan.net <<EOF
# export PATH=$PATH:/usr/bin
# cd /home/qnwang/worknew/AR/tengine-install/sbin
# sudo pkill tengine
# sudo pkill apache2
# ./tengine
# sudo tc qdisc del dev eth0 root netem
# cd /home/qnwang/worknew/AR/testclient
# nohup bash loss_change.sh > /dev/null 2>&1 &
# EOF
# 执行client命令，client端得到recv.txt，server端得到AR.slog
# $cmd
# 上传slog和loss_distributed.txt到github
# pid=\$(ps aux | grep 'bash loss_change.sh' | grep -v grep | awk '{print \$2}')
# if [ -n "\$pid" ]; then
#   echo "Stopping loss_change.sh with PID: \$pid"
#   kill \$pid
# else
#   echo "No running loss_change.sh script found."
# fi
# rm -rf AR.slog
# rm -rf loss_distributed.txt
ssh qnwang@udpcc.dfshan.net <<EOF
export PATH=$PATH:/usr/bin
sudo tc qdisc del dev eth0 root netem
cd /home/qnwang/worknew/AR
git add AR.slog loss_distributed.txt
git commit -m "FLAG="$FLAG""
git push -f https://$TOKEN@github.com/ruanqingxuan/congestion_switching.git main
EOF
#删除client端的AR.slog
cd /home/qnwang/worknew/AR
rm -rf AR.clog
rm -rf AR.slog
# 从github上下载server端的AR.slog，通过ssh
# git clone https://github.com/ruanqingxuan/congestion_switching.git
git clone git@github.com:ruanqingxuan/congestion_switching.git
cd congestion_switching
mv AR.slog /home/qnwang/worknew/AR
mv loss_distributed.txt /home/qnwang/worknew/AR/testclient/data
cd ..
rm -rf congestion_switching
# 开始执行分析程序
cd testclient/py
# 读取AR.slog，得到csv文件
python3 read_arslog.py
# 把csv文件抽取想要的部分，得到send.txt
python3 save_send.py
# 计算每包的传输时延和对应收到该包时的时间 trans_time+recv_time
python3 trans_time.py "$TIME" "$RTT" "$FLAG"
# 计算带宽浪费，得到整个运行过程的bandwidth_wasted.txt
python3 bandwidth_wasted.py
python3 avg_time.py "$TIME" "$RTT" "$LOSS" "$N" "$FLAG"
# 删除中间数据
rm -rf ../data/bandwidth_wasted.txt
# rm -rf ../data/recv.txt
# rm -rf ../data/loss_distributed.txt
# 画图，画双纵轴图，待做
python3 plot.py 2
