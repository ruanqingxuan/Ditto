# 本文件用来计算成功率
import csv
import sys
import math
import numpy as np
import pandas as pd


# read data
def read_txt(filename):
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

    data = np.column_stack((packet_numbers, types, recv_times))
    return data


def calculate_success_rate(send, recv, RTT, expect_time):
    # 第一件事就是把send和recv中不需要的参数去掉
    send_value_to_save = "XQC_PNS_APP_DATA"
    recv_value_to_save = "Epoch.ONE_RTT"
    # 使用布尔索引保留第二列值为 `value_to_remove` 的行
    send_array = send[send[:, 1] == send_value_to_save]
    recv_array = recv[recv[:, 1] == recv_value_to_save]
    print(f"send_array={len(send_array)} recv_array={len(recv_array)}")
    # 将数据加载为DataFrame
    send_df = pd.DataFrame(send_array, columns=["Col1", "Col2", "Col3"])
    recv_df = pd.DataFrame(recv_array, columns=["Col1", "Col2", "Col3"])
    # 将第三列转换为浮点数
    send_df["Col3"] = pd.to_numeric(send_df["Col3"], errors="coerce")
    recv_df["Col3"] = pd.to_numeric(recv_df["Col3"], errors="coerce")
    # 将第三列乘以10^6
    recv_df["Col3"] = recv_df["Col3"] * 1e6
    # 删除第一列'Col1'中的重复行，仅保留最前面一行
    recv_unique = recv_df.drop_duplicates(subset="Col1", keep="first")
    # 转换为 NumPy 数组
    send_unique = send_df.to_numpy()
    recv_unique = recv_unique.to_numpy()
    # 筛选数组send和数组recv第一列相同的行，找出同一个包
    common_rows = np.intersect1d(
        send_unique[:, 0], recv_unique[:, 0]
    )  # 找出共同的第一列值
    print(f"common_rows={len(common_rows)}")
    # 传输时延
    time = []
    for value in common_rows:
        # 获取数组a和b中对应行的索引
        index_send = np.where(send_unique[:, 0] == value)[0][0]
        index_recv = np.where(recv_unique[:, 0] == value)[0][0]
        # 计算第三列的差值并保存，单位ms
        diff = (
            recv_unique[index_recv, 2] - send_unique[index_send, 2]
        ) / 1000 + 0.4 * RTT
        print(diff)
        time.append(diff)
    # 转换为一维数组
    time = np.array(time)
    t = math.floor(expect_time / RTT)
    counts = {}
    count_ex = 0
    for i in range(1, t + 1):  # 限定范围
        # 使用字典存储动态变量
        counts[f"count_{i}"] = 0  # 动态生成计数器名称
    # 计算成功率+传输时延分布
    for value in time:
        if value < expect_time:
            for i in range(1, t + 1):  # 限定范围
                if (i - 1) * RTT < value < i * RTT:
                    counts[f"count_{i}"] += 1
        else:
            count_ex += 1
    print(counts)
    print(f"count_ex={count_ex} count={len(time)}")
    # 计算成功率
    successful = 1 - count_ex / len(time)
    pros = []
    # 计算传输时延分布
    for i in range(1, t + 1):
        pro = counts[f"count_{i}"] / len(time)
        pros.append(pro)
        print(f"{i-1}RTT<value<{i}RTT {pro}")
    for i in range(len(pros)):
        prob = 0
        for j in range(0, i + 1):
            prob += pros[j]
        print(f"value<{i}RTT {prob}")
    # 100次中的一次
    with open(f"../data/test1/success_rate.txt", "a") as file:
        print(successful, file=file)
    print(successful)


# main
if __name__ == "__main__":
    # 参数
    expect_time = float(sys.argv[1])
    RTT = int(sys.argv[2])
    # read send_txt
    send_filename = "../data/send.txt"
    recv_filename = "../data/recv.txt"
    send = read_txt(send_filename)
    recv = read_txt(recv_filename)
    calculate_success_rate(send, recv, RTT, expect_time)
