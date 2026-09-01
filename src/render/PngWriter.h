// PngWriter.h —— 无压缩 PNG 写出(截图用, 零依赖)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cad {

// rgba: 4 通道, stride = w*4, 自上而下
bool writePngRGBA(const std::string& path, int w, int h, const uint8_t* rgba);

} // namespace cad
