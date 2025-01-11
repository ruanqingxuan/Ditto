#!/bin/bash

# 设置网络接口名称，例如 eth0
IFACE="eth0"
RTT="100" #RTT
# 丢包率文件路径
loss_file="data/loss_rate.txt"
duration_file="data/durations.txt"
# 如果配置文件不存在，则生成随机值，要修改要把两个文件都删掉重新生成
if [[ ! -f $loss_file || ! -f $duration_file ]]; then
        echo "Generating random loss rates and durations..."
        # 随机生成丢包率（0%-10%之间）和持续时间（1秒到3秒之间）
        for i in {1..1000}; do
                loss_rate=$(awk -v min=0 -v max=10 'BEGIN{srand(systime() * 1000000 + PROCINFO["pid"]); printf "%.1f", min+rand()*(max-min)}')
                duration=$(awk -v min=0.001 -v max=0.999 'BEGIN{srand(systime()* 1000000 + PROCINFO["pid"]); printf "%.3f", min+rand()*(max-min)}')
                echo $loss_rate $duration
                echo $loss_rate >>$loss_file
                echo $duration >>$duration_file
        done
        echo "Random values saved to $loss_file and $duration_file."
else
        echo "Using existing loss rates and durations from $loss_file and $duration_file."
fi

# 读取和应用丢包率及持续时间
loss_rates=($(cat $loss_file))
durations=($(cat $duration_file))

for ((i = 0; i < ${#loss_rates[@]}; i++)); do
        loss_rate=${loss_rates[i]}
        duration=${durations[i]}
        echo "Setting loss rate to $loss_rate% for $duration seconds..."
        # 这里用 `tc` 模拟网络丢包率
        # sudo tc qdisc del dev eth0 root netem
        sudo tc qdisc add dev "$IFACE" root netem delay 50ms loss $loss_rate%
        # sudo tc qdisc show dev eth0
        # 对齐秒数
        timestamp=$(date +%s.%N | awk '{printf "%.6f", $1}')
        # echo $timestamp
        # 将两个参数输出到txt文件中
        echo "${timestamp} ${loss_rate}" >>../loss_distributed.txt
        sleep $duration
        sudo tc qdisc del dev eth0 root netem

done
# # 循环切换丢包率
# while true; do
#         sudo tc qdisc del dev $IFACE root netem 2>/dev/null
#         # 设置当前丢包率
#         # 丢包率的百分比列表，单位为 %
#         # 检查是否已经有固定的丢包率
#         if [[ -f "$LOSS_FILE" ]]; then
#                 # 从文件读取已固定的丢包率
#                 LOSS=$(cat "$LOSS_FILE")
#         else
#                 # 随机生成丢包率
#                 LOSS1=$((RANDOM % 9))
#                 LOSS2=$(echo "scale=2; $((RANDOM % 10)) * 0.1" | bc)
#                 LOSS=$(echo "$LOSS1+$LOSS2" | bc)

#                 # 保存生成的丢包率到文件
#                 echo "$LOSS" >"$LOSS_FILE"
#         fi
#         LOSS1=$((RANDOM % 9))
#         LOSS2=$(echo "scale=2; $((RANDOM % 10)) * 0.1" | bc)
#         LOSS=$(echo "$LOSS1+$LOSS2" | bc)
#         sudo tc qdisc add dev $IFACE root netem delay $((RTT - 50))ms loss ${LOSS}%
#         # echo "Current packet loss rate set to ${LOSS}%"
#         sudo tc qdisc show dev eth0
#         # 生成一个0到n秒的随机延迟
#         DELAY1=$(shuf -i 0-2 -n 1)
#         # 生成一个0.1到1秒的随机延迟（包含小数）
#         DELAY2=$(echo "scale=2; 0.1 + $((RANDOM % 10)) * 0.1" | bc)
#         DELAY=$(echo "$DELAY1+$DELAY2" | bc)
#         echo "Duration is ${DELAY}s"
#         sleep $DELAY
# done
