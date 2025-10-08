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
def visualize_3d_isosurface(sdf_volume, voxel_half_extent=32, iso_value=0.0):
    """3D等值面可视化 — 显示完整体素范围 [-extent, extent]^3"""
    if not HAS_PYVISTA:
        print("跳过3D可视化 (需要安装PyVista)")
        return

    try:
        # 翻转坐标系（按需修改）
        sdf = np.flip(np.flip(sdf_volume.copy(), axis=0), axis=1)

        nx, ny, nz = sdf.shape
        origin = (-voxel_half_extent, -voxel_half_extent, -voxel_half_extent)
        spacing = (
            (2.0 * voxel_half_extent) / (nx - 1),
            (2.0 * voxel_half_extent) / (ny - 1),
            (2.0 * voxel_half_extent) / (nz - 1),
        )

        # 旧版本用 ImageData
        grid = pv.ImageData()
        grid.dimensions = (nx, ny, nz)
        grid.origin = origin
        grid.spacing = spacing

        # 保证数据对应到坐标
        grid.point_data["sdf"] = sdf.ravel(order="F")

        # 提取等值面
        surface = grid.contour(isosurfaces=[iso_value], scalars="sdf")
        if surface.n_points == 0:
            for iso_try in [0.05, 0.1, 0.2, 0.5, 1.0]:
                surface = grid.contour(isosurfaces=[iso_try], scalars="sdf")
                if surface.n_points > 0:
                    print(f"使用等值面 SDF={iso_try}")
                    break

        # 绘制
        plotter = pv.Plotter(window_size=[1000, 700])
        plotter.add_mesh(surface, color="lightblue", show_edges=False)

        # 显示完整边界框
        bbox = pv.Cube(center=(0.0, 0.0, 0.0),
                       x_length=2 * voxel_half_extent,
                       y_length=2 * voxel_half_extent,
                       z_length=2 * voxel_half_extent)
        plotter.add_mesh(bbox, style="wireframe", color="black", line_width=2)

        plotter.add_axes()
        plotter.show_bounds(all_edges=True, location="outer")
        plotter.set_background("white")

        plotter.enable_parallel_projection() 
    
        # 相机调整，确保全范围可见
        plotter.reset_camera(bounds=bbox.bounds)
        cam = plotter.camera
        cam_pos = cam.position
        cam.position = (cam_pos[0] * 1.2, cam_pos[1] * 1.2, cam_pos[2] * 1.2)
        #cam.SetClippingRange(0.1, max(1.0, 2 * voxel_half_extent) * 10.0)

        print("3D可视化窗口已打开（完整范围 [-extent,extent]^3 可见）")
        plotter.show()

    except Exception as e:
        print(f"3D可视化失败: {e}")





def apply_sign_from_reference(brute_sdf, signed_sdf):
    """使用参考有符号SDF为暴力SDF添加符号信息"""
    print("\n=== 符号信息附加 ===")

    # 创建带符号的暴力SDF
    signed_brute_sdf = brute_sdf.copy()

    # 使用signed_sdf的符号信息
    inside_mask = signed_sdf < 0  # 内部区域
    outside_mask = signed_sdf >= 0  # 外部区域

    # 为内部区域添加负号
    signed_brute_sdf[inside_mask] = -signed_brute_sdf[inside_mask]

    # 统计转换结果
    inside_count = np.sum(inside_mask)
    outside_count = np.sum(outside_mask)
    total = brute_sdf.size

    print(f"符号附加完成:")
    print(f"  内部体素: {inside_count} ({inside_count/total*100:.1f}%)")
    print(f"  外部体素: {outside_count} ({outside_count/total*100:.1f}%)")
    print(f"  原始范围: [{brute_sdf.min():.3f}, {brute_sdf.max():.3f}]")
    print(f"  附加符号后: [{signed_brute_sdf.min():.3f}, {signed_brute_sdf.max():.3f}]")

    return signed_brute_sdf

def compare_sdf_data(sdf1, sdf2, name1="SDF1", name2="SDF2"):
    """对比两个SDF数据的差异"""
    print(f"\n=== {name1} vs {name2} 对比 ===")

    # 计算差异
    diff = np.abs(sdf1 - sdf2)

    print(f"{name1} 范围: [{sdf1.min():.3f}, {sdf1.max():.3f}]")
    print(f"{name2} 范围: [{sdf2.min():.3f}, {sdf2.max():.3f}]")
    print(f"绝对差异: 平均={diff.mean():.4f}, 最大={diff.max():.4f}, 标准差={diff.std():.4f}")

    # 计算相关性
    correlation = np.corrcoef(sdf1.flatten(), sdf2.flatten())[0, 1]
    print(f"相关系数: {correlation:.4f}")

    return diff

def process_sdf_comparison(brute_file, signed_file):
    """处理SDF对比的完整流程"""
    # 加载两个SDF数据
    print("加载BruteSdf数据...")
    brute_sdf = load_sdf_data(brute_file)
    brute_sdf = np.flip(brute_sdf, axis=0) 
    brute_sdf = np.flip(brute_sdf, axis=1)

    print("\n加载参考有符号SDF数据...")
    signed_sdf = load_sdf_data(signed_file)


    # 检查尺寸匹配
    if brute_sdf.shape != signed_sdf.shape:
        raise ValueError(f"SDF尺寸不匹配: BruteSdf{brute_sdf.shape} vs SignedSdf{signed_sdf.shape}")

    # 分析原始数据
    print("\n=== 原始BruteSdf分析 ===")
    analyze_sdf_quality(brute_sdf)

    print("\n=== 参考SignedSdf分析 ===")
    analyze_sdf_quality(signed_sdf)

    # 使用参考SDF为BruteSdf添加符号
    signed_brute_sdf = apply_sign_from_reference(brute_sdf, signed_sdf)

    # 分析附加符号后的数据
    print("\n=== 附加符号后BruteSdf分析 ===")
    analyze_sdf_quality(signed_brute_sdf)

    # 对比两个有符号SDF
    diff = compare_sdf_data(signed_brute_sdf, signed_sdf, "SignedBruteSdf", "ReferenceSdf")

    # 保存结果
    output_file = "SignedBruteSdf.raw"
    signed_brute_sdf.astype(np.float32).tofile(output_file)
    print(f"\n附加符号后的SDF已保存到: {output_file}")

    return brute_sdf, signed_sdf, signed_brute_sdf, diff

def interactive_visualization(brute_sdf, signed_sdf, signed_brute_sdf, diff):
    """交互式可视化选择"""
    print("\n选择可视化数据:")
    print("1. 原始BruteSdf (仅正值)")
    print("2. 参考SignedSdf")
    print("3. 附加符号后BruteSdf")
    print("4. 差异分析")

    choice = input("请选择 (1-4): ").strip()

    if choice == "1":
        print("\n可视化原始BruteSdf...")
        visualize_3d_isosurface(brute_sdf)
    elif choice == "2":
        print("\n可视化参考SignedSdf...")
        visualize_3d_isosurface(signed_sdf)
    elif choice == "3":
        print("\n可视化附加符号后BruteSdf...")
        visualize_3d_isosurface(signed_brute_sdf)
    elif choice == "4":
        print("\n可视化差异分析...")
        visualize_2d_slices(diff, [0.5])
    else:
        print("\n默认可视化附加符号后BruteSdf...")
        visualize_3d_isosurface(signed_brute_sdf)

def print_validation_summary():
    """打印验证总结"""
    print("\n=== SDF验证完成 ===")
    print("验证指标:")
    print("1. 数据范围应该合理 (通常在±几个单位之内)")
    print("2. 应该有明显的内部(负值)和外部(正值)区域")
    print("3. 表面附近应该有连续的梯度变化")
    print("4. 3D等值面应该形成合理的几何体表面")
    print("5. 相关系数应该接近1.0表示高度相关")

def UserInterface():
    pass

def main():
    """主函数"""
    # SDF文件路径（与Renderer.cpp中的输出路径对应）
    bruteSdf = "BruteSdf.raw"
    analyticalSdf = "AnalyticalSdf.raw"
    meshToSdf = "MeshToSdf.raw"
    multiViewSdf = "MultiViewSdf.raw"
    data = load_sdf_data(meshToSdf, resolution=64)
    
    # visualize_3d_isosurface(data)
    print("\n选择可视化数据:")
    print("1. bruteSdf ")
    print("2. meshToSdf")
    print("3. analyticalSdf")
    print("4. multiViewSdf")
    choice = input("请选择 (1-4): ").strip()
    if choice == "1":
        data = load_sdf_data(bruteSdf, resolution=64)
    elif choice == "2":
        data = load_sdf_data(meshToSdf, resolution=64)
    elif choice == "3":
        data = load_sdf_data(analyticalSdf, resolution=64)
    elif choice == "4":
        data = load_sdf_data(multiViewSdf, resolution=64)
    else:
        data = load_sdf_data(bruteSdf, resolution=64)
    visualize_3d_isosurface(data)
    # try:
    #     # 处理SDF对比
    #     brute_sdf, signed_sdf, signed_brute_sdf, diff = process_sdf_comparison(sdf_file, signedSdf)

    #     # 交互式可视化
    #     interactive_visualization(brute_sdf, signed_sdf, signed_brute_sdf, diff)

    #     # 打印验证总结
    #     print_validation_summary()
        
    # except Exception as e:
    #     print(f"错误: {e}")
    #     print("\n请确保:")
    #     print("1. 先运行渲染器并按E键导出SDF数据")
    #     print("2. sdf_output.raw文件存在于当前目录")
    #     print("3. 已安装必要的Python包: numpy, matplotlib, pyvista")

if __name__ == "__main__":
    main()