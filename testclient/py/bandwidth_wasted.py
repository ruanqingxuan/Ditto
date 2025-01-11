# 本文件用于从recv.txt读取收到的数据情况数据并计算带宽浪费
import sys
from collections import Counter


# read data
def read_recv_txt(filename):
    packet_numbers = []
    types = []
    recv_times = []

    with open(filename, "r") as file:
        for line in file:
            data = line.strip().split()  # 去除空格并分割成三列
            if len(data) == 3:  # 确保每行有三个元素
                packet_number, type_, recv_time = data
                packet_numbers.append(int(packet_number))  # 转换为int
                types.append(type_)  # 保持为字符串
                recv_times.append(float(recv_time))  # 转换为float

    return packet_numbers, types, recv_times


def calculate_bandwidth_wasted(packet_numbers, types):
    recv_packet = 0
    redundant_packet = 0
    rows_to_delete = []
    for i in range(len(types)):
        if types[i] == "Epoch.ONE_RTT":
            recv_packet = recv_packet + 1
        else:
            rows_to_delete.append(i)
    # 删除不是appdata的包
    app_packet_numbers = [
        val for i, val in enumerate(packet_numbers) if i not in rows_to_delete
    ]
    # 统计每个数字出现的次数
    counts = Counter(app_packet_numbers)
    # 过滤出出现次数大于1的数字，并打印
    duplicates = {num: count for num, count in counts.items() if count > 1}
    if duplicates:
        for num, count in duplicates.items():
            # 计算冗余包次数
            redundant_packet = redundant_packet + count - 1
    else:
        print("none redundant packet")
    print(f"recv_packet={recv_packet}")
    print(f"redundant_packet={redundant_packet}")
    bandwidth_wasted = float(redundant_packet / recv_packet)
    print(f"bandwidth_wasted={bandwidth_wasted}")
    return bandwidth_wasted


# main
if __name__ == "__main__":
    filename = "../data/recv.txt"  # 替换为你的txt文件路径
    packet_numbers, types, recv_times = read_recv_txt(filename)
    bandwidth_wasted = calculate_bandwidth_wasted(packet_numbers, types)
    # 100次求个平均
    with open(f"../data/bandwidth_wasted.txt", "a") as file:
        print(f"{bandwidth_wasted}", file=file)
