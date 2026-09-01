#!/usr/bin/env python3
# gen_pdffont.py —— 生成 src/drawing/PdfFontData.h (Noto Sans SC 子集)
# 依赖: pip3 install --user --break-system-packages fonttools
# 用法: python3 scripts/gen_pdffont.py [字体路径]
#   字体路径默认下载 Google Fonts 的 NotoSansSC[wght].ttf (OFL 许可)
# 覆盖字符: 仓库源码全部 CJK + GB2312 一级常用字 + ASCII 可打印
import io
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "drawing", "PdfFontData.h")


def collect_chars():
    chars = set()
    for base in ("src", "tests"):
        for root, _, files in os.walk(os.path.join(ROOT, base)):
            for f in files:
                if f.endswith((".h", ".cpp")):
                    s = open(os.path.join(root, f), encoding="utf-8", errors="ignore").read()
                    chars |= {c for c in s if ord(c) > 0x7F}
    for hi in range(0xB0, 0xD8):          # GB2312 一级常用字
        for lo in range(0xA1, 0xFF):
            try:
                chars |= {bytes([hi, lo]).decode("gbk")}
            except Exception:
                pass
    chars |= {chr(c) for c in range(0x20, 0x7F)}
    chars -= set("\n\r\t")
    return sorted(chars)


def get_font(path):
    if path and os.path.exists(path):
        return path
    tmp = os.path.join(tempfile.gettempdir(), "NotoSansSC-var.ttf")
    if not os.path.exists(tmp):
        url = ("https://github.com/google/fonts/raw/main/"
               "ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf")
        subprocess.check_call(["git", "clone", "--depth", "1", "--filter=blob:none",
                               "--sparse", "https://github.com/google/fonts.git",
                               tmp + ".repo"])
        subprocess.check_call(["git", "-C", tmp + ".repo", "sparse-checkout",
                               "set", "ofl/notosanssc"])
        os.replace(os.path.join(tmp + ".repo", "ofl", "notosanssc", "NotoSansSC[wght].ttf"), tmp)
    return tmp


def main():
    from fontTools.ttLib import TTFont
    from fontTools.varLib import instancer
    from fontTools import subset

    font_path = get_font(sys.argv[1] if len(sys.argv) > 1 else None)
    chars = collect_chars()
    print("字符集:", len(chars))

    f = TTFont(font_path)
    if "fvar" in f:
        instancer.instantiateVariableFont(f, {"wght": 400}, inplace=True)
    opts = subset.Options()
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    opts.drop_tables += ["GSUB", "GPOS", "GDEF"]
    ss = subset.Subsetter(opts)
    ss.populate(unicodes=[ord(c) for c in chars])
    ss.subset(f)
    buf = io.BytesIO()
    f.save(buf)
    data = buf.getvalue()
    print("子集字形:", f["maxp"].numGlyphs, "字节:", len(data))

    upem = f["head"].unitsPerEm
    cmap = f.getBestCmap()
    hmtx = f["hmtx"]
    pairs = sorted((ucs, f.getGlyphID(g), int(round(hmtx[g][0] * 1000.0 / upem)))
                   for ucs, g in cmap.items())

    out = []
    out.append("// PdfFontData.h —— 自动生成 (scripts/gen_pdffont.py), 勿手改")
    out.append("// Noto Sans SC 子集 (SIL OFL 1.1); 覆盖源码 CJK + GB2312 一级字 + ASCII")
    out.append("#pragma once")
    out.append("#include <cstdint>")
    out.append("#include <cstddef>")
    out.append("")
    out.append("namespace cad { namespace pdffont {")
    out.append("")
    out.append("struct CmapEntry { uint32_t ucs; uint16_t gid; };")
    out.append("inline const CmapEntry kCmap[] = {")
    out += ["    {0x%04X, %d}," % (u, g) for u, g, _ in pairs]
    out.append("};")
    out.append("inline const size_t kCmapN = sizeof(kCmap) / sizeof(kCmap[0]);")
    out.append("")
    maxgid = max(g for _, g, _ in pairs)
    wtab = [0] * (maxgid + 1)
    for _, g, w in pairs:
        wtab[g] = w
    out.append("inline const uint16_t kWidth[] = {")
    line = "   "
    for i, w in enumerate(wtab):
        line += " %d," % w
        if (i + 1) % 20 == 0:
            out.append(line)
            line = "   "
    if line.strip():
        out.append(line)
    out.append("};")
    out.append("inline const size_t kWidthN = sizeof(kWidth) / sizeof(kWidth[0]);")
    out.append("")
    out.append("inline const unsigned char kTtf[] = {")
    for i in range(0, len(data), 20):
        out.append("    " + "".join("0x%02x," % b for b in data[i:i + 20]))
    out.append("};")
    out.append("inline const size_t kTtfN = sizeof(kTtf);")
    out.append("")
    out.append("inline uint16_t gidOf(uint32_t ucs) {  // 二分; 未命中 -> 0 (.notdef)")
    out.append("    size_t lo = 0, hi = kCmapN;")
    out.append("    while (lo < hi) {")
    out.append("        size_t mid = (lo + hi) / 2;")
    out.append("        if (kCmap[mid].ucs < ucs) lo = mid + 1; else hi = mid;")
    out.append("    }")
    out.append("    return (lo < kCmapN && kCmap[lo].ucs == ucs) ? kCmap[lo].gid : 0;")
    out.append("}")
    out.append("inline uint16_t widthOf(uint16_t gid) { return gid < kWidthN ? kWidth[gid] : 0; }")
    out.append("")
    out.append("}} // namespace cad::pdffont")
    open(OUT, "w").write("\n".join(out) + "\n")
    print("写出:", OUT, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    main()
