# 用于进一步分析保存包号和包数
import sys
import csv
import numpy as np
import pandas as pd


# 读取CSV文件中的最后两列并分开保存到TXT文件
def save_send_to_txt(csv_file, txt_file):
    send = []
    # 打开CSV文件
    with open(csv_file, mode='r', newline='', encoding='utf-8') as csvfile:
        csvreader = csv.reader(csvfile)
        # 跳过第一行（标题行）
        next(csvreader)
        # 遍历CSV文件的每一行，假设你想提取第2列的数据（索引1）
        for row in csvreader:
            selected_columns = [row[2], row[5], row[6]]  # 存储3,6,7列
            send.append(selected_columns)  # 根据需要修改索引，选择特定的列
    # 将数组数据写入TXT文件
    with open(txt_file, mode='w', encoding='utf-8') as file:
        for row in send:
            file.write('\t'.join(row) + '\n')  # 用制表符分隔每一列，换行后写入文件


# 调用函数，指定CSV文件和输出TXT文件的路径
input_file = "../data/cong_perf.csv"  # 源文件名
output_file = "../data/send.txt"  # 输出文件名
save_send_to_txt(input_file, output_file)
