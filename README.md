# Shapr3D 桌面版(开源实现)

一个参考 Shapr3D 交互范式的**桌面端参数化三维 CAD**:

- **内核**: Open CASCADE Technology (OCCT) —— 精确 B-Rep 实体建模
- **草图**: 自研几何约束求解器(Levenberg–Marquardt + 自由度/过约束检测)
- **渲染**: OpenGL ES 2.0(自动回退桌面 GL)自研渲染管线 —— 影棚式 PBR 视口、软地面阴影、MSAA
- **并行**: 多线程 CPU(特征/实体级并行重算、OCCT 并行网格剖分、并行 HLR 投影)
- **本地**: 建模、工程图、数据交换全部在本机完成, 不依赖任何云服务

> 说明: 本项目是对 Shapr3D 核心能力的开源学习性实现, 与 Shapr3D 官方无关;
> 商业软件的功能广度不可能被单个项目 1:1 复刻, 本仓库交付的是下述功能支柱的**可用工程实现**。

## 功能支柱

| 支柱 | 实现 |
| --- | --- |
| **精确实体建模** | OCCT B-Rep: 长方体/圆柱/球/圆环/圆锥台, 草图拉伸/旋转, 布尔并/差/交, 圆角/倒角, 抽壳, 变换 |
| **参数化草图** | 直线/矩形/圆/三点弧/多边形; 顶点拖拽实时求解; 端点/网格吸附; 链式绘图 |
| **约束求解器(自研)** | 重合/水平/垂直/平行/垂直/距离/点线距/长度/半径/直径/角度/相等/中点/点在线上/点在圆上/同心/固定; LM 阻尼最小二乘, 数值雅可比, 高斯消元; Gram-Schmidt 秩估计 → 自由度统计与过约束告警 |
| **工程制图** | HLR 隐藏线消除 → 主视/俯视/左视/轴测 四视图自动布局 + 图框标题栏; **多线程并行投影** |
| **2D 制图** | 制图模式: 直线/圆/尺寸/注记工具, 画布平移缩放 |
| **同步建模** | 拖面(Push-Pull): 按住平面拖动, 向外加材料/向内切除; 特征树上随时改参数全链重算 |
| **测量与分析** | 距离(点/棱/面任意组合)、平面夹角、体积/表面积/质心/包围盒/质量(按材质密度) |
| **工程数据交换** | 导入 STEP/IGES/BREP/STL(自动形状修复); 导出 STEP/IGES/BREP/STL/OBJ; 工程图导出 SVG/DXF(R12)/矢量 PDF(内嵌 Noto Sans SC 子集, 中文标题/标注完整渲染) |
| **仿真渲染效果** | 影棚三点光 + 半球环境光 + 简化 GGX 高光 + 菲涅尔边缘光 + 金属度/粗糙度材质 + 高斯模糊软地面阴影(FBO 两趟分离模糊) + MSAA |
| **文档** | `.scn` 工程文件(JSON)全量参数化持久化; 撤销/重做(快照式, 64 步) |

## 界面

- 深色 Shapr3D 风格 UI(中文), 左侧工具条 + 右侧特征树/约束/材质面板 + 状态栏
- 三大模式: **建模 / 草图 / 制图**

### 快捷键

| 键 | 功能 | 键 | 功能 |
| --- | --- | --- | --- |
| `S` | 进入草图(若有选中面则贴面) | `E` | 拉伸 |
| `R` | 旋转(草图模式为矩形) | `F` | 圆角 |
| `C` | 倒角(草图模式为圆) | `T` | 抽壳 |
| `B` | 布尔并 | `P` | 同步拖面 |
| `M` | 测量 | `G` | 网格吸附开关 |
| `1/2/3/0` | 前/上/左/轴测视图 | `Home` | 全部显示 |
| `Ctrl+Z / Ctrl+Y` | 撤销/重做 | `Ctrl+S` | 保存工程 |
| `ESC` | 取消/返回选择 | `Del` | 清除选择 |
| 鼠标右键拖动 | 旋转视角 | 中键拖动 / Shift+右键 | 平移 |
| 滚轮 | 缩放 | Shift+点击 | 加选 |

## 构建依赖

- C++17 编译器 (GCC≥9 / Clang≥10 / MSVC 2019+)
- CMake ≥ 3.16(高版本 CMake 4.x 需加 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 配老 OCCT)
- **Open CASCADE Technology 7.4 ~ 7.8**(系统包或源码构建)
- 窗口: GLFW 3.3+(仓库脚本自动拉取源码一起编译)
- UI: Dear ImGui(脚本自动拉取); 中文字体 SimHei(脚本自动拉取, 可替换)

### Linux

```bash
# 依赖(以 Debian/Ubuntu 为例)
sudo apt install build-essential cmake ninja-build libocct-foundation-dev \
    libocct-modeling-data-dev libocct-modeling-algorithms-dev \
    libocct-data-exchange-dev libocct-application-framework-dev

# 构建
./scripts/build.sh
# 或指定 OCCT 路径
OCCT_DIR=/path/to/occt/build ./scripts/build.sh

./build/bin/shapr3d
```

### Windows (vcpkg)

```powershell
vcpkg install occt glfw3
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\bin\Release\shapr3d.exe
```

### 从源码构建 OCCT(无包管理器环境)

```bash
git clone --depth 1 --branch V7_6_3 https://github.com/Open-Cascade-SAS/OCCT.git
cmake -S OCCT -B OCCT/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DBUILD_MODULE_FoundationClasses=ON -DBUILD_MODULE_ModelingData=ON \
    -DBUILD_MODULE_ModelingAlgorithms=ON -DBUILD_MODULE_DataExchange=ON \
    -DBUILD_MODULE_ApplicationFramework=ON \
    -DBUILD_MODULE_Visualization=OFF -DBUILD_MODULE_Draw=OFF \
    -DBUILD_LIBRARY_TYPE=Shared -DUSE_FREETYPE=OFF
cmake --build OCCT/build -j$(nproc)
OCCT_DIR=$PWD/OCCT/build ./scripts/build.sh
```

## 无头测试(CI/无 GPU 环境)

```bash
cmake -B build -DBUILD_APP=OFF -DOpenCASCADE_DIR=... && cmake --build build
./build/bin/test_solver          # 约束求解器: 收敛性/拖拽/过约束/自由度
./build/bin/test_modeling        # 建模内核: 基本体/拉伸/布尔/圆角/抽壳/网格/序列化
./build/bin/test_drawing_io      # 工程图 + 数据交换: HLR/SVG/DXF/PDF/STEP往返
./build/bin/demo_headless out/   # 端到端演示, 输出 STEP/IGES/STL/OBJ/DXF/SVG/PDF
```

当前状态: 117 项断言全部通过 (solver 19 / modeling 37 / drawing_io 40 / demo 21),
STEP 往返体积偏差 0.000000%,PDF 内嵌中文 (Noto Sans SC 子集, OFL), 含参数化编辑(同步建模)端到端演示。

## 架构

```
src/
├── core/      通用: 日志/计时/线程池(ThreadPool)/JSON 持久化
├── kernel/    OCCT 封装: Feature 特征系统 · Document 文档/重算调度 · MeshBuilder 网格剖分
├── sketch/    草图数据模型 + 自研约束求解器 (LM + 秩估计)
├── render/    GLES2 渲染: 零依赖 GL 加载器 · 影棚着色器 · 阴影 FBO · PNG 截图
├── app/       Application 状态机(拾取/工具/交互) · Ui 面板 · ImGui 后端
├── drawing/   HLR 工程图生成 + SVG/DXF/PDF 矢量导出
├── io/        STEP/IGES/BREP/STL/OBJ 数据交换
└── analysis/  测量与质量属性(BRepExtrema/BRepGProp)
```

### 多线程说明

- `ThreadPool`(std::thread + work-stealing 队列): 按实体并行重算 + 并行网格化 + 并行 HLR 视图
- OCCT `IMeshTools_Parameters::InParallel = true`: 单实体内部按面并行三角化
- 渲染与显示全部走 GPU(OpenGL ES 2.0 管线, 桌面平台自动回退桌面 GL)

### 约束求解器说明

- 参数向量 = 全部草图点坐标 + 圆/弧半径 + 弧角参数
- 每条约束 → 残差方程; Levenberg–Marquardt(数值中心差分雅可比 + 线搜索 + 自适应阻尼)
- 秩估计(Gram-Schmidt 列主元)给出剩余自由度; 残差非零且秩满 → 判定过约束
- 拖拽 = 追加大权重软约束(把目标点钉在鼠标处), 与既有约束一起联立求解 → 实时跟手

## 路线图(欢迎共建)

- 草图: 偏移/镜像/阵列/修剪, 约束可视化图钉
- 特征: 放样/扫掠/筋/拔模, 参考几何体
- 制图: GB/ISO 标准图幅模板, 形位公差, 剖视图
- 渲染: 环境贴图 IBL, SSAO, 出图级光线追踪(可选)
- 交换: XCAF 颜色/装配树, DWG/DXF 读入

## 许可

- 本仓库代码: MIT
- Open CASCADE Technology: LGPL-2.1(with exception)
- Dear ImGui: MIT · GLFW: zlib · SimHei 字体: 版权归其所有者, 仅作开发占位, 请自行替换可分发字体
