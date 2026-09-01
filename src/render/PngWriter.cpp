// PngWriter.cpp —— zlib stored 块 + CRC32/Adler32
#include "PngWriter.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace cad {

static uint32_t crcTable[256];
static bool crcInit = false;

static uint32_t crc32(const uint8_t* data, size_t len) {
    if (!crcInit) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crcTable[n] = c;
        }
        crcInit = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = crcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// zlib 流: 2 字节头 + stored deflate 块 + adler
static std::vector<uint8_t> zlibStore(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out;
    out.push_back(0x78);
    out.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t chunk = std::min<size_t>(65535, raw.size() - pos);
        bool last = pos + chunk >= raw.size();
        out.push_back(last ? 1 : 0);
        out.push_back((uint8_t)(chunk & 0xFF));
        out.push_back((uint8_t)(chunk >> 8));
        out.push_back((uint8_t)(~chunk & 0xFF));
        out.push_back((uint8_t)((~chunk >> 8) & 0xFF));
        out.insert(out.end(), raw.begin() + pos, raw.begin() + pos + chunk);
        pos += chunk;
    }
    uint32_t ad = adler32(raw.data(), raw.size());
    uint8_t t[4];
    be32(t, ad);
    out.insert(out.end(), t, t + 4);
    return out;
}

bool writePngRGBA(const std::string& path, int w, int h, const uint8_t* rgba) {
    // raw: 每行前加 filter 0
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (w * 4 + 1));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + (size_t)y * w * 4, rgba + ((size_t)y + 1) * w * 4);
    }
    std::vector<uint8_t> z = zlibStore(raw);

    uint8_t ihdr[13];
    be32(ihdr, (uint32_t)w);
    be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;  // bit depth
    ihdr[9] = 6;  // RGBA
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;
        uint8_t len[4];
        be32(len, (uint32_t)data.size());
        out.insert(out.end(), len, len + 4);
        std::vector<uint8_t> body;
        body.insert(body.end(), type, type + 4);
        body.insert(body.end(), data.begin(), data.end());
        uint32_t c = crc32(body.data(), body.size());
        uint8_t cc[4];
        be32(cc, c);
        body.insert(body.end(), cc, cc + 4);
        return body;
    };

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, fp);
    std::vector<uint8_t> ih = chunk("IHDR", std::vector<uint8_t>(ihdr, ihdr + 13));
    fwrite(ih.data(), 1, ih.size(), fp);
    std::vector<uint8_t> id = chunk("IDAT", z);
    fwrite(id.data(), 1, id.size(), fp);
    std::vector<uint8_t> ie = chunk("IEND", {});
    fwrite(ie.data(), 1, ie.size(), fp);
    fclose(fp);
    return true;
}

} // namespace cad
