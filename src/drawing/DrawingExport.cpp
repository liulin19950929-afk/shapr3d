// DrawingExport.cpp —— SVG / DXF(R12) / PDF 导出(矢量)
#include "Drawing.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "PdfFontData.h"

namespace cad {

bool writeFileText(const std::string& path, const std::string& content) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    fwrite(content.data(), 1, content.size(), fp);
    fclose(fp);
    return true;
}

// ---------------- SVG ----------------
static void svgPoly(std::string& s, const std::vector<Vec2>& pts, double H, bool hidden) {
    s += "<path d=\"";
    char buf[128];
    for (size_t i = 0; i < pts.size(); ++i) {
        snprintf(buf, sizeof(buf), "%s%.2f %.2f ", i ? "L " : "M ", pts[i].x, H - pts[i].y);
        s += buf;
    }
    s += R"(" fill="none" stroke=")" + std::string(hidden ? "#8a8f98" : "#1c2733");
    s += "\" stroke-width=\"0.35\"";
    if (hidden) s += " stroke-dasharray=\"2,1.2\"";
    s += "/>\n";
}

std::string exportDrawingSVG(const Drawing& d) {
    std::string s;
    s.reserve(64 * 1024);
    char buf[256];
    snprintf(buf, sizeof(buf),
             R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="%.0fmm" height="%.0fmm" viewBox="0 0 %.0f %.0f">
<rect x="0" y="0" width="%.0f" height="%.0f" fill="#ffffff"/>
)", d.sheetW, d.sheetH, d.sheetW, d.sheetH, d.sheetW, d.sheetH);
    s += buf;
    // 图框
    snprintf(buf, sizeof(buf),
             "<rect x=\"10\" y=\"10\" width=\"%.0f\" height=\"%.0f\" fill=\"none\" stroke=\"#1c2733\" stroke-width=\"0.7\"/>\n",
             d.sheetW - 20, d.sheetH - 20);
    s += buf;
    // 标题栏
    double tbX = d.sheetW - 130, tbY = 10, tbW = 120, tbH = 40;
    snprintf(buf, sizeof(buf),
             "<rect x=\"%.0f\" y=\"%.0f\" width=\"%.0f\" height=\"%.0f\" fill=\"none\" stroke=\"#1c2733\" stroke-width=\"0.5\"/>\n",
             tbX, tbY, tbW, tbH);
    s += buf;
    snprintf(buf, sizeof(buf), "<line x1=\"%.0f\" y1=\"%.0f\" x2=\"%.0f\" y2=\"%.0f\" stroke=\"#1c2733\" stroke-width=\"0.4\"/>\n",
             tbX, tbY + 14, tbX + tbW, tbY + 14);
    s += buf;

    for (auto& v : d.views)
        for (auto& p : v.polies) svgPoly(s, p.pts, d.sheetH, p.hidden);
    for (auto& p : d.drafts) svgPoly(s, p.pts, d.sheetH, p.hidden);
    for (auto& c : d.circles) {
        snprintf(buf, sizeof(buf),
                 "<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" fill=\"none\" stroke=\"#1c2733\" stroke-width=\"0.35\"/>\n",
                 c.center.x, d.sheetH - c.center.y, c.r);
        s += buf;
    }
    for (auto& t : d.texts) {
        std::string anchor = t.align == 1 ? "middle" : t.align == 2 ? "end" : "start";
        snprintf(buf, sizeof(buf),
                 "<text x=\"%.2f\" y=\"%.2f\" font-size=\"%.1f\" font-family=\"sans-serif\" fill=\"#1c2733\" text-anchor=\"%s\">",
                 t.pos.x, d.sheetH - t.pos.y, t.height, anchor.c_str());
        s += buf;
        s += t.text;
        s += "</text>\n";
    }
    s += "</svg>\n";
    return s;
}

// ---------------- DXF R12 ----------------
std::string exportDrawingDXF(const Drawing& d) {
    std::string s;
    char buf[256];
    auto ent = [&](const char* e) { s += "0\n"; s += e; s += "\n"; };
    auto pair = [&](int code, double v) {
        snprintf(buf, sizeof(buf), "%d\n%.6f\n", code, v);
        s += buf;
    };
    auto pairS = [&](int code, const char* v) {
        snprintf(buf, sizeof(buf), "%d\n%s\n", code, v);
        s += buf;
    };

    ent("SECTION");
    pairS(2, "ENTITIES");

    auto line = [&](double x1, double y1, double x2, double y2, const char* layer) {
        ent("LINE");
        pairS(8, layer);
        pair(10, x1); pair(20, y1); pair(30, 0);
        pair(11, x2); pair(21, y2); pair(31, 0);
    };
    auto polyline = [&](const std::vector<Vec2>& pts, bool hidden) {
        for (size_t i = 1; i < pts.size(); ++i)
            line(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y, hidden ? "HIDDEN" : "OUTLINE");
    };

    // 图框
    line(10, 10, d.sheetW - 10, 10, "BORDER");
    line(d.sheetW - 10, 10, d.sheetW - 10, d.sheetH - 10, "BORDER");
    line(d.sheetW - 10, d.sheetH - 10, 10, d.sheetH - 10, "BORDER");
    line(10, d.sheetH - 10, 10, 10, "BORDER");

    for (auto& v : d.views)
        for (auto& p : v.polies) polyline(p.pts, p.hidden);
    for (auto& p : d.drafts) polyline(p.pts, p.hidden);
    for (auto& c : d.circles) {
        ent("CIRCLE");
        pairS(8, "OUTLINE");
        pair(10, c.center.x);
        pair(20, c.center.y);
        pair(30, 0);
        pair(40, c.r);
    }
    for (auto& t : d.texts) {
        ent("TEXT");
        pairS(8, "TEXT");
        pair(10, t.pos.x);
        pair(20, t.pos.y);
        pair(30, 0);
        pair(40, t.height);
        // R12 文本为 ANSI 字节流: 非 ASCII 以 \U+XXXX 转义(AutoCAD 兼容写法)
        {
            std::string safe;
            for (size_t i = 0; i < t.text.size();) {
                unsigned char c = (unsigned char)t.text[i];
                uint32_t u = c;
                size_t n = 1;
                if ((c & 0xE0) == 0xC0 && i + 1 < t.text.size()) { u = c & 0x1F; n = 2; }
                else if ((c & 0xF0) == 0xE0 && i + 2 < t.text.size()) { u = c & 0x0F; n = 3; }
                else if ((c & 0xF8) == 0xF0 && i + 3 < t.text.size()) { u = c & 0x07; n = 4; }
                bool ok = n > 1;
                for (size_t k = 1; ok && k < n; ++k)
                    if (((unsigned char)t.text[i + k] & 0xC0) != 0x80) ok = false;
                if (ok) {
                    for (size_t k = 1; k < n; ++k) u = (u << 6) | ((unsigned char)t.text[i + k] & 0x3F);
                    char ub[16];
                    snprintf(ub, sizeof(ub), "\\U+%04X", u);
                    safe += ub;
                    i += n;
                } else {
                    safe += t.text[i++];
                }
            }
            pairS(1, safe.c_str());
        }
    }
    ent("ENDSEC");
    ent("EOF");
    return s;
}

// ---------------- PDF ----------------
namespace {

struct PdfBuf {
    std::string content;            // 页内容流
    std::vector<uint16_t> usedGids; // F2 用到的字形(生成 /W 数组)
    // y 翻转
    void moveTo(double x, double y, double H) {
        char b[96];
        snprintf(b, sizeof(b), "%.2f %.2f m\n", x, H - y);
        content += b;
    }
    void lineTo(double x, double y, double H) {
        char b[96];
        snprintf(b, sizeof(b), "%.2f %.2f l\n", x, H - y);
        content += b;
    }
    // UTF-8 解码; 回调 (ucs4)
    template <typename F>
    static void utf8Each(const std::string& t, F&& cb) {
        for (size_t i = 0; i < t.size();) {
            unsigned char c = (unsigned char)t[i];
            uint32_t u = c;
            size_t n = 1;
            if ((c & 0xE0) == 0xC0 && i + 1 < t.size()) { u = c & 0x1F; n = 2; }
            else if ((c & 0xF0) == 0xE0 && i + 2 < t.size()) { u = c & 0x0F; n = 3; }
            else if ((c & 0xF8) == 0xF0 && i + 3 < t.size()) { u = c & 0x07; n = 4; }
            bool ok = true;
            for (size_t k = 1; k < n; ++k) {
                unsigned char cc = (unsigned char)t[i + k];
                if ((cc & 0xC0) != 0x80) { ok = false; break; }
                u = (u << 6) | (cc & 0x3F);
            }
            if (ok && n > 1) cb(u);
            else if (ok) cb(u); // ASCII
            i += ok ? n : 1;
        }
    }
    double textWidth(const std::string& t, double size) {
        double w = 0;
        utf8Each(t, [&](uint32_t u) { w += pdffont::widthOf(pdffont::gidOf(u)) * size / 1000.0; });
        return w;
    }
    void text(double x, double y, double H, double size, const std::string& t, int align = 0) {
        // 测宽 -> 按 align 平移 (0左 1中 2右)
        double w = textWidth(t, size);
        if (align == 1) x -= w * 0.5;
        else if (align == 2) x -= w;
        // 转字形 id 串
        std::string hex;
        utf8Each(t, [&](uint32_t u) {
            uint16_t g = pdffont::gidOf(u);
            usedGids.push_back(g);
            char hb[8];
            snprintf(hb, sizeof(hb), "%04X", g);
            hex += hb;
        });
        char b[64];
        snprintf(b, sizeof(b), "BT /F2 %.1f Tf %.2f %.2f Td <%s> Tj ET\n", size, x, H - y, hex.c_str());
        content += b;
    }
};

} // namespace

std::string exportDrawingPDF(const Drawing& d) {
    // A3 横放 (pt: 1190.55 x 841.89) 按 mm -> pt 缩放 2.8346
    const double K = 2.8346457;
    double W = d.sheetW * K, H = d.sheetH * K;

    PdfBuf b;
    // 白底
    b.content += "1 1 1 rg 0 0 " + std::to_string(W) + " " + std::to_string(H) + " re f\n";
    b.content += "0.11 0.15 0.2 RG 0.9 w\n";
    // 图框
    b.moveTo(10 * K, 10 * K, H); b.lineTo((d.sheetW - 10) * K, 10 * K, H);
    b.lineTo((d.sheetW - 10) * K, (d.sheetH - 10) * K, H);
    b.lineTo(10 * K, (d.sheetH - 10) * K, H); b.lineTo(10 * K, 10 * K, H);
    b.content += "S\n";

    auto drawPolies = [&](const std::vector<DrawPoly>& ps) {
        for (auto& p : ps) {
            b.content += p.hidden ? "[3 2] 0 d 0.6 w\n" : "[] 0 d 0.9 w\n";
            b.moveTo(p.pts[0].x * K, p.pts[0].y * K, H);
            for (size_t i = 1; i < p.pts.size(); ++i)
                b.lineTo(p.pts[i].x * K, p.pts[i].y * K, H);
            b.content += "S\n";
        }
    };
    for (auto& v : d.views) drawPolies(v.polies);
    drawPolies(d.drafts);
    for (auto& c : d.circles) {
        // 圆 -> 4 段贝塞尔
        double r = c.r * K, cx = c.center.x * K, cy = H - c.center.y * K;
        double k = 0.5523 * r;
        char t[256];
        b.content += "[] 0 d 0.9 w\n";
        snprintf(t, sizeof(t),
                 "%.2f %.2f m %.2f %.2f %.2f %.2f %.2f %.2f c "
                 "%.2f %.2f %.2f %.2f %.2f %.2f c "
                 "%.2f %.2f %.2f %.2f %.2f %.2f c "
                 "%.2f %.2f %.2f %.2f %.2f %.2f c S\n",
                 cx + r, cy,
                 cx + r, cy + k, cx + k, cy + r, cx, cy + r,
                 cx - k, cy + r, cx - r, cy + k, cx - r, cy,
                 cx - r, cy - k, cx - k, cy - r, cx, cy - r,
                 cx + k, cy - r, cx + r, cy - k, cx + r, cy);
        b.content += t;
    }
    b.content += "0.11 0.15 0.2 rg\n";
    for (auto& t : d.texts) b.text(t.pos.x * K, t.pos.y * K + t.height * K, H, t.height * K, t.text, t.align);

    // ---- 组装 PDF ----
    // 对象: 1 Catalog, 2 Pages, 3 Page, 4 F1(Helvetica), 5 Contents,
    //       6 F2(Type0), 7 CIDFontType2, 8 FontDescriptor, 9 FontFile2
    std::string objs[10];
    objs[1] = "<< /Type /Catalog /Pages 2 0 R >>\n";
    objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n";
    objs[3] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(W) + " " + std::to_string(H) +
              "] /Resources << /Font << /F1 4 0 R /F2 6 0 R >> >> /Contents 5 0 R >>\n";
    objs[4] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";
    std::string stream = b.content;
    objs[5] = "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream\n";

    // F2: Type0 + CIDFontType2(Identity-H), 覆盖实际用到的字形宽度
    std::sort(b.usedGids.begin(), b.usedGids.end());
    b.usedGids.erase(std::unique(b.usedGids.begin(), b.usedGids.end()), b.usedGids.end());
    std::string wArr = "[";
    for (uint16_t g : b.usedGids)
        wArr += std::to_string(g) + " [" + std::to_string(pdffont::widthOf(g)) + "] ";
    wArr += "]";
    objs[6] = "<< /Type /Font /Subtype /Type0 /BaseFont /NotoSansSC /Encoding /Identity-H "
              "/DescendantFonts [7 0 R] >>\n";
    objs[7] = "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /NotoSansSC "
              "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
              "/CIDToGIDMap /Identity /W " + wArr + " /FontDescriptor 8 0 R >>\n";
    objs[8] = "<< /Type /FontDescriptor /FontName /NotoSansSC /Flags 4 /FontBBox [-1000 -300 2100 1200] "
              "/ItalicAngle 0 /Ascent 1160 /Descent -288 /CapHeight 710 /StemV 80 /FontFile2 9 0 R >>\n";
    objs[9] = "<< /Length " + std::to_string(pdffont::kTtfN) + " /Length1 " + std::to_string(pdffont::kTtfN) +
              " >>\nstream\n";
    objs[9] += std::string(reinterpret_cast<const char*>(pdffont::kTtf), pdffont::kTtfN);
    objs[9] += "\nendstream\n";

    std::string out = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    size_t offsets[10] = {0};
    for (int i = 1; i <= 9; ++i) {
        offsets[i] = out.size();
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "%d 0 obj\n", i);
        out += hdr;
        out += objs[i];
        out += "endobj\n";
    }
    size_t xref = out.size();
    char xb[64];
    out += "xref\n0 10\n0000000000 65535 f \n";
    for (int i = 1; i <= 9; ++i) {
        snprintf(xb, sizeof(xb), "%010zu 00000 n \n", offsets[i]);
        out += xb;
    }
    snprintf(xb, sizeof(xb), "trailer\n<< /Size 10 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n", xref);
    out += xb;
    return out;
}

} // namespace cad
