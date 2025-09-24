
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;


public static void CompileFile(string filePath, bool isForce)
{
    // 检查文件是否存在
    if (!File.Exists(filePath))
    {
        Console.WriteLine($"Error: File not found {filePath}");
        return;
    }

    if (isForce == false)
    {
        // 判断文件是否在过去2小时内被修改
        DateTime lastModifiedTime = File.GetLastWriteTime(filePath);
        TimeSpan threshold = TimeSpan.FromHours(2);
        if ((DateTime.Now - lastModifiedTime) > threshold)
        {
            // 文件太旧，跳过编译
            // Console.WriteLine($"Skipping old file: {Path.GetFileName(filePath)}");
            return;
        }
    }


    string fileName = Path.GetFileNameWithoutExtension(filePath);
    string outputPath = Path.ChangeExtension(filePath, ".spv");

    string[] nameParts = fileName.Split('.'); // 例如 'Triangle.Vertex' -> ["Triangle", "Vertex"]
    if (nameParts.Length < 2)
    {
        Console.WriteLine($"Pass: Invalid file name format: {filePath}");
        return;
    }

    string shaderType = nameParts.Last().ToLower();

    // 构造编译命令参数
    var arguments = new List<string>();
    switch (shaderType)
    {
        case "vert":
            arguments.AddRange(new[] { "-T", "vs_6_0", "-E", "main", "-spirv" });
            break;
        case "frag":
            arguments.AddRange(new[] { "-T", "ps_6_0", "-E", "main", "-spirv" });
            break;
        case "geom":
            arguments.AddRange(new[] { "-T", "gs_6_0", "-E", "main", "-spirv" });
            break;
        case "comp":
            arguments.AddRange(new[] { "-T", "cs_6_0", "-E", "main", "-spirv", "-fspv-target-env=vulkan1.0", "-fvk-use-dx-layout" });
            break;
        default:
            Console.WriteLine($"Pass: Unsupported shader type '{shaderType}' in file: {filePath}");
            return;
    }

    // 添加输入和输出文件参数
    arguments.AddRange(new[] { "-Fo", outputPath, filePath });

    // 设置进程启动信息
    var startInfo = new ProcessStartInfo
    {
        FileName = "dxc.exe", // 确保 dxc.exe 在系统 PATH 中，或者提供完整路径
        Arguments = string.Join(" ", arguments),
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
        CreateNoWindow = true
    };

    // 执行编译命令
    try
    {
        using (var process = Process.Start(startInfo))
        {
            if (process == null)
            {
                Console.WriteLine($"Error: Failed to start dxc.exe process for {filePath}. Is dxc.exe in your PATH?");
                return;
            }

            // 读取错误输出，这对于调试shader至关重要
            string errorOutput = process.StandardError.ReadToEnd();
            process.WaitForExit();

            if (process.ExitCode == 0)
            {
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine($"Success: {filePath} -> {outputPath}");
                Console.ResetColor();
            }
            else
            {
                // 如果编译失败，打印dxc.exe的具体错误信息
                Console.WriteLine($"Error compiling {filePath}. Details:");
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine(errorOutput);
                Console.ResetColor();
            }
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"An exception occurred while trying to compile {filePath}: {ex.Message}");
    }
}



// 1. 编译当前目录下的 .hlsl 文件
string currentDir = Directory.GetCurrentDirectory();
Console.WriteLine("Starting shader compilation... in"+currentDir);
var filesInCurrentDir = Directory.GetFiles(currentDir, "*.hlsl");
foreach (var file in filesInCurrentDir)
{
    CompileFile(file, true);
}

// 2. 编译直接子目录下的 .hlsl 文件
var subDirectories = Directory.GetDirectories(currentDir);
foreach (var dir in subDirectories)
{
    var filesInSubDir = Directory.GetFiles(dir, "*.hlsl");
    foreach (var file in filesInSubDir)
    {
        CompileFile(file, false);
    }
}

Console.WriteLine("Shader compilation finished.");