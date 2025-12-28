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

try:
    from skimage.metrics import structural_similarity as ssim
    HAS_SKIMAGE = True
except ImportError:
    print("警告: 未安装scikit-image，SSIM计算将使用简化实现")
    HAS_SKIMAGE = False

def load_sdf_data(file_path, resolution=64, flip_x=False, flip_y=False, flip_z=False):
    """从二进制文件加载SDF数据

    Args:
        file_path: SDF文件路径
        resolution: 体素分辨率 (默认64)
        flip_x: 是否翻转X轴 (默认False)
        flip_y: 是否翻转Y轴 (默认False)
        flip_z: 是否翻转Z轴 (默认False)

    Returns:
        numpy数组: 加载并可选翻转后的SDF体素数据
    """
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

    # 应用坐标系翻转
    flip_info = []
    if flip_x:
        sdf_volume = np.flip(sdf_volume, axis=0)
        flip_info.append("X")
    if flip_y:
        sdf_volume = np.flip(sdf_volume, axis=1)
        flip_info.append("Y")
    if flip_z:
        sdf_volume = np.flip(sdf_volume, axis=2)
        flip_info.append("Z")

    flip_str = f" [翻转: {','.join(flip_info)}]" if flip_info else ""
    print(f"SDF数据加载成功: {resolution}³ = {len(sdf_data)} 体素 [R32_SFLOAT]{flip_str}")

    return sdf_volume

def save_sdf_data(sdf_volume, file_path):
    """将SDF数据保存到二进制文件

    Args:
        sdf_volume: SDF体素数据 (3D numpy数组)
        file_path: 保存的文件路径

    Returns:
        bool: 保存是否成功
    """
    try:
        # 确保数据类型为float32 (R32_SFLOAT)
        sdf_to_save = sdf_volume.astype(np.float32)

        # 保存到文件
        sdf_to_save.tofile(file_path)

        # 验证文件大小
        file_size = os.path.getsize(file_path)
        expected_size = sdf_to_save.size * 4  # 4 bytes per float32
        resolution = sdf_to_save.shape[0]

        if file_size == expected_size:
            print(f"SDF数据保存成功: {file_path}")
            print(f"  分辨率: {resolution}³ = {sdf_to_save.size} 体素")
            print(f"  文件大小: {file_size} bytes ({file_size / 1024:.2f} KB)")
            print(f"  数据范围: [{sdf_to_save.min():.3f}, {sdf_to_save.max():.3f}]")
            print(f"  格式: R32_SFLOAT")
            return True
        else:
            print(f"警告: 文件大小不匹配 (期望{expected_size}, 实际{file_size})")
            return False

    except Exception as e:
        print(f"保存SDF数据失败: {e}")
        return False

def abs_sdf(sdf_volume):
    """将SDF的所有负值转换为正值（取绝对值）

    Args:
        sdf_volume: SDF体素数据 (3D numpy数组)

    Returns:
        numpy数组: 转换后的SDF数据（所有值为正）
    """
    result = np.abs(sdf_volume)

    negative_count = np.sum(sdf_volume < 0)
    total_count = sdf_volume.size

    print(f"SDF负值转正完成:")
    print(f"  转换前范围: [{sdf_volume.min():.3f}, {sdf_volume.max():.3f}]")
    print(f"  转换后范围: [{result.min():.3f}, {result.max():.3f}]")
    print(f"  转换体素数: {negative_count} ({negative_count/total_count*100:.1f}%)")

    return result

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
        axes[i].set_ylabel('Y ')
        plt.colorbar(im, ax=axes[i])

    plt.tight_layout()
    plt.show()

def visualize_error_slices(error_volume, slice_positions=[0.25, 0.5, 0.75], title_prefix="误差"):
    """误差切片可视化 - 显示误差分布的2D切片

    Args:
        error_volume: 误差体素数据 (3D numpy数组)
        slice_positions: 切片位置列表 (0-1范围)
        title_prefix: 标题前缀
    """
    # 应用Y和Z轴翻转以匹配着色器坐标系
    error_flipped = error_volume.copy()

    resolution = error_flipped.shape[0]
    fig, axes = plt.subplots(1, len(slice_positions), figsize=(15, 5))

    if len(slice_positions) == 1:
        axes = [axes]

    # 使用热力图显示误差 (值越大越红)
    for i, pos in enumerate(slice_positions):
        z_index = int(pos * resolution)
        slice_data = error_flipped[:, :, z_index]

        # 使用'hot'或'YlOrRd'颜色映射,更直观地显示误差大小
        im = axes[i].imshow(slice_data, cmap='YlOrRd', origin='lower')
        axes[i].set_title(f'{title_prefix}切片 Z={z_index}/{resolution} (pos={pos:.2f})')
        axes[i].set_xlabel('X')
        axes[i].set_ylabel('Y (翻转)')

        # 添加颜色条并显示统计信息
        cbar = plt.colorbar(im, ax=axes[i])
        cbar.set_label('绝对误差')

        # 在标题中添加统计信息
        mean_err = slice_data.mean()
        max_err = slice_data.max()
        axes[i].text(0.5, -0.15, f'平均: {mean_err:.4f}, 最大: {max_err:.4f}',
                     transform=axes[i].transAxes, ha='center', fontsize=9)

    plt.tight_layout()
    plt.show()

def visualize_error_distribution(error_volume):
    """误差分布直方图和统计可视化

    Args:
        error_volume: 误差体素数据 (3D numpy数组)
    """
    error_flat = error_volume.flatten()

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    # 1. 误差直方图
    axes[0].hist(error_flat, bins=100, edgecolor='black', alpha=0.7)
    axes[0].set_xlabel('绝对误差')
    axes[0].set_ylabel('体素数量')
    axes[0].set_title('误差分布直方图')
    axes[0].grid(True, alpha=0.3)

    # 添加统计信息
    mean_err = error_flat.mean()
    median_err = np.median(error_flat)
    axes[0].axvline(mean_err, color='r', linestyle='--', label=f'平均: {mean_err:.4f}')
    axes[0].axvline(median_err, color='g', linestyle='--', label=f'中位数: {median_err:.4f}')
    axes[0].legend()

    # 2. 累积分布函数 (CDF)
    sorted_errors = np.sort(error_flat)
    cumulative = np.arange(1, len(sorted_errors) + 1) / len(sorted_errors)
    axes[1].plot(sorted_errors, cumulative, linewidth=2)
    axes[1].set_xlabel('绝对误差')
    axes[1].set_ylabel('累积概率')
    axes[1].set_title('累积分布函数 (CDF)')
    axes[1].grid(True, alpha=0.3)

    # 标记百分位点
    for p in [50, 90, 95, 99]:
        val = np.percentile(error_flat, p)
        axes[1].axvline(val, color='r', linestyle=':', alpha=0.5)
        axes[1].text(val, p/100, f'P{p}: {val:.3f}', rotation=90, va='bottom')

    # 3. 对数尺度直方图 (查看尾部分布)
    axes[2].hist(error_flat, bins=100, edgecolor='black', alpha=0.7)
    axes[2].set_xlabel('绝对误差')
    axes[2].set_ylabel('体素数量 (对数尺度)')
    axes[2].set_title('误差分布 (对数尺度)')
    axes[2].set_yscale('log')
    axes[2].grid(True, alpha=0.3)

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
        plotter.view_zx()
        plotter.camera.SetViewUp(-1.0, 0.0, 0.0) 
        # plotter.camera.Azimuth(90)
        # plotter.camera.SetViewUp((0, 0, 1))
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

def calculate_rmse(sdf1, sdf2):
    """计算均方根误差 (Root Mean Square Error)

    Args:
        sdf1: 第一个SDF数据 (通常是待评估的方法)
        sdf2: 第二个SDF数据 (通常是ground truth参考)

    Returns:
        float: RMSE值
    """
    squared_diff = (sdf1 - sdf2) ** 2
    mse = np.mean(squared_diff)
    rmse = np.sqrt(mse)
    return rmse

def calculate_mae(sdf1, sdf2):
    """计算平均绝对误差 (Mean Absolute Error)

    Args:
        sdf1: 第一个SDF数据 (通常是待评估的方法)
        sdf2: 第二个SDF数据 (通常是ground truth参考)

    Returns:
        float: MAE值
    """
    abs_diff = np.abs(sdf1 - sdf2)
    mae = np.mean(abs_diff)
    return mae

def calculate_max_error(sdf1, sdf2):
    """计算最大绝对误差 (Maximum Absolute Error)

    Args:
        sdf1: 第一个SDF数据 (通常是待评估的方法)
        sdf2: 第二个SDF数据 (通常是ground truth参考)

    Returns:
        float: 最大绝对误差值
    """
    abs_diff = np.abs(sdf1 - sdf2)
    max_error = np.max(abs_diff)
    return max_error

def calculate_psnr(image1, image2, data_range=None):
    """计算峰值信噪比 (Peak Signal-to-Noise Ratio)

    PSNR用于评估图像质量,值越高表示图像越接近参考图像
    通常用于SDFAO等渲染图像的质量评估

    Args:
        image1: 第一张图像 (通常是待评估的方法生成的图像)
        image2: 第二张图像 (通常是ground truth参考图像)
        data_range: 数据范围,如果为None则自动计算为max(image2) - min(image2)

    Returns:
        float: PSNR值 (单位: dB), 通常20-50dB之间
    """
    # 确保输入是浮点数
    image1 = image1.astype(np.float64)
    image2 = image2.astype(np.float64)

    # 计算MSE
    mse = np.mean((image1 - image2) ** 2)

    # 如果MSE为0,说明两张图像完全相同
    if mse == 0:
        return float('inf')

    # 确定数据范围
    if data_range is None:
        data_range = image2.max() - image2.min()

    # 计算PSNR
    psnr = 20 * np.log10(data_range / np.sqrt(mse))

    return psnr

def calculate_ssim(image1, image2, data_range=None, use_simple=False):
    """计算结构相似性指数 (Structural Similarity Index)

    SSIM用于评估图像质量,考虑了亮度、对比度和结构信息
    值在[-1, 1]之间,越接近1表示越相似

    Args:
        image1: 第一张图像 (通常是待评估的方法生成的图像)
        image2: 第二张图像 (通常是ground truth参考图像)
        data_range: 数据范围,如果为None则自动计算
        use_simple: 是否使用简化实现 (当scikit-image不可用时)

    Returns:
        float: SSIM值,范围[-1, 1],通常在[0, 1]之间
    """
    # 确定数据范围
    if data_range is None:
        data_range = max(image1.max(), image2.max()) - min(image1.min(), image2.min())

    # 如果有scikit-image且不强制使用简化实现,使用官方实现
    if HAS_SKIMAGE and not use_simple:
        # 对于2D图像
        if image1.ndim == 2:
            return ssim(image1, image2, data_range=data_range)
        # 对于3D体积数据,需要指定channel_axis
        else:
            # 逐切片计算SSIM并取平均
            ssim_values = []
            for i in range(image1.shape[2]):
                slice_ssim = ssim(image1[:, :, i], image2[:, :, i], data_range=data_range)
                ssim_values.append(slice_ssim)
            return np.mean(ssim_values)
    else:
        # 简化的SSIM实现 (用于没有scikit-image的情况)
        return _calculate_ssim_simple(image1, image2, data_range)

def _calculate_ssim_simple(image1, image2, data_range):
    """简化的SSIM实现

    基于SSIM的基本定义,不包含高斯滤波等高级特性
    """
    # 常数,避免除零
    C1 = (0.01 * data_range) ** 2
    C2 = (0.03 * data_range) ** 2

    # 计算均值
    mu1 = np.mean(image1)
    mu2 = np.mean(image2)

    # 计算方差和协方差
    sigma1_sq = np.var(image1)
    sigma2_sq = np.var(image2)
    sigma12 = np.mean((image1 - mu1) * (image2 - mu2))

    # 计算SSIM
    numerator = (2 * mu1 * mu2 + C1) * (2 * sigma12 + C2)
    denominator = (mu1**2 + mu2**2 + C1) * (sigma1_sq + sigma2_sq + C2)

    ssim_value = numerator / denominator

    return ssim_value

def compare_sdf_data(sdf1, sdf2, name1="SDF1", name2="true"):
    """对比两个SDF数据的差异,计算各种误差指标

    Args:
        sdf1: 第一个SDF数据 (通常是待评估的方法)
        sdf2: 第二个SDF数据 (通常是ground truth参考)
        name1: 第一个SDF的名称
        name2: 第二个SDF的名称

    Returns:
        dict: 包含误差图和各项指标的字典
    """
    print(f"\n=== {name1} vs {name2} 对比 ===")

    # 数据范围
    print(f"{name1} 范围: [{sdf1.min():.3f}, {sdf1.max():.3f}]")
    print(f"{name2} 范围: [{sdf2.min():.3f}, {sdf2.max():.3f}]")

    # 计算所有误差指标
    print("\n--- SDF数值分布误差指标 ---")

    # RMSE (均方根误差)
    rmse = calculate_rmse(sdf1, sdf2)
    print(f"RMSE (均方根误差): {rmse:.4f}")

    # MAE (平均绝对误差)
    mae = calculate_mae(sdf1, sdf2)
    print(f"MAE (平均绝对误差): {mae:.4f}")

    # 最大绝对误差
    max_error = calculate_max_error(sdf1, sdf2)
    print(f"最大绝对误差: {max_error:.4f}")

    # 计算误差分布
    diff = np.abs(sdf1 - sdf2)
    print(f"\n误差分布统计:")
    print(f"  标准差: {diff.std():.4f}")
    print(f"  中位数: {np.median(diff):.4f}")
    print(f"  90百分位: {np.percentile(diff, 90):.4f}")
    print(f"  95百分位: {np.percentile(diff, 95):.4f}")
    print(f"  99百分位: {np.percentile(diff, 99):.4f}")

    # 计算相关性
    correlation = np.corrcoef(sdf1.flatten(), sdf2.flatten())[0, 1]
    print(f"\n相关系数: {correlation:.4f}")

    # 返回结果字典
    results = {
        'diff': diff,
        'rmse': rmse,
        'mae': mae,
        'max_error': max_error,
        'correlation': correlation,
        'std': diff.std(),
        'median': np.median(diff),
        'p90': np.percentile(diff, 90),
        'p95': np.percentile(diff, 95),
        'p99': np.percentile(diff, 99)
    }

    return results

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

def load_image_data(file_path):
    """加载SDFAO渲染图像数据

    支持的格式: PNG, JPG, BMP等常见图像格式

    Args:
        file_path: 图像文件路径

    Returns:
        numpy数组,归一化到[0, 1]范围
    """
    from PIL import Image

    if not os.path.exists(file_path):
        raise FileNotFoundError(f"图像文件未找到: {file_path}")

    # 加载图像
    img = Image.open(file_path)

    # 转换为灰度图(如果是RGB)
    if img.mode != 'L':
        img = img.convert('L')

    # 转换为numpy数组并归一化到[0, 1]
    img_array = np.array(img, dtype=np.float64) / 255.0

    print(f"图像加载成功: {img_array.shape}, 范围: [{img_array.min():.3f}, {img_array.max():.3f}]")

    return img_array

def compare_sdfao_images(image1_path, image2_path, name1="方法1", name2="参考"):
    """对比两张SDFAO渲染图像的质量

    计算PSNR和SSIM指标,用于评估不同SDF生成方法的渲染质量

    Args:
        image1_path: 第一张图像路径 (待评估方法)
        image2_path: 第二张图像路径 (ground truth参考)
        name1: 第一张图像的名称
        name2: 第二张图像的名称

    Returns:
        dict: 包含PSNR和SSIM的结果字典
    """
    print(f"\n=== SDFAO图像对比: {name1} vs {name2} ===")

    # 加载图像
    image1 = load_image_data(image1_path)
    image2 = load_image_data(image2_path)
    diff = z(image1,image2)
    plt.imsave("diff.png", diff, cmap='hot')
    # 检查尺寸是否匹配
    if image1.shape != image2.shape:
        raise ValueError(f"图像尺寸不匹配: {image1.shape} vs {image2.shape}")

    # 计算PSNR
    psnr = calculate_psnr(image1, image2, data_range=1.0)
    print(f"\nPSNR (峰值信噪比): {psnr:.2f} dB")
    print(f"  - 参考范围: 20-30dB (可接受), 30-40dB (良好), >40dB (优秀)")

    # 计算SSIM
    ssim_value = calculate_ssim(image1, image2, data_range=1.0)
    print(f"\nSSIM (结构相似性): {ssim_value:.4f}")
    print(f"  - 参考范围: 0.8-0.9 (可接受), 0.9-0.95 (良好), >0.95 (优秀)")

    # 返回结果
    results = {
        'psnr': psnr,
        'ssim': ssim_value,
        'image1': image1,
        'image2': image2
    }

    return results

def z(image1, image2, name1="方法1", name2="参考"):
    """可视化SDFAO图像对比

    Args:
        image1: 第一张图像数组
        image2: 第二张图像数组
        name1: 第一张图像的名称
        name2: 第二张图像的名称
    """
    # 计算差异图
    diff = np.abs(image1 - image2)
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 12))

    # 图1: 第一张图像
    im1 = axes[0, 0].imshow(image1, cmap='gray', vmin=0, vmax=1)
    axes[0, 0].set_title(f'{name1}')
    axes[0, 0].axis('off')
    plt.colorbar(im1, ax=axes[0, 0])

    # 图2: 第二张图像
    im2 = axes[0, 1].imshow(image2, cmap='gray', vmin=0, vmax=1)
    axes[0, 1].set_title(f'{name2}')
    axes[0, 1].axis('off')
    plt.colorbar(im2, ax=axes[0, 1])

    # 图3: 差异图
    im3 = axes[1, 0].imshow(diff, cmap='hot', vmin=0, vmax=diff.max())
    axes[1, 0].set_title(f'绝对差异 (最大: {diff.max():.4f})')
    axes[1, 0].axis('off')
    plt.colorbar(im3, ax=axes[1, 0])

    # 图4: 差异直方图
    axes[1, 1].hist(diff.flatten(), bins=100, edgecolor='black', alpha=0.7)
    axes[1, 1].set_xlabel('像素差异')
    axes[1, 1].set_ylabel('像素数量')
    axes[1, 1].set_title('差异分布直方图')
    axes[1, 1].grid(True, alpha=0.3)

    # 添加统计信息
    mean_diff = diff.mean()
    median_diff = np.median(diff)
    axes[1, 1].axvline(mean_diff, color='r', linestyle='--', label=f'平均: {mean_diff:.4f}')
    axes[1, 1].axvline(median_diff, color='g', linestyle='--', label=f'中位数: {median_diff:.4f}')
    axes[1, 1].legend()

    plt.tight_layout()
    plt.show()
    return diff



def UserInterface():
    pass

def Visualize(bruteSdf,meshToSdf,analyticalSdf,multiViewSdf,resolution):
    print("\n选择可视化数据:")
    print("1. bruteSdf ")
    print("2. meshToSdf")
    print("3. analyticalSdf")
    print("4. multiViewSdf")
    choice = input("请选择 (1-4): ").strip()
    if choice == "1":
        data = bruteSdf
    elif choice == "2":
        data = meshToSdf
    elif choice == "3":
        data = analyticalSdf
    elif choice == "4":
        data = multiViewSdf
    else:
        data = bruteSdf
    visualize_3d_isosurface(data,voxel_half_extent=resolution/2)
def main():
    """主函数"""
    # SDF文件路径（与Renderer.cpp中的输出路径对应）
    # bruteSdf = "duck_4k_64_BruteSdf.raw"
    # analyticalSdf = "duck_4k_64_Analytical.raw"
    # meshToSdf = "duck_4k_64_JumpFlood.raw"
    # multiViewSdf = "duck_4k_64_Multview.raw"
    
    modelName = "rock_"
    resolution = 128
    modelName = modelName + str(resolution) +"_"
    methodName1 = "BruteSdf"
    methodName2 = "JumpFlood"
    methodName3 = "Analytical"
    methodName4 = "Multview"
    appendix = ".raw"

    default = "duck_4k_64_Multview.raw"
    fileTrue = modelName+ methodName1 + appendix
    file2 = modelName +methodName2 + appendix
    file3 = modelName +methodName3 + appendix
    file4 = modelName +methodName4 + appendix
    # file4 = "happy_15k_128_Multview.raw"
    dataTrue = load_sdf_data(fileTrue,resolution=resolution) 
    data2 = load_sdf_data(file2,resolution=resolution,flip_x=True,flip_y=True)
    data3 = load_sdf_data(default,resolution=resolution)
    data3 = abs_sdf(data3)
    data4 = load_sdf_data(file4,resolution=resolution)
    data4 = abs_sdf(data4)
    # diffSdf = data4 - dataTrue
    # visualize_error_distribution(diffSdf)
    # visualize_2d_slices(dataTrue,[0.5,0.5,0.5])
    # visualize_2d_slices(data4,[0.5,0.5,0.5])
    # visualize_error_slices(diffSdf,[0.25,0.5,0.75])
    # compare_sdfao_images("happy_15k_128_AO1.png","happy_15k_128_AO_brute1.png")
    # compare_sdf_data(dataTrue,data4,"meshtoSDf")
    # compare_sdf_data(data3,dataTrue,"Analytical")
    compare_sdf_data(data4,dataTrue,"MultiView")
    # visualize_3d_isosurface(data4,resolution/2)
    Visualize(dataTrue,data2,data3,data4,resolution)
    #save_sdf_data(dataTrue,fileTrue)




if __name__ == "__main__":
    main()