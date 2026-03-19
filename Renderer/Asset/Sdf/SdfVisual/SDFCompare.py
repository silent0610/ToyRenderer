import matplotlib.pyplot as plt
import numpy as np
import os

# =========================================================================
#                          用户配置区域 (USER CONFIG)
# =========================================================================

# --- 1. 定义 3 个模型 (对应 3 行) ---
# 列表里的名字用于显示，也用于拼接文件名
MODELS = ["rock", "barrel", "happy"] 

# --- 2. 定义 4 种方法 (对应 4 列) ---
# 建议把 GT 放在第一位或最后一位
METHODS = ["GT", "Ours", "NGP", "Heat"]

# --- 3. 图片文件命名规则 ---
# 脚本会根据上面的名字自动拼接路径。
# 例如: 模型="Dragon", 方法="Ours" -> 寻找 "Dragon_Ours.png"
# {model} 会被替换为模型名, {method} 会被替换为方法名
FILE_PATTERN = "{model}_64_{method}.png" 

# --- 4. 文件夹路径 ---
IMAGE_FOLDER = "./"  # 图片存放的文件夹，"." 表示当前目录

# --- 5. 排版设置 ---
OUTPUT_PATH = "grid_comparison_12.png"
FIG_SIZE    = (16, 10)  # 画布大小 (宽, 高)
FONT_SIZE   = 16        # 字体大小

# =========================================================================

def load_image(path):
    """读取图片，如果不存在则生成一个带文字的空白图"""
    if os.path.exists(path):
        img = plt.imread(path)
        # 处理 RGBA -> RGB
        if img.ndim == 3 and img.shape[2] == 4:
            img = img[:, :, :3]
        return img
    else:
        print(f"⚠️ 缺失图片: {path}")
        # 生成一个灰色占位图
        dummy = np.ones((512, 512, 3)) * 0.9 
        # 画个红叉或留白 (这里简单返回灰色)
        return dummy

def make_grid_plot():
    n_rows = len(MODELS)    # 3
    n_cols = len(METHODS)   # 4

    # 创建子图网格
    # constrained_layout=True 会自动调整间距，防止标签重叠
    fig, axes = plt.subplots(n_rows, n_cols, figsize=FIG_SIZE, constrained_layout=True)

    print(f"正在生成 {n_rows}x{n_cols} 对比图...")

    # 遍历行 (Model)
    for row_idx, model_name in enumerate(MODELS):
        # 遍历列 (Method)
        for col_idx, method_name in enumerate(METHODS):
            
            # 1. 拼凑文件名
            filename = FILE_PATTERN.format(model=model_name, method=method_name)
            filepath = os.path.join(IMAGE_FOLDER, filename)
            
            # 2. 读取图片
            img = load_image(filepath)
            
            # 3. 绘图
            ax = axes[row_idx, col_idx]
            ax.imshow(img)
            
            # 去掉刻度
            ax.set_xticks([])
            ax.set_yticks([])
            
            # --- 设置标签 ---
            
            # 仅在【第一行】显示方法名 (Top Labels)
            if row_idx == 0:
                ax.set_title(method_name, fontsize=FONT_SIZE+2, fontweight='bold', pad=10)
            
            # 仅在【第一列】左侧显示模型名 (Left Labels)
            if col_idx == 0:
                # 使用 set_ylabel 显示在左侧
                ax.set_ylabel(model_name, fontsize=FONT_SIZE+2, fontweight='bold', labelpad=10)

            # 边框美化 (可选: 给 Ours 加个红框?)
            # if method_name == "Ours":
            #     for spine in ax.spines.values():
            #         spine.set_edgecolor('red')
            #         spine.set_linewidth(3)

    # 保存
    plt.savefig(OUTPUT_PATH, dpi=300)
    print(f"✅ 完成！图片已保存至: {OUTPUT_PATH}")
    plt.show()

# =========================================================================
#                          自动生成测试数据 (可选)
# =========================================================================
def generate_dummy_data():
    """如果没有真实图片，生成一些假图片用于测试脚本"""
    if not os.path.exists(IMAGE_FOLDER):
        os.makedirs(IMAGE_FOLDER)
        
    for model in MODELS:
        for method in METHODS:
            filename = FILE_PATTERN.format(model=model, method=method)
            path = os.path.join(IMAGE_FOLDER, filename)
            if not os.path.exists(path):
                # 生成随机颜色的图
                color = np.random.rand(3)
                img = np.ones((256, 256, 3)) * color
                plt.imsave(path, img)
    print("已生成测试图片，请替换为真实文件。")

if __name__ == "__main__":
    # 如果你还没有图片，取消下面这行的注释来生成假数据测试
    # generate_dummy_data()
    
    make_grid_plot()