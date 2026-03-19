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
    

def main():
    """主函数"""
    # SDF文件路径（与Renderer.cpp中的输出路径对应）
    # bruteSdf = "duck_4k_64_BruteSdf.raw"
    # analyticalSdf = "duck_4k_64_Analytical.raw"
    # meshToSdf = "duck_4k_64_JumpFlood.raw"
    # multiViewSdf = "duck_4k_64_Multview.raw"
    
    modelName = "barrel_"
    resolution = 64
    modelName = modelName + str(resolution) +"_"
    methodName1 = "BruteSdf"
    methodName2 = "JumpFlood"
    methodName3 = "Analytical"
    methodName4 = "Multview"
    methodName5 = "NGP"
    methodName6 = "Heat"
    appendix = ".raw"

    # default = "duck_4k_64_Multview.raw"
    fileTrue = modelName+ methodName1 + appendix
    file2 = modelName +methodName2 + appendix
    # file3 = modelName +methodName3 + appendix
    file4 = modelName +methodName4 + appendix
    file5 = modelName +methodName5 + appendix
    file6 = modelName +methodName6 + appendix

    # dataTrue = load_sdf_data(fileTrue,resolution=resolution) 
    data5 = load_sdf_data(file5,resolution=resolution,flip_x=True,flip_y=True)
    data6 = load_sdf_data(file6,resolution=resolution,flip_x=True,flip_y=True)
    save_sdf_data(data5,file5)
    save_sdf_data(data6,file6)




if __name__ == "__main__":
    main()