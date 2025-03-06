import pandas as pd
import matplotlib.pyplot as plt
from .base_draw import BaseDraw
from .base_draw import styles
from .base_draw import scatter_styles
import numpy as np
import re


class DrawExp3_1(BaseDraw):
    def __init__(self, exp_name, font_size):
        super().__init__(
            exp_name,
            "/home/qnwang/backup/AR/testclient/picture/paper.mplstyle",
            font_size,
        )
        self.__time_file_map = {}
        self.__CCA_styles = {
            "BBR": scatter_styles[0],
            "CUBIC": scatter_styles[1],
            "COPA": scatter_styles[2],
            "Ditto": scatter_styles[3],
        }

    def set_time_file(self, file_path, file_name, CCA):
        numbers = re.findall(r"\d+", file_name)
        numbers = [int(num) for num in numbers]
        self.__time_file_map.setdefault(CCA, {})
        self.__time_file_map[CCA][numbers[0] / 1000] = file_path

    def set_exp_name(self, name):
        self._BaseDraw__exp_name = name

    def action(self):
        plt.style.use(self._BaseDraw__style)
        plt.figure(figsize=(8, 6))

        for CCA, value in self.__time_file_map.items():
            target_arr = []
            avg_arr = []
            for time, file in value.items():
                target_arr.append(time)
                df_time = pd.read_csv(file)
                time = df_time.values
                avg = np.mean(time)
                avg_arr.append(avg)
            plt.scatter(target_arr, avg_arr, label=CCA, **self.__CCA_styles[CCA])
        plt.plot(np.arange(20, 61, 10), np.arange(20, 61, 10))

        plt.xlabel("Specify transfer time(s)", fontsize=self._BaseDraw__font_size + 8)
        plt.ylabel("Actual transfer time(s)", fontsize=self._BaseDraw__font_size + 8)
        # plt.xticks(
        #     [20, 30, 40, 50, 60],
        #     ["20", "30", "40", "50", "60"],
        #     fontsize=self._BaseDraw__font_size,  # 设置字体大小
        # )  # 从0到10，间隔为2
        plt.xticks(
            np.arange(20, 65, 10), fontsize=self._BaseDraw__font_size
        )  # 设置字体大小)  # 从-1到1.5，间隔为0.5
        plt.yticks(
            np.arange(0, 65, 10), fontsize=self._BaseDraw__font_size
        )  # 设置字体大小)  # 从-1到1.5，间隔为0.5
        plt.grid(True)
        plt.legend(
            loc="upper right",
            fontsize=self._BaseDraw__font_size - 10,
            bbox_to_anchor=(1, 0.77),
        )
        plt.savefig(
            self._BaseDraw__exp_name + ".png",
            bbox_inches="tight",
            pad_inches=0.1,
        )
        plt.savefig(
            self._BaseDraw__exp_name + ".pdf",
            bbox_inches="tight",
            pad_inches=0.1,
        )
