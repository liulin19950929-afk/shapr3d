#!/usr/bin/env bash
# 构建脚本 —— 从零构建(自动获取依赖)
# 用法:
#   scripts/build.sh                 # 构建应用 + 测试
#   OCCT_DIR=/path/to/occt/build scripts/build.sh   # 指定 OCCT 构建目录
set -e
cd "$(dirname "$0")/.."

if [ ! -d third_party/imgui ] || [ ! -d third_party/glfw ]; then
    echo "[build] 获取第三方依赖..."
    bash scripts/fetch-deps.sh
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "[build] 缺少 cmake, 尝试 pip install cmake..."
    pip3 install --user --break-system-packages cmake ninja 2>/dev/null || pip3 install --user cmake ninja
    export PATH=$HOME/.local/bin:$PATH
fi

mkdir -p build
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    ${OCCT_DIR:+-DOpenCASCADE_DIR=$OCCT_DIR} \
    "$@"
cmake --build build -j "$(nproc)"
echo "[build] 完成: build/bin/shapr3d"
echo "  运行: LD_LIBRARY_PATH=${OCCT_DIR:-/usr/lib}/lib build/bin/shapr3d"
