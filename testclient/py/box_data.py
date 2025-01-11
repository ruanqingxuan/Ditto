# 本文件用来计算trans_time-expect_time
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


def calculate_box(send, recv, RTT, expect_time, flag):
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
    for value in common_rows:
        # 获取数组a和b中对应行的索引
        index_send = np.where(send_unique[:, 0] == value)[0][0]
        index_recv = np.where(recv_unique[:, 0] == value)[0][0]
        # 计算第三列的差值并保存，单位ms
        diff = (
            recv_unique[index_recv, 2] - send_unique[index_send, 2]
        ) / 1000 + 0.4 * RTT
        print(diff)
        with open(f"../data/test1/{expect_time}_box_data_{flag}.txt", "a") as file:
            print(diff, file=file)


# main
if __name__ == "__main__":
    # 参数
    expect_time = int(sys.argv[1])
    RTT = int(sys.argv[2])
    flag = sys.argv[3]
    # read send_txt
    send_filename = "../data/send.txt"
    recv_filename = "../data/recv.txt"
    send = read_txt(send_filename)
    recv = read_txt(recv_filename)
    calculate_box(send, recv, RTT, expect_time, flag)
