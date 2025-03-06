import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
from scipy.signal import convolve
import os
from matplotlib import rcParams
from src.draw1 import DrawExp1
from src.draw2 import DrawExp2
from src.draw3_3 import DrawExp3_3
from src.draw3_1 import DrawExp3_1
from src.draw3_2 import DrawExp3_2


# path = '/home/qnwang/worknew/CS/expdata/data'
path = "/home/qnwang/backup/CS/expdata/data"

# plt.rcParams["font.sans-serif"] = ["SimSun"]
plt.rcParams["axes.unicode_minus"] = False


if __name__ == "__main__":
    # exp path
    for name in os.listdir(path):
        name_path = os.path.join(path, name)
        if name == "exp1":
            for exp_name in os.listdir(name_path):
                draw1 = DrawExp1(exp_name=exp_name, font_size=40)
                exp_path = os.path.join(name_path, exp_name)
                # CCA path
                if os.path.isdir(exp_path):
                    for CCA_name in os.listdir(exp_path):
                        CCA_path = os.path.join(exp_path, CCA_name)
                        # CCA path
                        if os.path.isdir(CCA_path):
                            for file_name in os.listdir(CCA_path):
                                file_path = os.path.join(CCA_path, file_name)
                                if file_name == "cong_perf.csv":
                                    draw1.set_cong_file(file_path)
                                elif file_name == "CCA.csv":
                                    draw1.set_CCA_file(file_path)
                draw1.action()
        elif name == "exp2":
            for exp_name in os.listdir(name_path):
                draw2 = DrawExp2(exp_name=exp_name, perf=exp_name, font_size=30)
                exp_path = os.path.join(name_path, exp_name)
                # CCA path
                if os.path.isdir(exp_path):
                    for CCA_name in os.listdir(exp_path):
                        CCA_path = os.path.join(exp_path, CCA_name)
                        # CCA path
                        if os.path.isdir(CCA_path):
                            for file_name in os.listdir(CCA_path):
                                file_path = os.path.join(CCA_path, file_name)
                                if file_name == "net_perf.csv":
                                    draw2.add_CCA_perf(CCA_name, file_path)
                                elif file_name == "CCA_metric.csv":
                                    draw2.add_CCA_metric(CCA_name, file_path)
                                elif file_name == "CS_status.csv":
                                    draw2.set_status_file(file_path)
                draw2.action()
        elif name == "exp3_3":
            for exp_name in os.listdir(name_path):
                draw3_3 = DrawExp3_3(exp_name=exp_name, font_size=30)
                exp_path = os.path.join(name_path, exp_name)
                # CCA path
                if os.path.isdir(exp_path):
                    for CCA_name in os.listdir(exp_path):
                        draw3_3.set_exp_name(f"{exp_name}_{CCA_name}")
                        CCA_path = os.path.join(exp_path, CCA_name)
                        # CCA path
                        if os.path.isdir(CCA_path):
                            for file_name in os.listdir(CCA_path):
                                file_path = os.path.join(CCA_path, file_name)
                                if file_name == "net_perf.csv":
                                    draw3_3.set_perf_file(file_path)
                                elif file_name == "change.csv":
                                    draw3_3.set_change_file(file_path)
                                elif file_name == "CCA&metric.csv":
                                    draw3_3.set_metric_file(file_path)
                                elif file_name == "CCA_switching.csv":
                                    draw3_3.set_CCA_file(file_path)
                        draw3_3.action()
        elif name == "exp3_1":
            draw3_1 = DrawExp3_1("file", font_size=30)
            for exp_name in os.listdir(name_path):
                exp_path = os.path.join(name_path, exp_name)
                # CCA path
                if os.path.isdir(exp_path):
                    for CCA_name in os.listdir(exp_path):
                        CCA_path = os.path.join(exp_path, CCA_name)
                        # CCA path
                        if os.path.isdir(CCA_path):
                            for file_name in os.listdir(CCA_path):
                                file_path = os.path.join(CCA_path, file_name)
                                draw3_1.set_time_file(file_path, file_name, CCA_name)
            draw3_1.action()
        elif name == "exp3_2":
            draw3_2 = DrawExp3_2(exp_name="qoe", font_size=30)
            for exp_name in os.listdir(name_path):
                exp_path = os.path.join(name_path, exp_name)
                # CCA path
                if os.path.isdir(exp_path):
                    for CCA_name in os.listdir(exp_path):
                        CCA_path = os.path.join(exp_path, CCA_name)
                        # CCA path
                        if os.path.isdir(CCA_path):
                            for file_name in os.listdir(CCA_path):
                                file_path = os.path.join(CCA_path, file_name)
                                if exp_name == "bandwidth":
                                    draw3_2.add_bandwidth_file(CCA_name, file_path)
                                elif exp_name == "latency":
                                    draw3_2.add_latency_file(CCA_name, file_path)
                                elif exp_name == "lossrate":
                                    draw3_2.add_lossrate_file(CCA_name, file_path)
            draw3_2.action()
