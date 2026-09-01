// DrawingExport.cpp —— SVG / DXF(R12) / PDF 导出(矢量)
#include "Drawing.h"
#include <cstdio>
#include <cstring>

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
        pairS(1, t.text.c_str());
    }
    ent("ENDSEC");
    ent("EOF");
    return s;
}

// ---------------- PDF ----------------
namespace {

struct PdfBuf {
    std::string content;   // 页内容流
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
    void text(double x, double y, double H, double size, const std::string& t) {
        char b[256];
        // 转义括号
        std::string e;
        for (char c : t) {
            if (c == '(' || c == ')') e += '\\';
            e += c;
        }
        snprintf(b, sizeof(b), "BT /F1 %.1f Tf %.2f %.2f Td (%s) Tj ET\n", size, x, H - y, e.c_str());
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
    for (auto& t : d.texts) b.text(t.pos.x * K, t.pos.y * K + t.height * K, H, t.height * K, t.text);

    // ---- 组装 PDF ----
    std::string objs[6];
    objs[1] = "<< /Type /Catalog /Pages 2 0 R >>\n";
    objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n";
    objs[3] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(W) + " " + std::to_string(H) +
              "] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>\n";
    objs[4] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n";
    std::string stream = b.content;
    objs[5] = "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream\n";

    std::string out = "%PDF-1.4\n";
    size_t offsets[6] = {0};
    for (int i = 1; i <= 5; ++i) {
        offsets[i] = out.size();
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "%d 0 obj\n", i);
        out += hdr;
        out += objs[i];
        out += "endobj\n";
    }
    size_t xref = out.size();
    char xb[64];
    out += "xref\n0 6\n0000000000 65535 f \n";
    for (int i = 1; i <= 5; ++i) {
        snprintf(xb, sizeof(xb), "%010zu 00000 n \n", offsets[i]);
        out += xb;
    }
    snprintf(xb, sizeof(xb), "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n", xref);
    out += xb;
    return out;
}

} // namespace cad
