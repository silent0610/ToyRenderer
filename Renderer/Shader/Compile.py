import os
import subprocess
import sys
import time
# 获取当前目录

def CompileFile(file):
    
    # 获取文件的最后修改时间
    last_modified_time = os.path.getmtime(file)

    # 获取当前时间
    current_time = time.time()

    # 设置一个时间阈值，例如过去 1 小时
    threshold = 60 * 60  # 1 小时 = 60 分钟 * 60 秒

    # 判断文件是否在过去1小时内被修改
    if current_time - last_modified_time >= threshold:
        return
 

    fileName, fileExtension = os.path.splitext(file)
    # 输出文件路径
    output_path = fileName + ".spv" 

    nameParts = fileName.split('.')  # 例如 'Triangle.Vertex' -> ['Triangle', 'Vertex']
    # 构造编译命令
    if nameParts[1] == "Vert" or nameParts[1] =="vert":
        command = ["dxc", "-T", "vs_6_0", "-E","main","-spirv", "-Fo", output_path, file]  
    elif nameParts[1] == "Frag" or nameParts[1] == "frag":
        command = ["dxc", "-T", "ps_6_0","-E","main","-spirv", "-Fo", output_path, file]
    elif nameParts[1] == "Geom":
        command = ["dxc", "-T", "gs_6_0","-E","main","-spirv", "-Fo", output_path, file]
    else:
        print(f"错误: 文件名格式不符合要求: {file}")
        sys.exit(1)

    # 执行编译命令
    try:
        subprocess.run(command, check=True)
        print(f"编译成功: {file} -> {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"编译失败: {file} 错误信息: {e}")

currentDir = os.getcwd()


# 获取所有 .hlsl 文件
hlslFiles = [f for f in os.listdir(currentDir) if f.endswith('.hlsl')]

# 编译所有的 .hlsl 文件
for hlslFile in hlslFiles:
    CompileFile(hlslFile)

folders = [d for d in os.listdir('.') if os.path.isdir(d)]

for folder in folders:
    hlslFiles = [f for f in os.listdir(folder) if f.endswith('.hlsl')]
    for hlslFile in hlslFiles:
        CompileFile(folder+"/"+hlslFile)