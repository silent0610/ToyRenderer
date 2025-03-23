import os
import subprocess
import sys
# 获取当前目录
currentDir = os.getcwd()

# 获取所有 .hlsl 文件
hlslFiles = [f for f in os.listdir(currentDir) if f.endswith('.hlsl')]


# 编译所有的 .hlsl 文件
for hlslFile in hlslFiles:
    fileName, fileExtension = os.path.splitext(hlslFile)
    # 输出文件路径
    output_path = fileName + ".spv" 

    nameParts = fileName.split('.')  # 例如 'Triangle.Vertex' -> ['Triangle', 'Vertex']
    # 构造编译命令
    if nameParts[1] == "Vert":
        command = ["dxc", "-T", "vs_6_0", "-E","main","-spirv", "-Fo", output_path, hlslFile]  
    elif nameParts[1] == "Frag":
        command = ["dxc", "-T", "ps_6_0","-E","main","-spirv", "-Fo", output_path, hlslFile]
    else:
        print(f"错误: 文件名格式不符合要求: {hlslFile}")
        sys.exit(1)

    # 执行编译命令
    try:
        subprocess.run(command, check=True)
        print(f"编译成功: {hlslFile} -> {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"编译失败: {hlslFile} 错误信息: {e}")