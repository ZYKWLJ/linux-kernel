import os

def delete_o_files(root_dir):
    """
    递归删除指定目录及其子目录中所有的.o文件
    
    参数:
        root_dir: 要开始搜索的根目录路径
    """
    # 检查目录是否存在
    if not os.path.exists(root_dir):
        print(f"错误: 目录 '{root_dir}' 不存在")
        return
    
    # 遍历目录
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            # 检查文件扩展名是否为.o
            if filename.endswith('.o'):
                file_path = os.path.join(dirpath, filename)
                try:
                    os.remove(file_path)
                    print(f"已删除: {file_path}")
                except Exception as e:
                    print(f"删除失败 {file_path}: {e}")

if __name__ == "__main__":
    # 指定要清理的目录
    target_directory = r"D:\1code\Linux-0.11"
    
    # 确认用户意图
    confirm = input(f"确定要删除 '{target_directory}' 及其子目录中所有的.o文件吗? (y/n): ")
    if confirm.lower() == 'y':
        delete_o_files(target_directory)
        print("清理完成")
    else:
        print("操作已取消")
