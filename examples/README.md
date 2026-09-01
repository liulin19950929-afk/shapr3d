# 示例输出 (demo_headless 生成)

由 `./build/bin/demo_headless examples/` 端到端生成:

- `flange_drawing.svg` / `flange_drawing.dxf` — 法兰盘工程图(四视图 HLR, 主/俯/左/轴测, A3 四宫格+标题栏)
- `flange.step` — 精确 B-Rep (AP203/214), STEP 往返体积偏差 0.000000%
- `flange.stl` — 二进制网格导出

重新生成: `LD_LIBRARY_PATH=<occt>/lib ./build/bin/demo_headless examples/`
