# 本文件用来计算trans_time-time
import csv
import sys
import math
import numpy as np
import pandas as pd


# read data
def read_data_txt(filename):
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


def read_loss_txt(filename):
    loss_times = []
    loss_rate = []

    with open(filename, "r") as file:
        for line in file:
            data = line.strip().split()  # 去除空格并分割成两列
            if len(data) == 2:  # 确保每行有两个元素
                loss_time, loss = data
                loss_times.append(float(loss_time))  # 转换为float
                loss_rate.append(float(loss))  # 保持为字符串

    data = np.column_stack((loss_times, loss_rate))
    return data


def is_third_column_sorted(juzhen):
    # 检查数组是否为空或列数不足3
    # if not juzhen or len(juzhen[0]) < 3:
    #     return False

    # 遍历第三列
    for i in range(1, len(juzhen)):
        if juzhen[i][2] < juzhen[i - 1][2]:
            print(juzhen[i - 1][2])
            return False
    return True


def calculate_trans(send, recv, loss, RTT, expect_time, flag):
    # 取第一个收到包的时间为time起始值，单位s
    time_start = float(recv[0, 2])
    print(f"time_start={time_start}")
    # 对loss进行处理，时间对齐
    loss_df = pd.DataFrame(loss, columns=["Col1", "Col2"])
    loss_df["Col1"] = pd.to_numeric(loss_df["Col1"], errors="coerce")
    loss_df["Col1"] = loss_df["Col1"]
    loss_unique = loss_df.to_numpy()
    # 1. 减去 time_start
    adjusted_first_column = loss_unique[:, 0] - time_start
    # 2. 找到最大的负数
    max_negative = (
        np.max(adjusted_first_column[adjusted_first_column < 0])
        if np.any(adjusted_first_column < 0)
        else None
    )
    # 3. 将最大的负数置为0，其他负数所在的行删除
    if max_negative is not None:
        # 将最大的负数替换为0
        adjusted_first_column[adjusted_first_column == max_negative] = 0
        # 找到非负数的行以及被置为0的行
        mask = (adjusted_first_column >= 0) | (adjusted_first_column == 0)
    else:
        # 如果没有负数，仅保留非负数
        mask = adjusted_first_column >= 0
    # 4. 更新 loss_unique，仅保留符合条件的行
    loss_unique = np.column_stack((adjusted_first_column, loss_unique[:, 1]))[mask]
    # 将数组保存为 txt 文件
    np.savetxt(
        f"../data/test2/{expect_time}_loss_data_{flag}.txt",
        loss_unique,
        fmt="%.2f",
        delimiter=" ",
    )
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
    # 将send第三列除以10^6，单位换成s
    send_df["Col3"] = send_df["Col3"] / 1e6
    # 删除第一列'Col1'中的重复行，仅保留最前面一行
    recv_unique = recv_df.drop_duplicates(subset="Col1", keep="first")
    # 转换为 NumPy 数组
    send_unique = send_df.to_numpy()
    recv_unique = recv_unique.to_numpy()
    # 筛选数组send和数组recv第一列相同的行，找出同一个包
    common_rows = np.intersect1d(
        send_unique[:, 0], recv_unique[:, 0]
    )  # 找出共同的第一列值，这里会乱序
    print(f"common_rows={len(common_rows)}")
    # 时间，传输时延
    trans_times = []
    # 传输时延
    for value in common_rows:
        # 获取数组a和b中对应行的索引
        index_send = np.where(send_unique[:, 0] == value)[0][0]
        index_recv = np.where(recv_unique[:, 0] == value)[0][0]
        # 计算第三列的差值并保存，单位ms
        diff = (
            recv_unique[index_recv, 2] - send_unique[index_send, 2]
        ) * 1000 + 0.4 * RTT
        time = recv_unique[index_recv, 2] - time_start
        trans_row = [time, diff]
        trans_times.append(trans_row)
    # 按 time (第一列) 从小到大排序
    trans_times.sort(key=lambda x: x[0])
    trans_times = np.array(trans_times)
    if flag == 1:
        trans_times = trans_times[trans_times[:, 1] < 300]
    # else:
    #     trans_times = trans_times[trans_times[:, 1] < 1000]
    trans_times = trans_times[trans_times[:, 1] > 90]
    np.savetxt(
        f"../data/test2/{expect_time}_trans_data_{flag}.txt",
        trans_times,
        fmt="%.2f",
        delimiter=" ",
    )


# main
if __name__ == "__main__":
    # 参数
    expect_time = int(sys.argv[1])
    RTT = int(sys.argv[2])
    flag = sys.argv[3]
    # read send_txt
    send_filename = "../data/send.txt"
    recv_filename = "../data/recv.txt"
    loss_filename = "../data/loss_distributed.txt"
    send = read_data_txt(send_filename)
    recv = read_data_txt(recv_filename)
    loss = read_loss_txt(loss_filename)
    calculate_trans(send, recv, loss, RTT, expect_time, flag)
