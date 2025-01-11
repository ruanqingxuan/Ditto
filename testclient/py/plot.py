# 本文件用于画图
import matplotlib.pyplot as plt
from matplotlib import gridspec
import seaborn as sns
import numpy as np
import os
import sys
import pandas as pd
from matplotlib.ticker import FixedLocator
from matplotlib.collections import LineCollection

plt.style.use("/home/qnwang/worknew/AR/testclient/picture/paper.mplstyle")

# line styles
styles = [
    {
        "color": "#a00000",
        "linestyle": "-",
        "marker": "o",
        "markevery": 1,
        "markersize": 12,
        "markerfacecolor": "None",
    },
    {
        "color": "#00a000",
        "linestyle": "-",
        "marker": "x",
        "markevery": 1,
        "markerfacecolor": "None",
    },
    {
        "color": "#5060d0",
        "linestyle": "-",
        "marker": ">",
        "markersize": 12,
        "markevery": 1,
    },
    {
        "color": "#f25900",
        "linestyle": "-",
        "marker": "+",
        "markevery": 1,
        "markersize": 12,
        "markerfacecolor": "None",
    },
    {
        "color": "#500050",
        "linestyle": "-",
        "marker": "*",
        "markevery": 1,
        "markersize": 20,
    },
]


def plot_data_trans_time(trans_file, loss_file, flag):

    # 读取 trans_time-time 数据
    trans_time_data = np.loadtxt(trans_file)
    time_trans = trans_time_data[:, 0]
    trans_time = trans_time_data[:, 1]

    # 读取 loss-time 数据
    loss_data = np.loadtxt(loss_file)
    time_loss = loss_data[:, 0]
    loss_rate = loss_data[:, 1]

    # 构造 step-wise 数据（保持丢包率在区间内的值）
    loss_time_extended = []
    loss_rate_extended = []
    for i in range(len(time_loss) - 1):
        loss_time_extended.append(time_loss[i])
        loss_time_extended.append(time_loss[i + 1])
        loss_rate_extended.append(loss_rate[i])
        loss_rate_extended.append(loss_rate[i])

    # 确保最后一个时间点也在内
    loss_time_extended.append(time_loss[-1])
    loss_rate_extended.append(loss_rate[-1])

    # 设置显示中文字体
    # 设置正常显示符号
    # sns.set_style({"font.sans-serif": ["simsun", "Times New Roman"]})
    plt.rcParams["font.sans-serif"] = ["SimSun"]
    plt.rcParams["axes.unicode_minus"] = False

    # 绘制折线图
    fig, ax = plt.subplots(figsize=(12, 8))
    # 创建双纵轴图
    fig, ax1 = plt.subplots()

    # 第一条折线图 trans_time-time
    ax1.plot(
        time_trans,
        trans_time,
        label="每包传输时延",
        color="royalblue",
        linewidth=2,
    )
    ax1.set_xlabel("时间(s)", fontsize=20)
    ax1.set_xticks(range(0, 11, 2))  # 设置x轴刻度为0, 2, 4, ..., 10
    ax1.tick_params(axis="x", labelsize=20)
    ax1.set_ylabel("每包传输时延(ms)", fontsize=20)
    # 调整y轴刻度标签的字体大小
    ax1.tick_params(axis="y", labelsize=20)

    # 第二条折线图 loss-time
    ax2 = ax1.twinx()  # 创建共享x轴的第二个y轴
    ax2.step(
        loss_time_extended,
        loss_rate_extended,
        label="丢包率",
        color="tomato",
        where="post",
        linewidth=2,
    )
    ax2.set_ylabel("丢包率(%)", fontsize=20)
    ax2.tick_params(axis="y", labelsize=20)

    # 自动布局，画子图
    plt.xlim(0, 10)
    # 设置图例位置和大小
    # 设置图例：放置在左侧并上下排列
    # ax1.legend(
    #     loc="upper right",
    #     fontsize=16,
    #     frameon=True,
    #     markerscale=2,
    #     ncol=1,
    #     bbox_to_anchor=(1, 1),
    # )
    # ax2.legend(
    #     loc="upper right",
    #     fontsize=16,
    #     frameon=True,
    #     markerscale=2,
    #     ncol=1,
    #     bbox_to_anchor=(1, 1),
    # )
    # 合并两个图例
    handles, labels = [], []
    for ax in [ax1, ax2]:
        for handle, label in zip(*ax.get_legend_handles_labels()):
            handles.append(handle)
            labels.append(label)

    # 设置最终的统一图例
    fig.legend(
        handles,
        labels,
        loc="upper right",
        fontsize=15,
        frameon=True,
        markerscale=2,
        ncol=1,
        bbox_to_anchor=(0.865, 0.945),
    )

    plt.grid(True)
    plt.tight_layout()
    plt.show(block=False)
    print("show test2plot")
    if flag == 1:
        plt.savefig("../picture/artrans.png", dpi=300, bbox_inches="tight")
    else:
        plt.savefig("../picture/nonartrans.png", dpi=300, bbox_inches="tight")

    # 设置标题和标签
    # plt.title("10k file download啦啦啦")
    # plt.xlabel("Packet Loss Rate (%)")
    # plt.ylabel("Download Completion Time (s)")
    # plt.legend()
    # plt.grid(True)


def plot_box_bar(no_ar_data, ar_data, standard):
    plt.rcParams["font.sans-serif"] = ["SimSun"]
    # 处理数据
    no_ar_data = [x for x in no_ar_data if 100 < x < 1000]
    ar_data = [x for x in ar_data if 100 < x < 600]

    # 创建绘图区域
    fig, (ax1, ax2) = plt.subplots(
        1, 2, figsize=(24, 12), gridspec_kw={"width_ratios": [1, 2]}
    )
    # Create subplots
    ax1.plot([0, 1], [0, 1])
    ax1.plot([0, 1], [0, 2])

    # 绘制箱线图
    sns.boxplot(
        data=[ar_data, no_ar_data],
        ax=ax1,
        showmeans=True,
        medianprops={"color": "blue", "linewidth": 6},  # 中位线
        meanprops=dict(
            marker="^",
            markerfacecolor="deepskyblue",
            markeredgecolor="deepskyblue",
            markersize=12,
        ),  # 调整均值样式颜色
        boxprops=dict(edgecolor="black", facecolor="lightblue", linewidth=4),  # 箱体
        patch_artist=True,  # 启用填充
        whiskerprops={  # 设置须的线条属性
            "linestyle": "--",
            "linewidth": 4,
            "color": "#480656",
        },
        capprops=dict(color="black", linewidth=4),  # 上下边界颜色和线粗
        flierprops=dict(
            marker="o",  # 异常值形状
            color="skyblue",  # 异常值边框颜色
            markerfacecolor="skyblue",  # 异常值填充颜色
            markersize=12,
        ),  # 异常值大小
    )

    # 设置标题和坐标轴标签
    # plt.title("Boxplot from Files", fontsize=14)
    # 设置x轴的刻度位置
    ax1.set_xticks([0, 1])  # 例如设置三个刻度位置
    # 设置对应的x轴标签
    ax1.set_xticklabels(["开启AR", "不开启AR"])  # 标签数量与刻度数量相同
    # 调整y轴刻度标签的字体大小
    ax1.tick_params(axis="x", labelsize=48)
    ax1.tick_params(axis="y", labelsize=48)
    # ax1.set_ylim(0, 800)  # 设置y轴范围
    # ax1.set_yticks(np.arange(0, 850, 100))  # 自定义刻度间隔

    # 绘制偏差图
    no_ar_data = np.random.choice(no_ar_data, 20, replace=False)
    ar_data = np.random.choice(ar_data, 20, replace=False)
    # 偏差计算
    # ar_deviations = [d - standard for d in ar_data]  # 偏差（与100ms目标比较）
    # no_ar_deviations = [d - standard for d in no_ar_data]  # 偏差（与100ms目标比较）
    # ar_colors = [
    #     "navy" if d > 0 else "plum" for d in ar_deviations
    # ]  # 超出部分红色，低于部分灰色
    # no_ar_colors = ["darkorange" if d > 0 else "gray" for d in no_ar_deviations]
    ax2.bar(range(len(ar_data)), ar_data, color="navy", label="开启AR")
    ax2.bar(
        range(len(no_ar_data)),
        no_ar_data,
        bottom=ar_data,
        color="darkorange",
        label="不开启AR",
    )  # Stacked on top of data1
    ax2.legend()
    ax2.tick_params(axis="x", labelsize=48)
    ax2.tick_params(axis="y", labelsize=48)
    ax2.set_xticks(
        [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]
    )
    ax2.set_xticklabels(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]
    )  # 标签数量与刻度数量相同

    # 显示图形
    plt.tight_layout()
    plt.subplots_adjust(wspace=0.2)  # 设置子图之间的水平间距
    plt.show()
    plt.savefig(f"../picture/{standard}-deviation.png", dpi=300, bbox_inches="tight")


def plot_bar(ar_file_paths, nonar_file_paths):
    plt.rcParams["font.sans-serif"] = ["SimSun"]
    # 用于存储每个文件中的数据
    ar_data = []
    nonar_data = []
    # 读取每个文件的数据
    for file_path in ar_file_paths:
        if os.path.exists(file_path):
            with open(file_path, "r") as f:
                # 将每行的数据读为 float 并存入列表
                ar_file_data = [float(line.strip()) for line in f if line.strip()]
                ar_file_data = [x for x in ar_file_data if 100 < x < 600]
                average = sum(ar_file_data) / len(ar_file_data)
                ar_data.append(average)
        else:
            print(f"Warning: {file_path} does not exist. Appending empty data.")
            ar_data.append([])  # 如果文件不存在，添加空数据以占位
    print(ar_data)
    for file_path in nonar_file_paths:
        if os.path.exists(file_path):
            with open(file_path, "r") as f:
                # 将每行的数据读为 float 并存入列表
                nonar_file_data = [float(line.strip()) for line in f if line.strip()]
                nonar_file_data = [x for x in nonar_file_data if x < 1000]
                average = sum(nonar_file_data) / len(nonar_file_data)
                nonar_data.append(average)
        else:
            print(f"Warning: {file_path} does not exist. Appending empty data.")
            nonar_data.append([])  # 如果文件不存在，添加空数据以占位
    print(nonar_data)
    plt.figure(figsize=(8, 6))
    # 创建柱状图
    plt.bar(
        range(len(ar_data)),
        ar_data,
        edgecolor="orange",
        label="开启AR",
        linewidth=2.5,
        color="white",  # White fill color
        hatch="//",
        width=0.5,
    )
    plt.bar(
        range(len(nonar_data)),
        nonar_data,
        edgecolor="yellowgreen",
        label="不开启AR",
        linewidth=2.5,
        color="white",  # White fill color
        hatch="\\",
        width=0.5,
    )  # Stacked on top of data1

    plt.legend()
    plt.tick_params(axis="x", labelsize=24)
    plt.tick_params(axis="y", labelsize=24)
    # 添加标题和标签
    plt.xlabel("用户定义的每包传输时延(ms)", fontsize=24)
    plt.ylabel("平均每包实际传输时延(ms)", fontsize=24)
    # 横坐标标签
    x = np.arange(5)
    plt.xticks(ticks=x, labels=[100, 200, 300, 400, 500], fontsize=24)
    # 显示图例
    plt.legend(loc="upper left", fontsize=18)
    plt.show()
    plt.savefig("../picture/bar.png", dpi=300, bbox_inches="tight")


def plot_bar2(ar_file_paths, nonar_file_paths):
    plt.rcParams["font.sans-serif"] = ["SimSun"]
    # 用于存储每个文件中的数据
    ar_data = []
    nonar_data = []
    # 读取每个文件的数据
    for file_path in ar_file_paths:
        if os.path.exists(file_path):
            with open(file_path, "r") as f:
                # 将每行的数据读为 float 并存入列表
                ar_file_data = [float(line.strip()) for line in f if line.strip()]
                ar_file_data = [x for x in ar_file_data if 100 < x < 600]
                average = sum(ar_file_data) / len(ar_file_data)
                ar_data.append(average)
        else:
            print(f"Warning: {file_path} does not exist. Appending empty data.")
            ar_data.append([])  # 如果文件不存在，添加空数据以占位
    print(ar_data)
    for file_path in nonar_file_paths:
        if os.path.exists(file_path):
            with open(file_path, "r") as f:
                # 将每行的数据读为 float 并存入列表
                nonar_file_data = [float(line.strip()) for line in f if line.strip()]
                nonar_file_data = [x for x in nonar_file_data if x < 1000]
                average = sum(nonar_file_data) / len(nonar_file_data)
                nonar_data.append(average)
        else:
            print(f"Warning: {file_path} does not exist. Appending empty data.")
            nonar_data.append([])  # 如果文件不存在，添加空数据以占位
    print(nonar_data)
    plt.figure(figsize=(8, 6))
    # 创建并列柱状图
    fig, ax = plt.subplots()
    width = 0.4  # 柱的宽度
    x = np.arange(5)
    plt.xticks(ticks=x, labels=[100, 200, 300, 400, 500], fontsize=20)
    ax.bar(
        x - width / 2,
        ar_data,
        edgecolor="orange",
        linewidth=2.5,
        color="white",  # White fill color
        label="开启AR",
        hatch="//",
        width=width,
    )
    ax.bar(
        x + width / 2,
        nonar_data,
        label="不开启AR",
        edgecolor="yellowgreen",
        linewidth=2.5,
        color="white",  # White fill color
        hatch="\\",
        width=width,
    )
    plt.legend()
    plt.tick_params(axis="x", labelsize=20)
    plt.tick_params(axis="y", labelsize=20)
    # 添加标题和标签
    plt.xlabel("用户定义的每包传输时延(ms)", fontsize=20)
    plt.ylabel("平均每包实际传输时延(ms)", fontsize=20)
    plt.ylim(0, 290)  # 设置y轴范围
    plt.yticks(np.arange(0, 300, 50))  # 自定义刻度间隔
    # 横坐标标签
    x = np.arange(5)
    plt.xticks(ticks=x, labels=[100, 200, 300, 400, 500], fontsize=20)
    # 显示图例
    plt.legend(loc="upper left", fontsize=18)
    plt.show()
    plt.savefig("../picture/bar2.png", dpi=300, bbox_inches="tight")


def adjacent_values(vals, q1, q3):
    upper_adjacent_value = q3 + (q3 - q1) * 1.5
    upper_adjacent_value = np.clip(upper_adjacent_value, q3, vals[-1])

    lower_adjacent_value = q1 - (q3 - q1) * 1.5
    lower_adjacent_value = np.clip(lower_adjacent_value, vals[0], q1)
    return lower_adjacent_value, upper_adjacent_value


def plot_box_trans_time(file_paths, flag):

    #! 解决不显示的问题：中文设置为宋体格式
    # plt.rcParams["font.family"] = ["Times New Roman", "SimSun"]
    plt.rcParams["font.sans-serif"] = ["SimSun"]

    # 用于存储每个文件中的数据
    data = []

    # 读取每个文件的数据
    for file_path in file_paths:
        if os.path.exists(file_path):
            with open(file_path, "r") as f:
                # 将每行的数据读为 float 并存入列表
                file_data = [float(line.strip()) for line in f if line.strip()]
                if flag == 1:
                    file_data = [x for x in file_data if 100 < x < 600]
                else:
                    file_data = [x for x in file_data if x < 1000]
                data.append(file_data)
        else:
            print(f"Warning: {file_path} does not exist. Appending empty data.")
            data.append([])  # 如果文件不存在，添加空数据以占位

    # 绘制箱线图
    plt.figure(figsize=(12, 8))
    # plt.boxplot(
    #     data,
    #     x_labels=x_labels,
    #     showmeans=True,
    #     medianprops={"color": "blue", "linewidth": 2.2},  # 中位线
    #     meanprops=dict(
    #         marker="^", markerfacecolor="deepskyblue", markeredgecolor="deepskyblue"
    #     ),  # 调整均值样式颜色
    #     boxprops=dict(color="black", facecolor="lightblue", linewidth=1.5),  # 箱体
    #     patch_artist=True,  # 启用填充
    #     whiskerprops={  # 设置须的线条属性
    #         "linestyle": "--",
    #         "linewidth": 1.2,
    #         "color": "#480656",
    #     },
    #     capprops=dict(color="black", linewidth=1.5),  # 上下边界颜色和线粗
    #     flierprops=dict(
    #         marker="o",  # 异常值形状
    #         color="skyblue",  # 异常值边框颜色
    #         markerfacecolor="skyblue",  # 异常值填充颜色
    #         markersize=6,
    #     ),  # 异常值大小
    # )
    parts = plt.violinplot(
        data,
        showmedians=True,
        showextrema=False,
    )
    if flag == 1:
        # 修改平均值和中位值的颜色
        for pc in parts["bodies"]:
            pc.set_facecolor("lightskyblue")
            pc.set_edgecolor("none")
            pc.set_alpha(0.5)  # 透明度
    else:
        for pc in parts["bodies"]:
            pc.set_facecolor("lightcoral")
            pc.set_edgecolor("none")
            pc.set_alpha(0.5)
        # 设置中位线颜色
        # 获取小提琴图的中位数线，并设置其边缘颜色为红色
        vmedian = parts["cmedians"]
        vmedian.set_edgecolor("brown")

    # 设置中位线的颜色
    quartile1 = []
    medians = []
    quartile3 = []
    for arr in data:
        q1, median, q3 = np.percentile(arr, [25, 50, 75])  # 计算Q1、Q2、Q3
        quartile1.append(q1)
        medians.append(median)
        quartile3.append(q3)
    whiskers = np.array(
        [
            adjacent_values(sorted_array, q1, q3)
            for sorted_array, q1, q3 in zip(data, quartile1, quartile3)
        ]
    )
    whiskers_min, whiskers_max = whiskers[:, 0], whiskers[:, 1]

    inds = np.arange(1, len(medians) + 1)
    # plt.vlines(inds, medians, marker="-", color="blue", s=30, zorder=3)
    # plt.vlines(inds, quartile1, quartile3, color="k", linestyle="-", lw=5)
    # plt.vlines(inds, whiskers_min, whiskers_max, color="k", linestyle="-", lw=1)
    # 设置标题和坐标轴标签
    plt.xlabel("用户定义的每包传输时延(ms)", fontsize=28)
    plt.ylabel("每包实际传输时延(ms)", fontsize=28)

    # 设置纵坐标最大值为600
    if flag == 1:
        plt.ylim(0, 500)  # 设置y轴范围
        plt.yticks(np.arange(0, 600, 100), fontsize=28)  # 自定义刻度间隔
    else:
        plt.ylim(0, 700)  # 设置y轴范围
        plt.yticks(np.arange(0, 750, 100), fontsize=28)  # 自定义刻度间隔

    # 增加次要刻度线，更好显示低值区域
    # plt.yscale('symlog', linthresh=50)  # 对称对数尺度，集中于 100 左右
    # plt.grid(True, which='both', linestyle='--', linewidth=0.5, alpha=0.7)

    # 设置 x 轴刻度
    x = [1, 2, 3, 4, 5]
    plt.xticks(ticks=x, labels=[100, 200, 300, 400, 500], fontsize=28)

    # 显示图形
    plt.tight_layout()
    plt.grid(axis="y", linestyle="--", alpha=0.7)
    plt.show(block=False)
    print("show test1plot")
    if flag == 1:
        plt.savefig("../picture/ar_box.png", dpi=300, bbox_inches="tight")
    else:
        plt.savefig("../picture/nonar_box.png", dpi=300, bbox_inches="tight")


# main
if __name__ == "__main__":
    test = int(sys.argv[1])  # 文件路径列表
    if test == 1:
        # 假设文件路径和标准时延
        ar_file_paths = [
            "../data/test1/100_box_data_1.txt",
            "../data/test1/200_box_data_1.txt",
            "../data/test1/300_box_data_1.txt",
            "../data/test1/400_box_data_1.txt",
            "../data/test1/500_box_data_1.txt",
        ]
        nonar_file_paths = [
            "../data/test1/100_box_data_0.txt",
            "../data/test1/200_box_data_0.txt",
            "../data/test1/300_box_data_0.txt",
            "../data/test1/400_box_data_0.txt",
            "../data/test1/500_box_data_0.txt",
        ]
        # 使用画图格式插件
        # plot_box_trans_time(ar_file_paths, 1)
        plot_box_trans_time(nonar_file_paths, 0)
        # standard_delays = [100, 200, 300, 400, 500]
        # for index, standard in enumerate(standard_delays):
        #     with open(f"../data/test1/{standard}_box_data_0.txt", "r") as file:
        #         no_ar_data = [float(line.strip()) for line in file.readlines()]
        #     with open(f"../data/test1/{standard}_box_data_1.txt", "r") as file:
        #         ar_data = [float(line.strip()) for line in file.readlines()]
        #     # plot_box_bar(no_ar_data, ar_data, standard)
        # plot_bar2(ar_file_paths, nonar_file_paths)

    else:
        ar_trans_file = "../data/test2/100_trans_data_1.txt"  # 传输时间文件名
        ar_loss_file = "../data/test2/100_loss_data_1.txt"  # 丢包率分布文件名
        nonar_trans_file = "../data/test2/100_trans_data_0.txt"  # 传输时间文件名
        nonar_loss_file = "../data/test2/100_loss_data_0.txt"  # 丢包率分布文件名
        plot_data_trans_time(ar_trans_file, ar_loss_file, 1)
        plot_data_trans_time(nonar_trans_file, nonar_loss_file, 0)
