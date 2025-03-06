import pandas as pd
import matplotlib.pyplot as plt
from .base_draw import BaseDraw
from .base_draw import styles
import numpy as np


class DrawExp2(BaseDraw):
    def __init__(self, exp_name, perf, font_size):
        super().__init__(
            exp_name,
            "/home/qnwang/backup/AR/testclient/picture/paper.mplstyle",
            font_size,
        )
        self.__perf_unit = {
            "latest_rtt": "us",
            "loss_rate": "‰",
            "delivery_rate": "Mbps",
        }
        self.__perf = perf
        self.__CCA_styles = {
            "BBR": styles[0],
            "CUBIC": styles[1],
            "COPA": styles[2],
            "Ditto": styles[3],
        }
        self.__perf_file_map = {}
        self.__metric_file_map = {}
        self.__cong_file = ""
        self.__CCA_file = ""
        self.__status_file = ""

    def add_CCA_perf(self, CCA_name: str, file_path: str):
        # file map
        self.__perf_file_map[CCA_name] = file_path

    def add_CCA_metric(self, CCA_name: str, file_path: str):
        # file map
        self.__metric_file_map[CCA_name] = file_path

    def set_cong_file(self, file_path):
        self.__cong_file = file_path

    def set_CCA_file(self, file_path):
        self.__CCA_file = file_path

    def set_status_file(self, file_path):
        self.__status_file = file_path

    def action(self):
        plt.style.use(self._BaseDraw__style)
        # axd = plt.figure(figsize=(40, 10), layout="constrained").subplot_mosaic(
        #   """
        #   AE
        #   AE
        #   AE
        #   """,
        #   gridspec_kw={
        #     "bottom": 0.25,
        #     "top": 0.90,
        #     "left": 0.05,
        #     "right": 0.95,
        #     "wspace": 0.05,
        #     "hspace": 1,
        #   },
        # )

        # draw network perf
        fig_A, ax_A = plt.subplots(figsize=(8, 6))
        for CCA_name, file_path in self.__perf_file_map.items():
            df_CCA = pd.read_csv(file_path)
            time = df_CCA["time"].values / 1000
            divide = 1
            if self.__perf == "loss_rate":
                divide = 10000
            elif self.__perf == "delivery_rate":
                divide = 1e6 / 8
            perf = df_CCA[self.__perf].values / divide
            time_conv, perf_conv = self.convolve_result(65, time, perf)
            ax_A.plot(
                time_conv, perf_conv, label=CCA_name, **self.__CCA_styles[CCA_name]
            )
        ax_A.set_xlabel("Time(ms)", fontsize=self._BaseDraw__font_size + 8)
        perf_chinese = ""
        if self.__perf == "delivery_rate":
            perf_chinese = "Throughput"
        elif self.__perf == "loss_rate":
            perf_chinese = "Loss Rate"
        elif self.__perf == "latest_rtt":
            perf_chinese = "RTT"
        ax_A.set_ylabel(
            perf_chinese + f"({self.__perf_unit[self.__perf]})",
            fontsize=self._BaseDraw__font_size + 8,
        )
        ax_A.set_xlim(0, 10000)  # X轴范围 0 到 8000
        ax_A.set_xticks(range(0, 10001, 2000))
        ax_A.set_ylim(0, 170)  # Y轴范围 0 到 25
        ax_A.set_yticks(np.arange(0, 170, 25))
        # ax_A.set_ylim(min(perf_conv) * 0.95, max(perf_conv) * 1.05)  # 预留 5% 额外空间
        ax_A.legend(loc="lower right", fontsize=self._BaseDraw__font_size - 10, ncol=1)
        ax_A.grid(True)
        plt.tight_layout()  # 自动调整布局
        ax_A.tick_params(
            axis="both", labelsize=self._BaseDraw__font_size
        )  # 调整刻度标签字体大小
        plt.subplots_adjust(top=0.5)  # 适当减少 top 值，避免超出画布

        fig_A.tight_layout()
        fig_A.savefig(
            self._BaseDraw__exp_name + "_A.png", bbox_inches="tight"
        )  # 保存子图 A
        fig_A.savefig(
            self._BaseDraw__exp_name + "_A.pdf", bbox_inches="tight"
        )  # 保存子图 A

        # draw metric
        fig_E, ax_E = plt.subplots(figsize=(8, 6))
        status_chinese_map = {
            "SLOW START": "SS",
            "PROBE CCA": "PC",
            "STABLE RUNNING": "SR",
            "P/S": "PC/SR",
        }

        for CCA_name, file_path in self.__metric_file_map.items():
            df_CCA = pd.read_csv(file_path)
            time = df_CCA["time"].values / 1000
            metric = df_CCA["metric"].values
            time_conv, metric_conv = self.convolve_result(65, time, metric)
            ax_E.plot(
                time_conv, metric_conv, label=CCA_name, **self.__CCA_styles[CCA_name]
            )
        df_status = pd.read_csv(self.__status_file)
        for i in range(len(df_status) - 1):
            row = df_status.loc[i]
            right = df_status.loc[i + 1]["time"]
            mid_time = (row["time"] + right) / 2
            if i != 0:
                ax_E.axvline(
                    x=row["time"] / 1000, color="r", linestyle="--", linewidth=2
                )
            height = 1.01
            if self._BaseDraw__exp_name == "loss_rate":
                height = 1.01
            ax_E.text(
                mid_time / 1000,
                height,
                status_chinese_map[row["status"]],
                fontsize=self._BaseDraw__font_size,
                color="red",
                ha="center",
            )
        ax_E.set_xlabel("Time(ms)", fontsize=self._BaseDraw__font_size + 8)
        ax_E.set_ylabel("metric", fontsize=self._BaseDraw__font_size + 8)
        ax_E.set_xlim(0, 10000)  # X轴范围 0 到 8000
        ax_E.set_xticks(range(0, 10001, 2000))
        ax_E.set_ylim(0, 1)  # Y轴范围 0 到 25
        ax_E.set_yticks(np.arange(0, 1.2, 0.2))
        ax_E.legend(loc="lower right", fontsize=self._BaseDraw__font_size - 10)
        ax_E.tick_params(axis="both", labelsize=self._BaseDraw__font_size)
        ax_E.grid(True)
        fig_E.tight_layout()
        # plt.show()
        fig_E.savefig(
            self._BaseDraw__exp_name + "_E.png", bbox_inches="tight"
        )  # 保存子图 E
        fig_E.savefig(
            self._BaseDraw__exp_name + "_E.pdf", bbox_inches="tight"
        )  # 保存子图 E
