#!/bin/bash
#本文件用于开启整个AR测试1，包括执行，收集数据
TIME="100" # expect_time
RTT="100"  #RTT
LOSS="50"  # 10 15 20 25 30
FLAG="1"
N="2"
TOKEN="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" # 在GitHub上生成的Personal Access Token git push -f origin main
#将需要重复执行的命令存储在 cmd 变量中 sudo tc qdisc add dev eth0 root netem loss ${LOSS}%
cmd="python3 client.py https://udpcc.dfshan.net:8443/30k" # --ca-certs /home/qnwang/worknew/cert/fullchain.pem
cd py
# 使用 for 循环执行该命令 n 次
for ((i = 1; i <= $N; i++)); do
    echo "Running iteration $i"
    # 执行client命令，client端得到recv.txt，server端得到AR.slog
    pid=$(ps aux | grep 'python3' | grep -v 'grep' | awk '{print $2}')
    if [ -n "$pid" ]; then
        echo "Terminating Python process with PID: $pid"
        sudo kill -9 $pid
    else
        echo "No Python process found."
    fi
    # 在执行之前打开server ./tengine 设置丢包率
    ssh -T qnwang@udpcc.dfshan.net <<EOF
    export PATH=$PATH:/usr/bin
    cd /home/qnwang/worknew/AR/tengine-install/sbin
    sudo pkill tengine
    sudo pkill apache2
    ./tengine
    sudo tc qdisc del dev eth0 root netem
    sudo tc qdisc add dev eth0 root netem delay $((RTT - 30))ms loss ${LOSS}%
    sudo tc qdisc show dev eth0
EOF
    $cmd
    # 上传slog到github
    ssh qnwang@udpcc.dfshan.net <<EOF
    export PATH=$PATH:/usr/bin
    sudo tc qdisc del dev eth0 root netem
    cd /home/qnwang/worknew/AR
    sudo tc qdisc show dev eth0
    git add AR.slog
    git commit -m "TIME="$TIME""
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
    cd ..
    rm -rf congestion_switching
    # 开始执行分析程序
    cd testclient/py
    # 读取AR.slog，得到csv文件
    python3 read_arslog.py
    # 把csv文件抽取想要的部分，得到send.txt
    python3 save_send.py
    # 计算传输时间分布，得到box_data.txt
    python3 box_data.py "$TIME" "$RTT" "$FLAG"
    # # 传输时延分布挑一个写就行了
    # if [ "$N" -eq 1 ]; then
    #     python3 success_distributed.py "$LOSS" "$TIME" "$RTT" "$FLAG"
    # fi
    # 计算带宽浪费，得到bandwidth_wasted.txt
    python3 bandwidth_wasted.py
    rm -rf ../data/recv.txt
done
# 用于取100次的平均值
python3 avg_time.py "$TIME" "$RTT" "$LOSS" "$N" "$FLAG"
# 已完成图的一个点，删除相关文件，准备下一轮
# rm -rf ../data/success_rate.txt
rm -rf ../data/bandwidth_wasted.txt
# 画图，box_plot
# python3 plot.py 1
# pdb
# python3 -m pdb client.py https://udpcc.dfshan.net:8443/30k --ca-certs /home/qnwang/worknew/cert/fullchain.pem
