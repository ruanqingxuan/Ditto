def second_largest(filename):
    """计算文件中倒数第二大的数"""
    with open(filename, "r") as f:
        numbers = [
            float(line.strip()) for line in f if line.strip()
        ]  # 读取所有非空行并转换为浮点数
    unique_numbers = sorted(set(numbers), reverse=True)  # 去重并排序
    return unique_numbers[1] if len(unique_numbers) > 1 else None  # 返回99%分位数


def process_files(file1, file2, output_file):
    """处理两个文件并写入结果"""
    second_max1 = second_largest(file1)
    second_max2 = second_largest(file2)

    with open(output_file, "w") as f:
        f.write(f"{file1} XQUIC 99%分位数: {second_max1}\n")
        f.write(f"{file2} Ditto 99%分位数: {second_max2}\n")


# 示例：请替换 'file1.txt' 和 'file2.txt' 为你的实际文件名
file1 = "../data/xquic.txt"
file2 = "../data/ditto.txt"
output_file = "../data/tail-delay.txt"

process_files(file1, file2, output_file)
print(f"计算完成，结果已写入 {output_file}")
