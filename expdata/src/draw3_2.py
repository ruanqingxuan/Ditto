import pandas as pd
import matplotlib.pyplot as plt
from .base_draw import BaseDraw
from .base_draw import styles
import numpy as np
import re
import os


def extract_number(s):
    file_name = os.path.basename(s)
    numbers = re.findall(r"\d+", file_name)
    numbers = [int(num) for num in numbers]
    return numbers[0]


class DrawExp3_2(BaseDraw):
    def __init__(self, exp_name, font_size):
        super().__init__(
            exp_name,
            "/home/qnwang/backup/AR/testclient/picture/paper.mplstyle",
            font_size,
        )
        self.__bandwidth_file_map = {}
        self.__latency_file_map = {}
        self.__lossrate_file_map = {}
        self.__CCA_styles = {
            "BBR": styles[0],
            "CUBIC": styles[1],
            "COPA": styles[2],
            "Ditto": styles[3],
        }

    def add_bandwidth_file(self, CCA, file_path):
        self.__bandwidth_file_map.setdefault(CCA, []).append(file_path)

    def add_latency_file(self, CCA, file_path):
        self.__latency_file_map.setdefault(CCA, []).append(file_path)

    def add_lossrate_file(self, CCA, file_path):
        self.__lossrate_file_map.setdefault(CCA, []).append(file_path)

    def action(self):
        labelsize = 30
        #     plt.style.use(self._BaseDraw__style)
        #     axd = plt.figure(figsize=(40, 10), layout="constrained").subplot_mosaic(
        #         """
        #   ABC
        #   """,
        #         gridspec_kw={
        #             "bottom": 0.25,
        #             "top": 0.90,
        #             "left": 0.05,
        #             "right": 0.95,
        #             "wspace": 0.05,
        #             "hspace": 1,
        #         },
        #     )
        # qoe_arr = []
        # x_arr = []

        # bandwidth
        fig_A, ax_A = plt.subplots(figsize=(8, 6))
        for CCA, QoEs in self.__bandwidth_file_map.items():
            qoe_arr = []
            x_arr = []
            QoEs = sorted(QoEs, key=extract_number)
            for QoE in QoEs:
                file_name = os.path.basename(QoE)
                numbers = re.findall(r"\d+", file_name)
                numbers = [int(num) for num in numbers]
                x_arr.append(numbers[0])
                df_qoe = pd.read_csv(QoE)
                value = df_qoe.values
                value[value < 0] = 0
                qoe_arr.append(value.mean())
            ax_A.plot(x_arr, qoe_arr, label=CCA, **self.__CCA_styles[CCA])
        ax_A.set_ylabel("QoE(kbps)", fontsize=self._BaseDraw__font_size + 8)
        ax_A.set_xlabel("Bandwidth(Mbps)", fontsize=self._BaseDraw__font_size + 8)
        # ax_A.set_xticks([5, 10, 15, 20, 25], x_arr)
        ax_A.set_xticks(np.arange(5, 26, 5), np.arange(5, 26, 5))
        ax_A.set_yticks(np.arange(0, 5301, 530), np.arange(0, 5301, 530))
        ax_A.tick_params("both", labelsize=labelsize)
        ax_A.legend(loc="lower right", fontsize=self._BaseDraw__font_size - 8)
        plt.tight_layout()
        fig_A.savefig(
            self._BaseDraw__exp_name + "_bandwidth.png",
            bbox_inches="tight",
            pad_inches=0.1,
        )
        fig_A.savefig(
            self._BaseDraw__exp_name + "_bandwidth.pdf",
            bbox_inches="tight",
            pad_inches=0.1,
        )

        # latency
        fig_B, ax_B = plt.subplots(figsize=(8, 6))
        for CCA, QoEs in self.__latency_file_map.items():
            qoe_arr = []
            x_arr = []
            QoEs = sorted(QoEs, key=extract_number)
            for QoE in QoEs:
                file_name = os.path.basename(QoE)
                numbers = re.findall(r"\d+", file_name)
                numbers = [int(num) for num in numbers]
                x_arr.append(numbers[0])
                df_qoe = pd.read_csv(QoE)
                value = df_qoe.values
                value[value < 0] = 0
                qoe_arr.append(value.mean())
            ax_B.plot(x_arr, qoe_arr, label=CCA, **self.__CCA_styles[CCA])
        ax_B.set_ylabel("QoE(kbps)", fontsize=self._BaseDraw__font_size + 8)
        ax_B.set_xlabel("One-way Latency(ms)", fontsize=self._BaseDraw__font_size + 8)
        ax_B.legend(loc="lower right", fontsize=self._BaseDraw__font_size)
        ax_B.set_xticks([10, 210, 410, 610, 810], x_arr)
        ax_B.set_yticks(np.arange(0, 5301, 530), np.arange(0, 5301, 530))
        ax_B.tick_params("both", labelsize=labelsize)
        ax_B.legend(loc="lower right", fontsize=self._BaseDraw__font_size - 8)
        plt.tight_layout()
        fig_B.savefig(
            self._BaseDraw__exp_name + "_latency.png",
            bbox_inches="tight",
            pad_inches=0.1,
        )
        fig_B.savefig(
            self._BaseDraw__exp_name + "_latency.pdf",
            bbox_inches="tight",
            pad_inches=0.1,
        )

        # lossrate
        fig_C, ax_C = plt.subplots(figsize=(8, 6))
        for CCA, QoEs in self.__lossrate_file_map.items():
            qoe_arr = []
            x_arr = []
            QoEs = sorted(QoEs, key=extract_number)
            for QoE in QoEs:
                file_name = os.path.basename(QoE)
                numbers = re.findall(r"\d+", file_name)
                numbers = [int(num) for num in numbers]
                x_arr.append(numbers[0])
                df_qoe = pd.read_csv(QoE)
                value = df_qoe.values
                value[value < 0] = 0
                qoe_arr.append(value.mean())
            ax_C.plot(x_arr, qoe_arr, label=CCA, **self.__CCA_styles[CCA])
        ax_C.set_ylabel("QoE(kbps)", fontsize=self._BaseDraw__font_size + 8)
        ax_C.set_xlabel(
            "Loss Rate(\%)", fontsize=self._BaseDraw__font_size + 8, usetex=True
        )
        ax_C.legend(loc="lower right", fontsize=self._BaseDraw__font_size)
        # ax_C.set_xticks([0, 5, 10, 15, 20], x_arr)
        ax_C.set_xticks(np.arange(0, 24, 5), np.arange(0, 24, 5))
        ax_C.set_yticks(np.arange(0, 5301, 530), np.arange(0, 5301, 530))
        ax_C.tick_params("both", labelsize=labelsize)
        ax_C.legend(loc="lower right", fontsize=self._BaseDraw__font_size - 8)
        plt.tight_layout()
        fig_C.savefig(
            self._BaseDraw__exp_name + "_lossrate.png",
            bbox_inches="tight",
            pad_inches=0.1,
        )
        fig_C.savefig(
            self._BaseDraw__exp_name + "_lossrate.pdf",
            bbox_inches="tight",
            pad_inches=0.1,
        )
        # plt.savefig(self._BaseDraw__exp_name + ".png")
