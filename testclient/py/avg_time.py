# 本文件用于求100次下载时间的平均值

import sys
import math


def calculate_average(input_file, output_file, n, test, loss):
    # 初始化变量
    total = 0
    count = 0

    # 读取文件中的前n行数据并求和
    with open(input_file, "r") as infile:
        for i, line in enumerate(infile):
            if i < n:
                try:
                    # 累加每行的数值
                    total += float(line.strip())
                    count += 1
                except ValueError:
                    print(f"Line {i+1} is not a valid number.")
            else:
                break

    # 计算平均值
    average = total / count if count > 0 else 0

    # 将平均值写入新的文件
    with open(output_file, "a") as outfile:
        # outfile.write(f"Average of first {n} lines: {average:.5f}\n")
        outfile.write(f"{test} {loss}% {average:.5f}\n")


# 从命令行获取传入的参数
expect_time = float(sys.argv[1])
RTT = int(sys.argv[2])
test = math.floor(expect_time / RTT)
loss = sys.argv[3]
n = int(sys.argv[4])  # 要读取的行数
flag = int(sys.argv[5])
print("expect_time:", expect_time)
print("test:", test)
print("loss:", loss)
print("n:", n)
print("flag:", flag)
# test2, loss change
if loss == "change":
    # AR
    if flag == 1:
        # bandwidth_wasted-AR
        bandwidth_wasted_input_file = "../data/bandwidth_wasted.txt"  # 源文件名
        bandwidth_wasted_output_file = (
            "../data/test2/avg_bandwidth_wasted_ar.txt"  # 输出文件名
        )
        # 计算带宽浪费均值
        calculate_average(
            bandwidth_wasted_input_file, bandwidth_wasted_output_file, n, test, loss
        )
        # success_rate-AR
        # success_rate_input_file = "../data/success_rate.txt"
        # success_rate_output_file = "../data/avg_success_rate_ar2.txt"
    # else:
    # success_rate-non AR
    # success_rate_input_file = "../data/success_rate.txt"
    # success_rate_output_file = "../data/avg_success_rate2.txt"
    # 计算成功率均值
    # calculate_average(success_rate_input_file, success_rate_output_file, n, test, loss)
else:  # test1, loss unchanged
    # AR
    if flag == 1:
        # bandwidth_wasted-AR
        bandwidth_wasted_input_file = "../data/bandwidth_wasted.txt"  # 源文件名
        bandwidth_wasted_output_file = (
            "../data/test1/avg_bandwidth_wasted_ar.txt"  # 输出文件名
        )
        # 计算带宽浪费均值
        calculate_average(
            bandwidth_wasted_input_file, bandwidth_wasted_output_file, n, test, loss
        )
        # success_rate-AR
        # success_rate_input_file = "../data/success_rate.txt"
        # success_rate_output_file = "../data/avg_success_rate_ar1.txt"
    # else:
    # success_rate-non AR
    # success_rate_input_file = "../data/success_rate.txt"
    # success_rate_output_file = "../data/avg_success_rate1.txt"
    # 计算成功率均值
    # calculate_average(success_rate_input_file, success_rate_output_file, n, test, loss)
