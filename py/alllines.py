import os


def count_lines_in_file(file_path):
    """
    统计单个文件的代码行数
    :param file_path: 文件路径
    :return: 代码行数
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            lines = file.readlines()
            return len([line for line in lines if line.strip() and not line.strip().startswith('//')])
    except Exception as e:
        print(f"读取文件 {file_path} 时出错: {e}")
        return 0


def count_lines_in_directory(directory):
    """
    递归统计目录下所有 .c 和 .h 文件的代码行数
    :param directory: 目录路径
    :return: .c 文件总行数, .h 文件总行数
    """
    c_total_lines = 0
    h_total_lines = 0
    c_file_total=0;
    h_file_total=0;
    s_total_lines=0;
    s_file_total=0;
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.c'):
                file_path = os.path.join(root, file)
                lines = count_lines_in_file(file_path)
                c_total_lines += lines
                # print(f"{file_path}: {lines} 行")
                c_file_total+=1;
            elif file.endswith('.h'):
                file_path = os.path.join(root, file)
                lines = count_lines_in_file(file_path)
                h_total_lines += lines
                # print(f"{file_path}: {lines} 行")
                h_file_total+=1;
            elif file.endswith('.s') or file.endswith('.S'):
                file_path = os.path.join(root, file)
                lines = count_lines_in_file(file_path)
                s_total_lines += lines
                s_file_total+=1;
                print(f"{file_path}: {lines} 行")
    print(f".c 文件总数: {c_file_total}")
    print(f".h 文件总数: {h_file_total}")
    print(f".S 文件总数: {s_file_total}")
    return c_total_lines, h_total_lines , s_total_lines


if __name__ == "__main__":
    # directory = r"/home/eyk/桌面/TL/"
    # directory = r"/home/eyk/1code/linux-0.11"
    directory = r"/home/eyk/1code/Linux-0.11code"
    
    c_lines, h_lines ,s_total_lines= count_lines_in_directory(directory)
    print(f".c 文件: {c_lines}")
    print(f".h 文件: {h_lines}")
    print(f".s 文件: {s_total_lines}")
    print(f"总代码行数: {c_lines + h_lines}")
    print(f"总代码行数(含汇编): {c_lines + h_lines+s_total_lines}")
    