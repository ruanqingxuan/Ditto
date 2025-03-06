import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from .base_draw import BaseDraw
from .base_draw import styles


class DrawExp3_3(BaseDraw):
    def __init__(self, exp_name, font_size):
        super().__init__(
            exp_name,
            "/home/qnwang/backup/AR/testclient/picture/paper.mplstyle",
            font_size,
        )
        self.__perf_file = ""
        self.__metric_file = ""
        self.__CCA_styles = {
            "rtt": styles[3],
            "dr": styles[4],
        }
        self.__change_file = ""
        self.__CCA_file = ""

    def set_perf_file(self, file_path):
        self.__perf_file = file_path

    def set_metric_file(self, file_path):
        self.__metric_file = file_path

    def set_change_file(self, file_path):
        self.__change_file = file_path

    def set_exp_name(self, name):
        self._BaseDraw__exp_name = name

    def set_CCA_file(self, name):
        self.__CCA_file = name

    def action(self):
        plt.style.use(self._BaseDraw__style)
        #   axd = plt.figure(figsize=(8, 6), layout="constrained").subplot_mosaic(
        #       """
        # AB
        # """,
        #       gridspec_kw={
        #           "bottom": 0.25,
        #           "top": 0.80,
        #           "left": 0.05,
        #           "right": 0.95,
        #           "wspace": 0.06,
        #           "hspace": 1,
        #       },
        #   )
        labelsize = 30
        # draw network perf
        fig_A, ax_A = plt.subplots(figsize=(8, 6))
        df_perf = pd.read_csv(self.__perf_file)
        time = df_perf["time"].values / 1e6
        rtt = df_perf["latest_rtt"].values / 1000
        delivery_rate = df_perf["delivery_rate"].values / (1e6 / 8)
        # rtt
        time_conv, rtt_conv = self.convolve_result(65, time, rtt)
        ax_A.plot(time_conv, rtt_conv, label="RTT", **self.__CCA_styles["rtt"])
        ax_A.set_xlabel("Time(s)", fontsize=self._BaseDraw__font_size + 8)
        ax_A.set_ylabel("RTT(ms)", fontsize=self._BaseDraw__font_size + 8)
        ax_A.tick_params("both", labelsize=labelsize)
        ax_A.set_ylim(40, 100)
        ax_A.set_yticks(range(40, 101, 10))
        # delivery rate
        axdA_twin = ax_A.twinx()
        time_conv, dr_conv = self.convolve_result(65, time, delivery_rate)
        axdA_twin.plot(
            time_conv, dr_conv, label="Throughput", **self.__CCA_styles["dr"]
        )
        axdA_twin.set_ylabel("Throughput(Mbps)", fontsize=self._BaseDraw__font_size + 8)
        axdA_twin.tick_params("both", labelsize=labelsize)
        axdA_twin.set_ylim(4, 15)
        axdA_twin.set_yticks(range(4, 17, 2))

        ax_A.set_xlim(0, 35)  # X轴范围 0 到 8000
        ax_A.set_xticks(range(0, 36, 5))

        # 获取两个 y 轴上的图例句柄和标签
        handles_A, labels_A = ax_A.get_legend_handles_labels()
        handles_twin, labels_twin = axdA_twin.get_legend_handles_labels()
        # 合并图例
        ax_A.legend(
            handles_A + handles_twin,  # 合并 RTT 和吞吐量的曲线图例
            labels_A + labels_twin,
            loc="upper right",
            fontsize=self._BaseDraw__font_size,
            frameon=True,  # 显示图例的边框
            fancybox=True,  # 圆角边框
            # edgecolor="black"  # 黑色边框
        )
        ax_A.axvline(x=8, color="b", linestyle="--", linewidth=2)
        ax_A.grid(True)
        fig_A.savefig(self._BaseDraw__exp_name + "_RTT_Throughput.png")
        fig_A.savefig(self._BaseDraw__exp_name + "_RTT_Throughput.pdf")

        # draw metric
        fig_B, ax_B = plt.subplots(figsize=(8, 6))
        df_metric = pd.read_csv(self.__metric_file)
        time = df_metric["time"].values / 1e6
        metric = df_metric["metric"].values
        metric[metric > 2] = 2
        metric = metric / 2
        time_conv, metric_conv = self.convolve_result(65, time, metric)
        ax_B.plot(time_conv, metric_conv)
        # line
        if self.__CCA_file != "":
            df_CCA = pd.read_csv(self.__CCA_file)
            mid_time = 0
            left = -2500
            for i in range(len(df_CCA)):
                row = df_CCA.loc[i]
                mid_time = (row["time"] + left) / 2
                left = row["time"]
                if i < len(df_CCA) - 1:
                    ax_B.axvline(
                        x=row["time"] / 1e6, color="r", linestyle="--", linewidth=2
                    )
                if i == 3:
                    mid_time += 1200
                # ax_B.text(
                #     mid_time / 1e6,
                #     1.02 + (i % 2) * 0.05,  # 交错排布，避免重叠
                #     row["origin_CCA"],
                #     fontsize=self._BaseDraw__font_size,
                #     color="red",
                #     ha="center",
                #     rotation=0,
                # )
                ax_B.annotate(
                    row["origin_CCA"],  # 标注的文本
                    xy=(mid_time / 1e6, 1.0),  # 箭头指向的点 (x, y)
                    xytext=(mid_time / 1e6, 1.09 + (i % 2) * 0.05),  # 交错排布
                    fontsize=self._BaseDraw__font_size,
                    color="red",
                    ha="center",
                    rotation=0,
                    # bbox=dict(
                    #     facecolor="white", alpha=0.7, edgecolor="none"
                    # ),  # 白色背景
                    arrowprops=dict(
                        facecolor="black", arrowstyle="->", lw=1.5
                    ),  # 红色箭头
                )

        if self.__change_file != "":
            df_change = pd.read_csv(self.__change_file)
            for i in range(len(df_change)):
                row = df_change.loc[i]
                ax_A.axvline(
                    x=row["time"] / 1e6, color="b", linestyle="--", linewidth=2
                )
                ax_B.axvline(
                    x=row["time"] / 1e6, color="b", linestyle="--", linewidth=2
                )
        ax_B.set_xlabel("Time(s)", fontsize=self._BaseDraw__font_size + 8)
        ax_B.set_ylabel("metric", fontsize=self._BaseDraw__font_size + 8)
        # ax_B.tick_params(axis="both", labelsize=self._BaseDraw__font_size)

        # ax_B.set_xlim(0, 35000)  # X轴范围 0 到 8000
        ax_B.set_xticks(np.arange(0, 36, 5), np.arange(0, 36, 5))
        ax_B.set_yticks(
            np.arange(0, 1.1, 0.2),
            [0, 0.2, 0.4, 0.6, 0.8, 1.0],
        )
        # ax_B.set_yticks(
        #     np.arange(0, 1.1, 0.2),
        #     [0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0],
        # )

        # ax_B.set_ylim(0, 1)  # X轴范围 0 到 8000
        # ax_B.set_xticks(np.arange(0, 1.1, 0.2))

        ax_B.tick_params("both", labelsize=labelsize)
        ax_B.grid(True)
        fig_B.savefig(self._BaseDraw__exp_name + "_Metric.png")
        fig_B.savefig(self._BaseDraw__exp_name + "_Metric.pdf")
