#!/usr/bin/env bash
# Fetch third-party dependencies (all from GitHub; no package manager required).
# Everything lands in third_party/ which is git-ignored.
set -e
cd "$(dirname "$0")/.."
mkdir -p third_party
cd third_party

clone() {
  local url=$1 tag=$2 dir=$3
  if [ -d "$dir" ]; then
    echo "[deps] $dir already present"
  else
    echo "[deps] cloning $url ($tag) -> $dir"
    git clone --depth 1 --branch "$tag" "$url" "$dir"
  fi
}

clone https://github.com/ocornut/imgui.git    v1.90.4 imgui
clone https://github.com/glfw/glfw.git        3.4      glfw

# CJK-capable UI font (SimHei) for the Chinese UI
if [ ! -f fonts/SimHei.ttf ]; then
  echo "[deps] fetching CJK font"
  mkdir -p fonts
  git clone --depth 1 https://github.com/StellarCN/scp_zh.git /tmp/scp_zh || true
  if [ -f /tmp/scp_zh/fonts/SimHei.ttf ]; then
    cp /tmp/scp_zh/fonts/SimHei.ttf fonts/SimHei.ttf
  fi
  rm -rf /tmp/scp_zh
fi

echo "[deps] done"
