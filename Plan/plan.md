# Thingi10K 水密子集批量对比

目标没变：用 Thingi10K 的水密子集画对数坐标的速度图，比 JFA、BVH 和 MultiView。主图是三角面数增加时的预处理时间和求值时间，分辨率固定在 \(128^3\)。另外两张图分开画，不和这张散点叠在一起。

ABC（`E:\all\Projects\retrieve`，块 53）和 Fusion 360 Gallery（`E:\all\Projects\r1.0.1`，构造序列 JSON）继续作为论文里的工业例子，不进入这张散点图。

## 当前进度

渲染器里的预处理计时已经能用。一次进程读一份模型清单，Vulkan 只初始化一次，后面的模型换网格并重录命令缓冲。每个模型自己先丢掉 `--warmup` 帧，再对 `--repeat` 帧取平均。默认都是 50。交互模式的 `Run()` 没有改。

已经接上的是 JFA（现有 MeshToSdf）和 MultiView（现有 Unified Pipeline）。时间是这两段 GPU timestamp，不含加载模型、建窗口和交换链。BVH 只有接口，CSV 状态是 `skipped`。求值时间 `eval_ms` 还是空的，没有 4096 点采样。

还没做：Thingi10K 筛选和 glTF 转换、画图、中位数列、SDF 误差（RMSE、MAE、MaxAE、P95、P99、PCC）。误差只比 SDF 体素场和暴力真值，DFAO 的 PSNR、SSIM、图像 MAE 先不做。误差要在计时结束之后算，不写入 `precomp_ms`。

## 成果

下面这组数来自同一次进程、分辨率 \(128^3\)、每个模型 1 帧预热加 1 帧计时。它说明清单模式和两条计时都通了，不是现在默认的 50 帧平均，不能直接放进论文。

| 模型 | 三角面 | JFA (ms) | MultiView (ms) |
|---|---:|---:|---:|
| model3 | 128 | 5.81 | 1.69 |
| barrel | 768 | 5.71 | 1.71 |
| model2 | 1832 | 5.64 | 2.71 |
| rock | 2136 | 7.25 | 1.80 |
| bunnyfixed3k | 2944 | 5.69 | 1.73 |
| happy_15k | 14765 | 5.77 | 3.09 |

BVH 三行都是 `skipped`，`eval_ms` 为空。面数是索引缓冲长度除以 3。单独冷启动时同一模型的 JFA 可以到二十多毫秒，和上表不是同一协议。

之后用 `--warmup 2 --repeat 3` 跑过 `model3`，日志是 `frames=3`，预热帧没有进平均。默认 50/50 的正式表还没跑。

## 如何使用

在仓库根目录编译：

```text
xmake build
```

程序是 `Build/windows/x64/release/MyToyRenderer.exe`。模型路径和 `Config.json5` 的 `modelPath` 一样，相对资源目录，例如 `Models/checked/rock.gltf`。

一个模型：

```text
Build\windows\x64\release\MyToyRenderer.exe --batch Models/checked/rock.gltf --out Build/bench.csv --resolution 128 --warmup 50 --repeat 50
```

一份清单，一次进程跑完。清单里一行一个路径，`#` 后面是注释。

```text
Build\windows\x64\release\MyToyRenderer.exe --batch-list Build/bench_models.txt --out Build/bench.csv --warmup 50 --repeat 50
```

Python 只负责写清单并启动这一次进程。在仓库根目录：

```text
python Renderer/Asset/Sdf/DatasetBench/run_bench.py Models/checked/rock.gltf Models/checked/barrel.gltf --warmup 50 --repeat 50
```

默认可执行文件、清单和 CSV 分别是 `Build/windows/x64/release/MyToyRenderer.exe`、`Build/bench_models.txt`、`Build/bench.csv`。`--queries` 先记在 CSV 里，求值还没用到它。

程序启动时仍整份读取项目根目录的 `Config.json5`。批量模式只覆盖四项：`modelPath`（清单第一个，之后每个模型再换）、`VoxelResolution`、`SdfResolution`，以及把 `DynamicGeometry.enable` 设为 false。相机数量上限、深度分辨率、八叉树采样层、JFA 模式和质量、`WorldSize`、`SdfMode`、校验层仍按 JSON 生效。命令行再带一个 `v` 会关掉校验层。

CSV 追加写入。文件不存在或为空时先写表头。每个模型三行：

```text
model,triangles,method,resolution,queries,precomp_ms,eval_ms,eval_per_query_us,status
```

`precomp_ms` 是正式帧的平均建场时间。JFA 和 MultiView 在采样数等于 `--repeat` 时状态为 `precomp_only`，否则为 `error`。空的求值列留空，不写 0。跑完后进程用 `std::_Exit` 退出；短跑之后调用现有 `Cleanup()` 会在堆检查处中止，交互模式关闭窗口仍走 `Cleanup()`。

窗口仍会出现。深度间接绘制目前只画第一个部件，多部件模型的 MultiView 计时还不是整模。清单里的第一个模型可能略慢，时间戳不含 CPU 初始化，但 GPU 时钟还在爬升。

## 分辨率和相机数的 log-log 图

和面数散点分开。协议仍是每个设定 50 帧预热、50 帧平均。模型用同一批水密子集里的代表，不必 185 个全跑。这两张还没跑。现有的 \(128^3\) 面数图不能代替它们。

- **固定 \(K\)，变化 \(N\)。** \(N = 64, 128, 256, 512, 1024\)。\(K\) 取一个固定预算，和正文主实验一致，并在图注里写明。
- **固定 \(N\)，变化 \(K\)。** \(N = 128\)，\(K = 1, 2, 4, 8, 16, 32\)。

对比方法是本方法、JFA、TCKB22、Barill。每条曲线报告预处理、深度渲染、融合、查询、总时间，以及内存占用和时间分解。JFA 没有深度渲染和融合，这两列留空。TCKB22 和 Barill 还没有实现。

## 复杂度说法

算法 1 是沿 \(z\) 的并行前缀扫描，工作量和 \(N^3\) 成正比，循环里没有相机数 \(K\)。紧挨着的那段把体素填充写成 \(O(KN^3)\)，需要改掉。

\(K\) 出现在后面两个阶段：深度渲染大约是 \(O(KM)\)，\(M\) 是三角面数；融合阶段每个体素查看 \(K\) 个深度，大约是 \(O(KN^3)\)。总代价写成 \(O(KM + KN^3)\)，不要把 \(KN^3\) 安到算法 1 上。

用上面两张 log-log 图检验这个说法。若固定 \(K\) 增大 \(N\) 时，本方法并没有比 JFA 的 \(N^3 \log N\) 更慢得少，就不要再声称渐近复杂度更好。改成优势来自 GPU 光栅化并行、内存访问更集中，以及保守的距离界。面数图已经说明：在固定的 \(128^3\) 上，高面数段 MultiView 接近随 \(M\) 线性变慢，这只支持深度渲染里的 \(KM\) 项，不能单独证明 \(KN^3\) 优于 \(N^3 \log N\)。

## 数据

用 Thingi10K 官方标记筛选，不自己重算拓扑：`solid=True`、`num_components=1`、`closed=True`、`manifold=True`。按三角面数对数分桶，从 \(10^2\) 到库里的最大面数，每桶抽同样数量，先凑大约 200 个。STL 转成 glTF，归一化到 \([-1,1]^3\)。厚度和内部相机覆盖率第一轮不挡运行，跑完后把深度大面积失效的模型记进失败列表。图注写过滤条件，以及 10,000 个模型里最终保留了多少。

## 不做的事

- 不在 Python 里重写 MultiView 或 JFA。
- 不用 ABC 的 `trimesh` obj 或 Fusion 360 的构造序列 JSON 填这张速度图。
- 不把完整 10,000 个未过滤模型放进主曲线。
- 面数散点的第一轮可以先不接 Barill。分辨率和相机数那两张 log-log 图要加入 Barill 和 TCKB22。
- 不做 DFAO、PSNR、SSIM 和图像 MAE。
