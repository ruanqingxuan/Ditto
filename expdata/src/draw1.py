import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
from scipy.signal import convolve
import os
from matplotlib import rcParams
from .base_draw import BaseDraw


class DrawExp1(BaseDraw):
    def __init__(self, exp_name, font_size):
        super().__init__(
            exp_name,
            "/home/qnwang/backup/AR/testclient/picture/paper.mplstyle",
            font_size,
        )
        self.__cong_file = ""
        self.__CCA_file = ""

    def set_cong_file(self, file_path):
        self.__cong_file = file_path

    def set_CCA_file(self, file_path):
        self.__CCA_file = file_path

    def action(self):
        plt.style.use(self._BaseDraw__style)
        axd = plt.figure(figsize=(20, 25), layout="constrained").subplot_mosaic(
            """
      B
      C
      D
      """,
            gridspec_kw={
                "bottom": 0.25,
                "top": 0.8,
                "left": 0.05,
                "right": 0.95,
                "wspace": 0.45,
                "hspace": 0.1,
            },
        )
        # draw CCA perf(cwnd, srtt, pacing_rate, etc.)
        df_CCA_perf = pd.read_csv(self.__cong_file)
        df_CCA_state = pd.read_csv(self.__CCA_file)
        time = df_CCA_perf["time"]
        cwnd = df_CCA_perf["cwnd"]
        axd["B"].plot(time / 1000, cwnd, color="grey")
        axd["B"].set_ylabel("cwnd", fontsize=self._BaseDraw__font_size)
        mid_time = 0
        for i in range(len(df_CCA_state)):
            row = df_CCA_state.loc[i]
            right = 5000000
            if i < len(df_CCA_state) - 1:
                right = df_CCA_state.loc[i + 1]["time"]
            mid_time = (row["time"] + right) / 2
            if i != 0:
                axd["D"].axvline(
                    x=row["time"] / 1000, color="r", linestyle="--", linewidth=2
                )
                axd["C"].axvline(
                    x=row["time"] / 1000, color="r", linestyle="--", linewidth=2
                )
                axd["B"].axvline(
                    x=row["time"] / 1000, color="r", linestyle="--", linewidth=2
                )
            axd["B"].text(
                mid_time / 1000,
                1.5 * 1e6,
                row["CCA"],
                fontsize=self._BaseDraw__font_size,
                color="red",
                ha="center",
            )
        # for it in df_CCA_state.itertuples():
        #   axd['B'].axvline(x=it.time/1000, color='r', linestyle='--', linewidth=2)
        #   axd['B'].text(it.time/1000, 1.5 * 1e6, it.CCA, fontsize=36, color='red')
        srtt = df_CCA_perf["srtt"]
        axd["C"].plot(time / 1000, srtt, color="blue")
        axd["C"].set_ylabel("RTT(us)", fontsize=self._BaseDraw__font_size)
        pacing_rate = df_CCA_perf["pacing rate"]
        axd["D"].plot(time / 1000, pacing_rate, color="green")
        axd["D"].set_xlabel("Time(ms)", fontsize=self._BaseDraw__font_size)
        axd["D"].set_ylabel("delivery_rate", fontsize=self._BaseDraw__font_size)
        plt.savefig(self._BaseDraw__exp_name + ".png")
