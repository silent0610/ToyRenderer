import os
import subprocess
import sys
import time
# 获取当前目录

def CompileFile(file,  isForce):
    
    if( isForce== False):

        # 获取文件的最后修改时间
        last_modified_time = os.path.getmtime(file)

        # 获取当前时间
        current_time = time.time()

        # 设置一个时间阈值，例如过去 1 小时
        threshold = 60 * 60 * 10  # 1 小时 = 60 分钟 * 60 秒

        # 判断文件是否在过去1小时内被修改
        if current_time - last_modified_time >= threshold:
            return
 

    fileName, fileExtension = os.path.splitext(file)
    # 输出文件路径
    output_path = fileName + ".spv" 

    nameParts = fileName.split('.')  # 例如 'Triangle.Vertex' -> ['Triangle', 'Vertex']
    
    # 构造编译命令
    if len(nameParts) < 2:
        print(f"pass: invalid file name format: {file}")
        return
        
    shaderType = nameParts[-1].lower()  # 获取最后一个部分作为着色器类型
    
    if shaderType == "vert":
        command = ["dxc", "-T", "vs_6_0", "-E","main","-spirv", "-Fo", output_path, file]  
    elif shaderType == "frag":
        command = ["dxc", "-T", "ps_6_0","-E","main","-spirv", "-Fo", output_path, file]
    elif shaderType == "geom":
        command = ["dxc", "-T", "gs_6_0","-E","main","-spirv", "-Fo", output_path, file]
    elif shaderType == "comp":
        command = ["dxc", "-T", "cs_6_0","-E","main","-spirv", "-fspv-target-env=vulkan1.0", "-fvk-use-dx-layout", "-Fo", output_path, file]
    else:
        print(f"pass: unsupported shader type '{shaderType}' in file: {file}")
        return

    # 执行编译命令
    try:
        subprocess.run(command, check=True)
        print(f"success: {file} -> {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"error: {file} 错误信息: {e}")

currentDir = os.getcwd()


# 获取所有 .hlsl 文件
hlslFiles = [f for f in os.listdir(currentDir) if f.endswith('.hlsl')]

# 编译所有的 .hlsl 文件
for hlslFile in hlslFiles:
    CompileFile(hlslFile, True)

folders = [d for d in os.listdir('.') if os.path.isdir(d)]

for folder in folders:
    hlslFiles = [f for f in os.listdir(folder) if f.endswith('.hlsl')]
    for hlslFile in hlslFiles:
        CompileFile(folder+"/"+hlslFile,False)