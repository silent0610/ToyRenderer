import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.colors as mcolors
import numpy as np
import os
import math

# 尝试导入 SSIM 计算库
try:
    from skimage.metrics import structural_similarity as ssim_func
    HAS_SKIMAGE = True
except ImportError:
    HAS_SKIMAGE = False
    print("提示: 未安装 scikit-image，将只计算 PSNR 和 MAE。")

# =========================================================================
#                          用户配置区域 (USER CONFIG)
# =========================================================================

# --- 1. 输入图片路径 ---
GT_PATH   = "rock_128_GT.png"    # Ground Truth
OURS_PATH = "rock_128_MV.png"      # 本文方法
OURS_TIME = "0.71 ms" 


COMPARE_METHODS = [
    ("JFA", "rock_128_JFA.png", "6.60 ms"),       # 直接指定单位 s
]

DEFAULT_TIME_UNIT = "ms" # 如果上面的时间填的是纯数字，则默认用这个单位


CROP_REGIONS = [
    (325, 495, 100, 100),
    (875, 435, 100, 100),
    (590, 900, 100, 100)
]



OUTPUT_PATH = "Compare/rock_128.png"
DPI         = 300 


VMAX_ERROR  = 0.8  
GAMMA_VALUE = 0.8   


CROP_SIZE_INCH  = 2.5 
MAIN_VIEW_RATIO = 4    
BASE_FONT_SIZE  = 18   

# =========================================================================
#                       以下是核心逻辑 
# =========================================================================

def load_image(path):
    # 检查路径是否存在
    if not os.path.exists(path):
        # Python 使用 raise 抛出异常，f-string 用于格式化错误信息
        raise FileNotFoundError(f"错误: 找不到文件 -> {path}")
    img = plt.imread(path)
    # 归一化: 如果是 uint8 (0-255), 转换为 float32 (0.0-1.0)
    if img.dtype == np.uint8:
        img = img.astype(np.float32) / 255.0
    # 去除 Alpha 通道: 如果是 RGBA (4通道), 转为 RGB (3通道)
    if img.ndim == 3 and img.shape[2] == 4:
        img = img[:, :, :3]
        
    return img

def calculate_metrics(img, gt):
    """计算 PSNR, SSIM, MAE 和 Max Error"""
    diff = np.abs(img - gt)
    
    # 1. MAE (平均误差)
    mae = np.mean(diff)
    
    # 2. [新增] Max Error (最大误差)
    max_error = np.max(diff)

    # 3. PSNR
    mse = np.mean(diff ** 2)
    if mse == 0:
        psnr = float('inf')
    else:
        psnr = 20 * math.log10(1.0 / math.sqrt(mse))
    
    # 4. SSIM
    ssim_val = 0.0
    if HAS_SKIMAGE:
        ssim_val = ssim_func(gt, img, data_range=1.0, channel_axis=2, win_size=11)
        
    return psnr, ssim_val, mae, max_error

def format_label(psnr, ssim, mae, time_val=None):
    """生成智能标签: 自动识别时间是字符串还是数字"""
    line1 = f"PSNR: {psnr:.2f} dB  SSIM: {ssim:.4f}"
    
    line2 = f"MAE: {mae:.4f}"
    
    if time_val is not None:
        # [修改点]: 智能判断类型
        if isinstance(time_val, str):
            # 如果是字符串 (如 "45.2s")，直接显示
            line2 += f"  Time: {time_val}"
        else:
            # 如果是数字 (如 45.2)，加上默认单位
            line2 += f"  Time: {time_val}{DEFAULT_TIME_UNIT}"
        
    return f"{line1}\n{line2}"

def make_teaser_plot():
    print(f"正在生成对比图...")

    img_gt = load_image(GT_PATH)
    img_ours = load_image(OURS_PATH)
    
    psnr_ours, ssim_ours, mae_ours,maxErrorOur = calculate_metrics(img_ours, img_gt)
    print("ours max error ",maxErrorOur)
    diff_ours = np.abs(img_ours - img_gt)
    if diff_ours.ndim == 3: diff_ours = np.mean(diff_ours, axis=2)

    # --- 构建数据列 ---
    grid_columns = [
        {"name": "GT", "img": img_gt, "type": "rgb", "label": ""}
    ]
    
    # Ours Label (传入 flexible 的时间)
    ours_label_str = format_label(psnr_ours, ssim_ours, mae_ours, OURS_TIME)
    
    grid_columns.append({"name": "Ours", "img": img_ours, "type": "rgb", "label": ours_label_str})
    grid_columns.append({"name": "Ours Error", "img": diff_ours, "type": "error", "label": ""})

    for method_item in COMPARE_METHODS:
        # 兼容性处理: 允许只传2个参数，或者传3个参数
        if len(method_item) == 3:
            method_name, method_path, method_time = method_item
        else:
            method_name, method_path = method_item
            method_time = None

        img_m = load_image(method_path)
        psnr_m, ssim_m, mae_m, maxError = calculate_metrics(img_m, img_gt)
        print(method_item[0]," max error ",maxErrorOur)
        diff_m = np.abs(img_m - img_gt)
        if diff_m.ndim == 3: diff_m = np.mean(diff_m, axis=2)
        
        # 传入 method_time (可以是字符串，也可以是数字)
        label_text = format_label(psnr_m, ssim_m, mae_m, method_time)
        
        grid_columns.append({"name": method_name, "img": img_m, "type": "rgb", "label": label_text})
        grid_columns.append({"name": "Error", "img": diff_m, "type": "error", "label": ""})

    # --- 布局计算 ---
    n_grid_cols = len(grid_columns)
    n_rows = 3
    
    dynamic_font_scale = 1.0 + max(0, (n_grid_cols - 4) * 0.05)
    title_fs = 30
    label_fs = 20
    
    total_width = (MAIN_VIEW_RATIO + n_grid_cols) * CROP_SIZE_INCH
    total_height = n_rows * CROP_SIZE_INCH * 1.45 
    
    fig = plt.figure(figsize=(total_width, total_height), dpi=DPI)
    
    gs = fig.add_gridspec(
        n_rows, 
        1 + n_grid_cols, 
        width_ratios=[MAIN_VIEW_RATIO] + [1] * n_grid_cols, 
        wspace=0.05, 
        hspace=0.35 
    )

    box_colors = ['#FF0000', '#00FF00', '#00FFFF']
    norm = mcolors.PowerNorm(gamma=GAMMA_VALUE, vmin=0.0, vmax=VMAX_ERROR)
    cmap = 'inferno'
    im_error_dummy = None 

    # --- 左侧大图 ---
    ax_main = fig.add_subplot(gs[:, 0])
    ax_main.imshow(img_ours)
    ax_main.set_title("Ours", fontsize=title_fs+2, fontweight='bold', pad=15)
    ax_main.axis('off')
    
    for i, (x, y, w, h) in enumerate(CROP_REGIONS):
        color = box_colors[i]
        rect = patches.Rectangle((x, y), w, h, linewidth=3, edgecolor=color, facecolor='none')
        ax_main.add_patch(rect)

    # --- 右侧网格 ---
    for row_idx in range(3):
        x, y, w, h = CROP_REGIONS[row_idx]
        border_color = box_colors[row_idx]

        for col_idx, col_data in enumerate(grid_columns):
            ax = fig.add_subplot(gs[row_idx, col_idx + 1])
            
            img_data = col_data["img"]
            h_img, w_img = img_data.shape[:2]
            crop_img = img_data[max(0, y):min(h_img, y+h), max(0, x):min(w_img, x+w)]

            if col_data["type"] == "error":
                im = ax.imshow(crop_img, cmap=cmap, norm=norm)
                im_error_dummy = im 
            else:
                ax.imshow(crop_img)

            for spine in ax.spines.values():
                spine.set_edgecolor(border_color)
                spine.set_linewidth(2.5)
            
            ax.set_xticks([])
            ax.set_yticks([])

            if row_idx == 0:
                ax.set_title(col_data["name"], fontsize=title_fs, fontweight='bold')
            
            if row_idx == 2 and col_data["label"]: 
                ax.set_xlabel(col_data["label"], fontsize=label_fs-1, labelpad=8, color='#000000', linespacing=1.4)

    # --- Colorbar ---
    plt.subplots_adjust(left=0.02, right=0.95, top=0.92, bottom=0.10)
    cbar_ax = fig.add_axes([0.96, 0.35, 0.012, 0.3])
    
    if im_error_dummy:
        cb = fig.colorbar(im_error_dummy, cax=cbar_ax)
        cb.set_label(f'Abs. Error (Gamma={GAMMA_VALUE})', fontsize=label_fs)
        cb.ax.tick_params(labelsize=label_fs-2)
    else:
        cbar_ax.axis('off')

    plt.savefig(OUTPUT_PATH, bbox_inches='tight')
    print(f"✅ 完成! 图片已保存至: {OUTPUT_PATH}")
    plt.close()

if __name__ == "__main__":
    # 测试数据
    if not os.path.exists(GT_PATH):
        print("⚠️ 生成随机测试图...")
        dummy = np.random.rand(512, 512, 3)
        plt.imsave(GT_PATH, dummy)
        plt.imsave(OURS_PATH, dummy * 0.98 + 0.01)
        plt.imsave("AO_JFA.png", dummy * 0.8)

    make_teaser_plot()