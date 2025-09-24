#!/usr/bin/env python3
"""
SDF可视化脚本 - 加载从MyToyRenderer导出的SDF数据并进行可视化验证
用法: 运行渲染器后按E键导出SDF数据，然后运行此脚本
"""

import numpy as np
import matplotlib.pyplot as plt
import os

try:
    import pyvista as pv
    HAS_PYVISTA = True
    # 设置PyVista主题
    pv.set_plot_theme("document")
except ImportError:
    print("警告: 未安装PyVista，只能进行2D可视化")
    HAS_PYVISTA = False

def load_sdf_data(file_path, resolution=64):
    """从二进制文件加载SDF数据"""
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"SDF文件未找到: {file_path}")
    
    expected_size = resolution ** 3
    file_size = os.path.getsize(file_path)
    
    # 统一使用R32_SFLOAT格式
    if file_size == expected_size * 4:
        sdf_data = np.fromfile(file_path, dtype=np.float32)
    else:
        # 尝试推断分辨率
        if file_size % 4 == 0:
            total_voxels = file_size // 4
            resolution = round(total_voxels ** (1/3))
            if resolution ** 3 == total_voxels:
                sdf_data = np.fromfile(file_path, dtype=np.float32)
                print(f"自动推断分辨率: {resolution}³")
            else:
                raise ValueError(f"文件大小不匹配32位浮点格式: {file_size} bytes")
        else:
            raise ValueError(f"文件大小必须是4的倍数(R32_SFLOAT): {file_size} bytes")
    
    expected_size = resolution ** 3
    if len(sdf_data) != expected_size:
        raise ValueError(f"数据尺寸不匹配: 期望{expected_size}，实际{len(sdf_data)}")
    
    # 重塑为3D数组
    sdf_volume = sdf_data.reshape((resolution, resolution, resolution))
    print(f"SDF数据加载成功: {resolution}³ = {len(sdf_data)} 体素 [R32_SFLOAT]")
    
    return sdf_volume

def analyze_sdf_quality(sdf_volume):
    """分析SDF质量指标"""
    print("\n=== SDF质量分析 ===")
    print(f"数据范围: [{sdf_volume.min():.3f}, {sdf_volume.max():.3f}]")
    print(f"平均值: {sdf_volume.mean():.3f}")
    print(f"标准差: {sdf_volume.std():.3f}")
    
    # 统计内部/外部体素
    inside_voxels = np.sum(sdf_volume < 0)
    outside_voxels = np.sum(sdf_volume > 0)  
    zero_voxels = np.sum(np.abs(sdf_volume) < 1e-6)
    
    total = sdf_volume.size
    print(f"内部体素 (SDF<0): {inside_voxels} ({inside_voxels/total*100:.1f}%)")
    print(f"外部体素 (SDF>0): {outside_voxels} ({outside_voxels/total*100:.1f}%)")
    print(f"零值体素: {zero_voxels} ({zero_voxels/total*100:.1f}%)")
    
    # 检查是否有异常值
    abs_sdf = np.abs(sdf_volume)
    large_values = np.sum(abs_sdf > 10.0)
    if large_values > 0:
        print(f"警告: 发现{large_values}个可能异常的大值 (|SDF|>10)")

def visualize_2d_slices(sdf_volume, slice_positions=[0.25, 0.5, 0.75]):
    """2D切片可视化 - 应用Y和Z轴翻转"""
    # 应用Y和Z轴翻转以匹配着色器坐标系
    sdf_flipped = sdf_volume.copy()
    sdf_flipped = np.flip(sdf_flipped, axis=1)  # 翻转Y轴 (axis=1)
    sdf_flipped = np.flip(sdf_flipped, axis=2)  # 翻转Z轴 (axis=2)
    
    resolution = sdf_flipped.shape[0]
    fig, axes = plt.subplots(1, len(slice_positions), figsize=(15, 5))
    
    if len(slice_positions) == 1:
        axes = [axes]
    
    for i, pos in enumerate(slice_positions):
        z_index = int(pos * resolution)
        slice_data = sdf_flipped[:, :, z_index]
        
        im = axes[i].imshow(slice_data, cmap='RdBu_r', origin='lower')
        axes[i].set_title(f'Z切片 {z_index}/{resolution} (pos={pos:.2f}) [Y,Z翻转]')
        axes[i].set_xlabel('X')
        axes[i].set_ylabel('Y (翻转)')
        plt.colorbar(im, ax=axes[i])
    
    plt.tight_layout()
    plt.show()

def visualize_3d_isosurface(sdf_volume):
    """3D等值面可视化"""
    if not HAS_PYVISTA:
        print("跳过3D可视化 (需要安装PyVista)")
        return
    
    try:
        # 应用Y和Z轴翻转以匹配着色器坐标系
        print("应用坐标系翻转...")
        sdf_flipped = sdf_volume.copy()
        # sdf_flipped = np.flip(sdf_flipped, axis=1)  # 翻转Y轴 (axis=1)
        sdf_flipped = np.flip(sdf_flipped, axis=0) 
        sdf_flipped = np.flip(sdf_flipped, axis=1) 
        # 创建PyVista网格
        grid = pv.ImageData()
        grid.dimensions = sdf_flipped.shape
        grid.origin = (0, 0, 0)
        grid.spacing = (1, 1, 1)
        grid.point_data["sdf"] = sdf_flipped.flatten()
        
        # 提取SDF=0的等值面
        surface = grid.contour(isosurfaces=[0], scalars="sdf")
        
        if surface.n_points == 0:
            print("警告: 未找到SDF=0的表面，尝试其他等值面...")
            # 尝试接近0的值
            for iso_value in [-0.5, 0.5, -1.0, 1.0]:
                surface = grid.contour(isosurfaces=[iso_value], scalars="sdf")
                if surface.n_points > 0:
                    print(f"使用等值面 SDF={iso_value}")
                    break
        
        if surface.n_points == 0:
            print("错误: 无法提取任何等值面，SDF数据可能有问题")
            return
        
        print(f"等值面提取成功: {surface.n_points}个顶点, {surface.n_cells}个面片")
        
        # 渲染3D表面
        plotter = pv.Plotter(window_size=[800, 600])
        plotter.add_mesh(surface, color='lightblue', show_edges=True)
        plotter.add_axes()
        plotter.show_grid()
        plotter.set_background('white')
        
        print("3D可视化窗口已打开，关闭窗口继续...")
        plotter.show()
        
    except Exception as e:
        print(f"3D可视化失败: {e}")

def main():
    """主函数"""
    # SDF文件路径（与Renderer.cpp中的输出路径对应）
    sdf_file = "B.raw"
    
    try:
        # 加载SDF数据
        sdf_volume = load_sdf_data(sdf_file)
        
        # 分析SDF质量
        analyze_sdf_quality(sdf_volume)
        
        # # 2D切片可视化
        # print("\n显示2D切片...")
        # visualize_2d_slices(sdf_volume)
        
        # 3D等值面可视化
        print("\n显示3D等值面...")
        visualize_3d_isosurface(sdf_volume)
        
        print("\n=== SDF验证完成 ===")
        print("验证指标:")
        print("1. 数据范围应该合理 (通常在±几个单位之内)")
        print("2. 应该有明显的内部(负值)和外部(正值)区域")
        print("3. 表面附近应该有连续的梯度变化")
        print("4. 3D等值面应该形成合理的几何体表面")
        
    except Exception as e:
        print(f"错误: {e}")
        print("\n请确保:")
        print("1. 先运行渲染器并按E键导出SDF数据")
        print("2. sdf_output.raw文件存在于当前目录")
        print("3. 已安装必要的Python包: numpy, matplotlib, pyvista")

if __name__ == "__main__":
    main()