// Application.cpp —— 交互实现(拾取/工具/命令)
#include "Application.h"
#include "Ui.h"
#include "ImGuiBackend.h"
#include "../core/ThreadPool.h"
#include "../analysis/Measure.h"
#include "../io/Exchange.h"

#include <unordered_set>
#include <unordered_map>

namespace cad {

static GLFWwindow* g_win = nullptr;
static Application* g_app = nullptr;

static void scrollCb(GLFWwindow*, double dx, double dy) {
    ui::imGuiScroll((float)dx, (float)dy);
    if (ImGui::GetIO().WantCaptureMouse) return;
    Application* a = g_app;
    if (!a) return;
    if (a->mode == Mode::Drawing) {
        a->drawZoom *= (float)std::pow(1.1, dy);
        a->drawZoom = std::max(0.05f, std::min(40.f, a->drawZoom));
    } else {
        a->cam.dist *= (float)std::pow(0.9, dy);
        a->cam.dist = std::max(2.f, std::min(50000.f, a->cam.dist));
    }
}

static void charCb(GLFWwindow*, unsigned int c) {
    ui::imGuiChar(c);
}

static void errorCb(int, const char* msg) {
    LOGE("GLFW: %s", msg);
}

void Application::showToast(const std::string& msg, bool error) {
    toast = msg;
    toastUntil = Stopwatch::nowMs() + (error ? 5000 : 2500);
    if (error) LOGW("%s", msg.c_str());
}

void Application::pushUndo() {
    undoStack.push_back(doc.serialize());
    if (undoStack.size() > undoLimit) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void Application::doUndo() {
    if (undoStack.empty()) return;
    redoStack.push_back(doc.serialize());
    doc.deserialize(undoStack.back());
    undoStack.pop_back();
    recompute();
    clearSelection();
    showToast("已撤销");
}

void Application::doRedo() {
    if (redoStack.empty()) return;
    undoStack.push_back(doc.serialize());
    doc.deserialize(redoStack.back());
    redoStack.pop_back();
    recompute();
    clearSelection();
    showToast("已重做");
}

void Application::recompute() {
    doc.recomputeAll();
    renderer.invalidateShadow();
}

void Application::clearSelection() {
    selFaces.clear();
    selEdges.clear();
    selVertices.clear();
    selBodies.clear();
    selectedFeatureId = kInvalidId;
}

// ---------------- 特征 ----------------
Id Application::commitFeature(Feature f, Id bodyId) {
    pushUndo();
    Body* b = doc.body(bodyId);
    if (!b && !doc.bodies.empty()) b = &doc.bodies.back();
    if (!b) {
        b = &doc.addBody();
        b->name = "实体" + std::to_string(doc.bodies.size());
    }
    f.id = doc.newId();
    f.name = f.displayLabel();
    b->features.push_back(std::move(f));
    recompute();
    Body* bb = doc.body(b->id);
    if (bb && !bb->error.empty()) showToast(bb->error, true);
    selectedFeatureId = bb->features.back().id;
    return selectedFeatureId;
}

void Application::deleteFeature(Id fid) {
    pushUndo();
    for (auto& b : doc.bodies) {
        for (auto it = b.features.begin(); it != b.features.end(); ++it) {
            if (it->id == fid) {
                b.features.erase(it);
                recompute();
                showToast("已删除特征");
                return;
            }
        }
    }
}

void Application::booleanOp(int op) {
    // 去重实体
    std::vector<Id> ids;
    std::unordered_set<Id> seen;
    for (auto& s : selFaces) {
        if (seen.insert(s.bodyId).second) ids.push_back(s.bodyId);
    }
    if (ids.size() < 2) {
        // 尝试实体选择
        for (auto& s : selBodies)
            if (seen.insert(s.bodyId).second) ids.push_back(s.bodyId);
    }
    if (ids.size() < 2) {
        showToast("布尔运算需要选择两个以上实体的面(Shift+点击)", true);
        return;
    }
    // 以第一个为主体, 依次与后续求布尔
    Id mainBody = ids[0];
    for (size_t i = 1; i < ids.size(); ++i) {
        Feature f;
        f.type = FeatureType::Boolean;
        f.opMode = op;
        f.targetBody = ids[i];
        commitFeature(std::move(f), mainBody);
        doc.body(ids[i])->hiddenByOp = (op == 1); // 差集隐藏工具体
        if (op == 1) doc.body(ids[i])->visible = false;
    }
    showToast(op == 0 ? "布尔并完成" : op == 1 ? "布尔差完成" : "布尔交完成");
}

// ---------------- 草图 ----------------
void Application::enterSketch(Id sketchId) {
    SketchDef* sk = doc.sketch(sketchId);
    if (!sk) return;
    activeSketchId = sketchId;
    mode = Mode::Sketch;
    tool = Tool::Select;
    pendingPts.clear();
    solveSketch();
    // 视角对准草图
    gp_Dir n = sk->plane.Axis().Direction();
    cam.yaw = std::atan2(n.Y(), n.X());
    cam.pitch = -0.35f;
    cam.target = {(float)sk->plane.Location().X(), (float)sk->plane.Location().Y(),
                  (float)sk->plane.Location().Z()};
}

void Application::startNewSketch(const gp_Pln& pln) {
    pushUndo();
    SketchDef& s = doc.addSketch(pln, "草图" + std::to_string(doc.sketches.size() + 1));
    enterSketch(s.id);
}

void Application::exitSketch() {
    mode = Mode::Object;
    tool = Tool::Select;
    pendingPts.clear();
    rubberLines.clear();
    snapMarkers.clear();
    dragging = false;
    dragPointId = kInvalidId;
}

Id Application::addSketchLine(Vec2 a, Vec2 b, bool construction, Id* outP1, Id* outP2) {
    SketchDef* sk = doc.sketch(activeSketchId);
    if (!sk) return kInvalidId;
    Id p1 = doc.newId(), p2 = doc.newId(), lid = doc.newId();
    sk->addPoint(a.x, a.y, p1);
    sk->addPoint(b.x, b.y, p2);
    sk->addLine(p1, p2, lid, construction);
    if (outP1) *outP1 = p1;
    if (outP2) *outP2 = p2;
    return lid;
}

Id Application::addSketchCircle(Vec2 c, double r) {
    SketchDef* sk = doc.sketch(activeSketchId);
    if (!sk) return kInvalidId;
    Id pid = doc.newId(), cid = doc.newId();
    sk->addPoint(c.x, c.y, pid);
    sk->addCircle(pid, std::max(1e-3, r), cid);
    return cid;
}

void Application::solveSketch() {
    SketchDef* sk = doc.sketch(activeSketchId);
    if (!sk) return;
    ConstraintSolver s;
    lastSolve = s.solve(*sk);
}

Id Application::hitSketchPoint(Vec2 screen, double tolPx) {
    SketchDef* sk = doc.sketch(activeSketchId);
    if (!sk) return kInvalidId;
    Id best = kInvalidId;
    double bestD = tolPx;
    for (auto& p : sk->points) {
        Vec3 w = {(float)sk->to3D(p.x, p.y).X(), (float)sk->to3D(p.x, p.y).Y(), (float)sk->to3D(p.x, p.y).Z()};
        Vec3 sp = projectPoint(w);
        double d = std::hypot(sp.x - screen.x, sp.y - screen.y);
        if (d < bestD) {
            bestD = d;
            best = p.id;
        }
    }
    return best;
}

void Application::addConstraintUI(CstType t, double value) {
    SketchDef* sk = doc.sketch(activeSketchId);
    if (!sk) return;
    // 从 pendingPts? 约束基于 UI 中勾选的实体 —— 简化: 使用最近两次点击的实体引用栈
    // 这里由 UI 层构造 refs 传入 —— 保留接口
    (void)t;
    (void)value;
}

// ---------------- 工程图 ----------------
void Application::generateDrawing() {
    const TopoDS_Shape* shape = nullptr;
    for (auto& s : selBodies) {
        Body* b = doc.body(s.bodyId);
        if (b && !b->result.IsNull()) {
            shape = &b->result;
            break;
        }
    }
    if (!shape)
        for (auto& b : doc.bodies)
            if (!b.result.IsNull()) {
                shape = &b.result;
                break;
            }
    if (!shape) {
        showToast("没有可生成工程图的实体", true);
        return;
    }
    Stopwatch sw;
    sw.start();
    std::string err = drawing.generateFromShape(*shape);
    if (!err.empty()) {
        showToast(err, true);
        return;
    }
    drawing.partName = doc.name;
    drawingValid = true;
    LOGI("工程图生成 %.1f ms", sw.stop());
}

void Application::exportDrawing(const std::string& path, const std::string& kind) {
    if (!drawingValid) {
        showToast("请先生成工程图", true);
        return;
    }
    std::string content;
    if (kind == "svg") content = exportDrawingSVG(drawing);
    else if (kind == "dxf") content = exportDrawingDXF(drawing);
    else content = exportDrawingPDF(drawing);
    if (writeFileText(path, content)) showToast("已导出: " + path);
    else showToast("导出失败: " + path, true);
}

void Application::exportModel(const std::string& path) {
    TopoDS_Shape all;
    BRep_Builder bb;
    TopoDS_Compound comp;
    bool multi = false;
    for (auto& b : doc.bodies) {
        if (!b.visible || b.result.IsNull()) continue;
        if (!multi) {
            all = b.result;
            multi = true;
            // 若还有后续, 升级为 compound
        } else {
            if (comp.IsNull()) {
                bb.MakeCompound(comp);
                bb.Add(comp, all);
            }
            bb.Add(comp, b.result);
            all = comp;
        }
    }
    if (all.IsNull()) {
        showToast("没有可导出的实体", true);
        return;
    }
    io::Format f = io::detectFormat(path);
    std::string err;
    bool ok = (f == io::Format::STL || f == io::Format::OBJ) ? io::exportMesh(all, path, f, doc.meshDeflection, err)
                                                             : io::exportBRep(all, path, f == io::Format::Auto ? io::Format::STEP : f, err);
    showToast(ok ? "已导出: " + path : err, !ok);
}

void Application::importModel(const std::string& path) {
    TopoDS_Shape shape;
    std::string err;
    if (!io::importShape(path, shape, err)) {
        showToast(err.empty() ? "导入失败" : err, true);
        return;
    }
    pushUndo();
    Body& b = doc.addBody();
    b.name = "导入 " + std::to_string(doc.bodies.size());
    Feature& f = b.features.emplace_back();
    f.id = doc.newId();
    f.type = FeatureType::Imported;
    f.name = "导入模型";
    f.result = shape;
    recompute();
    clearSelection();
    SelRef ref;
    ref.bodyId = b.id;
    ref.kind = 3;
    ref.shape = shape;
    selBodies.push_back(ref);
    frameAll();
    showToast("已导入: " + path);
}

void Application::screenshot(const std::string& path) {
    if (renderer.screenshot(path)) showToast("截图已保存: " + path);
}

void Application::frameAll() {
    Bnd_Box bb;
    if (!doc.sceneBounds(bb)) {
        cam = Camera();
        return;
    }
    double x0, y0, z0, x1, y1, z1;
    bb.Get(x0, y0, z0, x1, y1, z1);
    // 全部草图也纳入
    for (auto& s : doc.sketches) {
        if (!s.visible) continue;
        for (auto& p : s.points) {
            gp_Pnt w = s.to3D(p.x, p.y);
            bb.Add(w);
        }
    }
    bb.Get(x0, y0, z0, x1, y1, z1);
    cam.frame({(float)x0, (float)y0, (float)z0}, {(float)x1, (float)y1, (float)z1});
    cam.pitch = -0.42f;
    cam.yaw = -1.05f;
}

void Application::alignView(int v) {
    static const float pi = (float)M_PI;
    switch (v) {
        case 0: cam.yaw = pi / 2; cam.pitch = 0; break;         // 前(+Y 注视)
        case 1: cam.yaw = -pi / 2; cam.pitch = 0; break;        // 后
        case 2: cam.yaw = pi; cam.pitch = 0; break;             // 左(+X)
        case 3: cam.yaw = 0; cam.pitch = 0; break;              // 右
        case 4: cam.pitch = pi / 2 - 0.01f; cam.yaw = pi / 2; break; // 上
        case 5: cam.pitch = -pi / 2 + 0.01f; cam.yaw = pi / 2; break;// 下
        default: cam.yaw = -pi / 3; cam.pitch = -pi / 6; break; // 轴测
    }
}

// ---------------- 拾取 ----------------
PickHit Application::pickBody(float sx, float sy) {
    Vec3 ro, rd;
    renderer.rayAt(sx, sy, cam, ro, rd, viewW(), viewH());
    PickHit best;
    best.dist = 1e30;
    for (auto& b : doc.bodies) {
        if (!b.visible) continue;
        const MeshData* md = doc.mesh(b.id);
        if (!md || md->verts.empty()) continue;
        int triCount = (int)(md->verts.size() / 9);
        for (int t = 0; t < triCount; ++t) {
            const float* A = &md->verts[t * 9];
            const float* B = A + 3;
            const float* C = B + 3;
            float tt;
            if (rayTriangle(ro, rd, {A[0], A[1], A[2]}, {B[0], B[1], B[2]}, {C[0], C[1], C[2]}, tt)) {
                if (tt < best.dist) {
                    best.hit = true;
                    best.dist = tt;
                    best.bodyId = b.id;
                    best.triIdx = t;
                    best.point = ro + rd * tt;
                }
            }
        }
    }
    if (best.hit) {
        const MeshData* md = doc.mesh(best.bodyId);
        // 三角 -> 面
        int acc = 0;
        for (int f = 0; f < (int)md->faceCount.size(); ++f) {
            acc += (int)md->faceCount[f];
            if (best.triIdx < acc) {
                best.faceIdx = f;
                break;
            }
        }
    }
    return best;
}

static float distPtSeg(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float l2 = dx * dx + dy * dy;
    if (l2 < 1e-9f) return std::hypot(px - ax, py - ay);
    float t = ((px - ax) * dx + (py - ay) * dy) / l2;
    t = std::max(0.f, std::min(1.f, t));
    return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

bool Application::pickEdge(float sx, float sy, SelRef& out) {
    double bestD = 8.0;
    bool found = false;
    for (auto& b : doc.bodies) {
        if (!b.visible) continue;
        const MeshData* md = doc.mesh(b.id);
        if (!md) continue;
        // 每条棱折线 -> 屏幕
        for (auto& line : md->edgeLines) {
            std::vector<Vec2> scr;
            Vec3 last3;
            for (size_t i = 0; i < line.size(); i += 3) {
                Vec3 w{line[i], line[i + 1], line[i + 2]};
                scr.push_back({projectPoint(w).x, projectPoint(w).y});
                last3 = w;
            }
            for (size_t i = 1; i < scr.size(); ++i) {
                float d = distPtSeg(sx, sy, scr[i - 1].x, scr[i - 1].y, scr[i].x, scr[i].y);
                if (d < bestD) {
                    bestD = d;
                    found = true;
                    // 重建整条棱: 找 OCCT 棱
                    // 简化: 记录折线中点作为锚点
                    size_t mid = line.size() / 6 * 3;
                    if (out.bodyId != b.id || true) {
                        out.bodyId = b.id;
                        out.kind = 1;
                        out.anchor = {line[mid], line[mid + 1], line[mid + 2]};
                    }
                }
            }
        }
    }
    if (!found) return false;
    // 找最近的 OCCT 棱
    Body* b = doc.body(out.bodyId);
    if (!b || b->result.IsNull()) return false;
    gp_Pnt anchor(out.anchor.x, out.anchor.y, out.anchor.z);
    TopoDS_Edge bestE;
    double bestED = 1e30;
    TopExp_Explorer ex(b->result, TopAbs_EDGE);
    for (; ex.More(); ex.Next()) {
        TopoDS_Edge e = TopoDS::Edge(ex.Current());
        try {
            BRepAdaptor_Curve c(e);
            GCPnts_QuasiUniformDeflection d(c, 0.05);
            if (!d.IsDone()) continue;
            double dd = 1e30;
            for (int i = 1; i <= d.NbPoints(); ++i) dd = std::min(dd, d.Value(i).Distance(anchor));
            if (dd < bestED) {
                bestED = dd;
                bestE = e;
            }
        } catch (...) {
        }
    }
    if (bestE.IsNull()) return false;
    out.shape = bestE;
    out.label = "棱";
    return true;
}

bool Application::pickVertex(float sx, float sy, SelRef& out) {
    double bestD = 7.0;
    bool found = false;
    for (auto& b : doc.bodies) {
        if (!b.visible) continue;
        const MeshData* md = doc.mesh(b.id);
        if (!md) continue;
        for (auto& line : md->edgeLines) {
            size_t npts = line.size() / 3;
            for (size_t i : {size_t(0), npts - 1}) {
                Vec3 w{line[i * 3], line[i * 3 + 1], line[i * 3 + 2]};
                Vec3 s = projectPoint(w);
                float d = std::hypot(s.x - sx, s.y - sy);
                if (d < bestD) {
                    bestD = d;
                    found = true;
                    out.bodyId = b.id;
                    out.kind = 2;
                    out.anchor = w;
                }
            }
        }
    }
    if (!found) return false;
    Body* b = doc.body(out.bodyId);
    if (!b || b->result.IsNull()) return false;
    gp_Pnt anchor(out.anchor.x, out.anchor.y, out.anchor.z);
    TopExp_Explorer ex(b->result, TopAbs_VERTEX);
    for (; ex.More(); ex.Next()) {
        TopoDS_Vertex v = TopoDS::Vertex(ex.Current());
        gp_Pnt p = BRep_Tool::Pnt(v);
        if (p.Distance(anchor) < 1e-6) {
            out.shape = v;
            out.label = "顶点";
            return true;
        }
    }
    return false;
}

bool Application::pickPlanarFace(float sx, float sy, SelRef& out, gp_Pln* plnOut) {
    PickHit h = pickBody(sx, sy);
    if (!h.hit || h.faceIdx < 0) return false;
    const MeshData* md = doc.mesh(h.bodyId);
    TopoDS_Shape s = md->faceMap(h.faceIdx + 1);
    if (s.IsNull() || s.ShapeType() != TopAbs_FACE) return false;
    TopoDS_Face f = TopoDS::Face(s);
    BRepAdaptor_Surface surf(f, Standard_False);
    if (surf.GetType() != GeomAbs_Plane) return false;
    Body* b = doc.body(h.bodyId);
    if (!b) return false;
    out.bodyId = b->id;
    out.kind = 0;
    out.shape = f;
    out.anchor = h.point;
    out.label = "平面";
    if (plnOut) *plnOut = surf.Plane();
    return true;
}

Vec3 Application::worldAt(float sx, float sy, double z) {
    Vec3 ro, rd;
    renderer.rayAt(sx, sy, cam, ro, rd, viewW(), viewH());
    if (std::fabs(rd.z) < 1e-6) return {(float)ro.x, (float)ro.y, (float)z};
    double t = (z - ro.z) / rd.z;
    if (t < 0) t = 0;
    Vec3 p = ro + rd * (float)t;
    return p;
}

Vec3 Application::rayOnPlane(float sx, float sy, const gp_Pln& pln, bool* ok) {
    Vec3 ro, rd;
    renderer.rayAt(sx, sy, cam, ro, rd, viewW(), viewH());
    gp_Pnt O = pln.Location();
    gp_Dir N = pln.Axis().Direction();
    Vec3 n{(float)N.X(), (float)N.Y(), (float)N.Z()};
    Vec3 o{(float)O.X(), (float)O.Y(), (float)O.Z()};
    Vec3 d{ro + rd * 1000.f};
    float denom = n.dot(rd);
    if (std::fabs(denom) < 1e-8f) {
        if (ok) *ok = false;
        return {};
    }
    float t = n.dot(o - ro) / denom;
    if (ok) *ok = true;
    Vec3 p = ro + rd * t;
    return p;
}

const char* Application::statusHint() const {
    if (mode == Mode::Sketch) {
        switch (tool) {
            case Tool::SketchLine: return "直线: 单击放置顶点, 双击/ESC 结束; 拖动顶点可修改";
            case Tool::SketchRect: return "矩形: 单击两角";
            case Tool::SketchCircle: return "圆: 单击圆心, 再单击半径";
            case Tool::SketchArc: return "圆弧: 依次单击 起点/终点/弧上一点";
            case Tool::SketchPolygon: return "多边形: 单击中心, 再单击顶点";
            default: return "草图模式: 选择/拖动几何; S 快捷键查看工具";
        }
    }
    if (mode == Mode::Drawing) return "工程图: 滚轮缩放, 中键拖动平移";
    switch (tool) {
        case Tool::Select: return "选择: 点击面/棱/顶点(Shift 加选), 右键拖动旋转视角";
        case Tool::Extrude: return "拉伸: 选择面或含轮廓的草图, 拖动箭头或输入距离";
        case Tool::Revolve: return "旋转: 选择草图与轴线, 设定角度";
        case Tool::Fillet: return "圆角: 选择棱, 输入半径后应用";
        case Tool::Chamfer: return "倒角: 选择棱, 输入距离后应用";
        case Tool::Shell: return "抽壳: 选择要移除的面, 输入厚度";
        case Tool::PushPull: return "同步拖面: 按住面拖动, 即时加材料/去材料";
        case Tool::Measure: return "测量: 依次点击两个图元";
        default: return "右键旋转 | 中键平移 | 滚轮缩放 | S 草图 E 拉伸 F 圆角";
    }
}

// ---------------- 主循环 ----------------
int Application::run() {
    g_app = this;
    if (!glfwInit()) {
        LOGE("GLFW 初始化失败");
        return 1;
    }
    glfwSetErrorCallback(errorCb);

    GLFWwindow* win = nullptr;
    std::string glInfo;
    // 1) 优先 OpenGL ES 2.0 (EGL)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, 8);
    win = glfwCreateWindow(1500, 940, "Shapr3D 桌面版 · OCCT 内核", nullptr, nullptr);
    if (win) glInfo = "OpenGL ES 2.0 (EGL)";
    // 2) 回退桌面 GL
    if (!win) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_SAMPLES, 8);
        win = glfwCreateWindow(1500, 940, "Shapr3D 桌面版 · OCCT 内核", nullptr, nullptr);
        if (win) glInfo = "Desktop OpenGL";
    }
    // 3) 无 MSAA 重试
    if (!win) {
        glfwWindowHint(GLFW_SAMPLES, 0);
        win = glfwCreateWindow(1500, 940, "Shapr3D 桌面版 · OCCT 内核", nullptr, nullptr);
    }
    if (!win) {
        LOGE("窗口创建失败(需要支持 OpenGL ES 2.0 或 OpenGL 2.1 的驱动)");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    g_win = win;

    if (!gl::load((void* (*)(const char*))glfwGetProcAddress)) {
        LOGE("GL 函数加载失败");
        return 1;
    }
    if (!renderer.init()) return 1;

    // 字体
    std::string font = "third_party/fonts/SimHei.ttf";
    {
        FILE* f = fopen(font.c_str(), "rb");
        if (!f) font.clear();
        else fclose(f);
    }
    ui::ImGuiInit(win, font);

    glfwSetScrollCallback(win, scrollCb);
    glfwSetCharCallback(win, charCb);

    buildWelcomeDocument(doc);
    frameAll();

    // 主循环
    double lastX = 0, lastY = 0;
    bool orbiting = false, panning = false;
    bool lmbDown = false, lmbWasDown = false;
    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        int dw, dh, fbw, fbh;
        glfwGetWindowSize(win, &dw, &dh);
        glfwGetFramebufferSize(win, &fbw, &fbh);
        if (dw == 0 || dh == 0) continue;
        renderer.resize(fbw, fbh);

        double mx, my;
        glfwGetCursorPos(win, &mx, &my);
        bool overUi = ImGui::GetIO().WantCaptureMouse;
        lmbWasDown = lmbDown;
        lmbDown = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        bool mmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        bool shift = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        // ---- 相机操作 ----
        if (!overUi) {
            if (rmb && shift) {
                // 平移
                float k = cam.dist * 0.0016f;
                Vec3 f = cam.viewDir();
                Vec3 r = f.cross(Vec3{0, 0, 1}).normalized();
                Vec3 u = r.cross(f);
                cam.target = cam.target - r * (float)((mx - lastX) * k) + u * (float)((my - lastY) * k);
                panning = true;
            } else if (rmb) {
                cam.yaw += (float)(mx - lastX) * 0.008f;
                cam.pitch = std::max(-1.55f, std::min(1.55f, cam.pitch + (float)(my - lastY) * 0.008f));
                orbiting = true;
            } else if (mmb) {
                float k = cam.dist * 0.0016f;
                Vec3 f = cam.viewDir();
                Vec3 r = f.cross(Vec3{0, 0, 1}).normalized();
                Vec3 u = r.cross(f);
                cam.target = cam.target - r * (float)((mx - lastX) * k) + u * (float)((my - lastY) * k);
                panning = true;
            } else {
                orbiting = panning = false;
            }
        }
        lastX = mx;
        lastY = my;

        ui::ImGuiNewFrame(win);
        handleFrameInput(win, (float)mx, (float)my, lmbDown, lmbWasDown, overUi);
        ui::drawAll(*this);

        // ---- 渲染场景 ----
        renderer.beginFrame(cam, rsettings);
        if (mode != Mode::Drawing) {
            renderer.drawShadow(doc);
            renderer.drawGroundQuad(cam, std::max(200.f, cam.dist * 1.2f));
            for (auto& b : doc.bodies) {
                const MeshData* md = doc.mesh(b.id);
                if (!md || md->verts.empty()) continue;
                Highlight hl = Highlight::None;
                for (auto& s : selBodies)
                    if (s.bodyId == b.id) hl = Highlight::Selected;
                if (!b.visible || b.hiddenByOp) continue;
                renderer.drawBody(b.id, *md, b.material, hl);
                if (rsettings.mode != ViewMode::Shaded)
                    renderer.drawEdges(*md, {0.12f, 0.15f, 0.2f}, 0.85f);
            }
            // 轴与草图
            if (mode == Mode::Sketch || rsettings.showSketches)
                renderer.drawSketch(doc, activeSketchId, rubberLines, snapMarkers);
            renderer.drawAxes(60);
            // 测量线
            if (hasMeasureA && hasMeasureB) {
                renderer.drawPolylines3D({{measureA.anchor, measureB.anchor}}, {0.95f, 0.3f, 0.15f, 1});
            }
            renderer.endFrame();
        } else {
            renderer.endFrame();
            // 制图背景
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            dl->AddRectFilled({0, 0}, {(float)dw, (float)dh}, IM_COL32(52, 56, 64, 255));
            ui::drawDrawingCanvas(*this, dl, (float)dw, (float)dh);
        }

        ImGui::Render();
        ui::ImGuiRender();
        glfwSwapBuffers(win);
    }

    ui::ImGuiShutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

} // namespace cad

namespace cad {

// ---------------- 每帧交互 ----------------
Vec2 Application::snapSketchPoint(const gp_Pln& pln, float mx, float my) {
    bool ok = false;
    Vec3 p3 = rayOnPlane(mx, my, pln, &ok);
    Vec2 p{};
    if (!ok) return {};
    SketchDef* sk = doc.sketch(activeSketchId);
    if (sk) p = sk->to2D(gp_Pnt(p3.x, p3.y, p3.z));
    snapMarkers.clear();

    // 顶点吸附(屏幕距离)
    if (sk) {
        Id bestP = kInvalidId;
        double bestD = 9.0;
        for (auto& q : sk->points) {
            gp_Pnt w = sk->to3D(q.x, q.y);
            Vec3 s = projectPoint({(float)w.X(), (float)w.Y(), (float)w.Z()});
            double d = std::hypot(s.x - mx, s.y - my);
            if (d < bestD) {
                bestD = d;
                bestP = q.id;
            }
        }
        if (bestP) {
            Vec2 pos = sk->pointPos(bestP);
            gp_Pnt w = sk->to3D(pos.x, pos.y);
            snapMarkers.push_back({(float)w.X(), (float)w.Y(), (float)w.Z()});
            return pos;
        }
    }
    // 网格吸附
    if (gridSnap) {
        Vec2 g{std::round(p.x), std::round(p.y)};
        gp_Pnt w = sk ? sk->to3D(g.x, g.y) : gp_Pnt(g.x, g.y, 0);
        snapMarkers.push_back({(float)w.X(), (float)w.Y(), (float)w.Z()});
        return g;
    }
    return p;
}

void Application::handleFrameInput(GLFWwindow* win, float mx, float my, bool lmb, bool lmbPrev, bool overUi) {
    ImGuiIO& io = ImGui::GetIO();
    double nowMs = Stopwatch::nowMs();
    static double lastClickMs = 0;
    static float lastClickX = 0, lastClickY = 0;

    // ---------- 快捷键 ----------
    if (!io.WantTextInput) {
        bool ctrl = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        auto pressed = [&](int k) {
            int st = glfwGetKey(win, k);
            static std::unordered_map<int, bool> was;
            bool w = was[k];
            was[k] = st == GLFW_PRESS;
            return st == GLFW_PRESS && !w;
        };
        if (ctrl) {
            if (pressed(GLFW_KEY_Z)) doUndo();
            else if (pressed(GLFW_KEY_Y)) doRedo();
            else if (pressed(GLFW_KEY_S)) {
                std::string path = doc.filePath.empty() ? doc.name + ".scn" : doc.filePath;
                if (doc.saveToFile(path)) showToast("已保存: " + path);
            }
        } else if (mode == Mode::Object) {
            if (pressed(GLFW_KEY_S)) {
                gp_Pln pln = planeXY();
                if (!selFaces.empty()) {
                    BRepAdaptor_Surface surf(TopoDS::Face(selFaces[0].shape), Standard_False);
                    if (surf.GetType() == GeomAbs_Plane) pln = surf.Plane();
                }
                startNewSketch(pln);
            } else if (pressed(GLFW_KEY_E)) { tool = Tool::Extrude; axisDragging = false; }
            else if (pressed(GLFW_KEY_R)) tool = Tool::Revolve;
            else if (pressed(GLFW_KEY_F)) tool = Tool::Fillet;
            else if (pressed(GLFW_KEY_C)) tool = Tool::Chamfer;
            else if (pressed(GLFW_KEY_T)) tool = Tool::Shell;
            else if (pressed(GLFW_KEY_B)) booleanOp(0);
            else if (pressed(GLFW_KEY_M)) { tool = Tool::Measure; hasMeasureA = hasMeasureB = false; }
            else if (pressed(GLFW_KEY_P)) tool = Tool::PushPull;
            else if (pressed(GLFW_KEY_G)) gridSnap = !gridSnap;
            else if (pressed(GLFW_KEY_1)) alignView(0);
            else if (pressed(GLFW_KEY_2)) alignView(4);
            else if (pressed(GLFW_KEY_3)) alignView(2);
            else if (pressed(GLFW_KEY_0)) alignView(6);
            else if (pressed(GLFW_KEY_ESCAPE)) { tool = Tool::Select; clearSelection(); }
            else if (pressed(GLFW_KEY_DELETE) || pressed(GLFW_KEY_BACKSPACE)) clearSelection();
        } else if (mode == Mode::Sketch) {
            if (pressed(GLFW_KEY_L)) tool = Tool::SketchLine;
            else if (pressed(GLFW_KEY_R)) tool = Tool::SketchRect;
            else if (pressed(GLFW_KEY_C)) tool = Tool::SketchCircle;
            else if (pressed(GLFW_KEY_A)) tool = Tool::SketchArc;
            else if (pressed(GLFW_KEY_G)) gridSnap = !gridSnap;
            else if (pressed(GLFW_KEY_ESCAPE)) {
                if (!pendingPts.empty()) {
                    pendingPts.clear();
                    rubberLines.clear();
                } else {
                    exitSketch();
                }
            }
        } else if (mode == Mode::Drawing) {
            if (pressed(GLFW_KEY_ESCAPE)) mode = Mode::Object;
        }
    }

    bool clicked = lmb && !lmbPrev;
    bool released = !lmb && lmbPrev;
    if (overUi && !axisDragging && !dragging && !pull.active) return;

    // ================= 草图模式 =================
    if (mode == Mode::Sketch) {
        SketchDef* sk = doc.sketch(activeSketchId);
        if (!sk) return;
        cursorPlane = snapSketchPoint(sk->plane, mx, my);

        if (lmb && !clicked && !dragging) {
            Id hit = hitSketchPoint({mx, my}, 10);
            if (hit) {
                dragPointId = hit;
                dragging = true;
            }
        }
        if (dragging && dragPointId && lmb) {
            ConstraintSolver s;
            lastSolve = s.solveDrag(*sk, dragPointId, cursorPlane);
        }
        if (released && dragging) {
            dragging = false;
            dragPointId = kInvalidId;
            solveSketch();
            return;
        }

        rubberLines.clear();
        if (tool == Tool::SketchLine && pendingPts.size() == 1) {
            gp_Pnt a = sk->to3D(pendingPts[0].x, pendingPts[0].y);
            gp_Pnt b = sk->to3D(cursorPlane.x, cursorPlane.y);
            rubberLines.push_back({{(float)a.X(), (float)a.Y(), (float)a.Z()},
                                   {(float)b.X(), (float)b.Y(), (float)b.Z()}});
        } else if (tool == Tool::SketchRect && pendingPts.size() == 1) {
            Vec2 a = pendingPts[0], b = cursorPlane;
            std::vector<Vec2> c = {a, {b.x, a.y}, b, {a.x, b.y}};
            for (int i = 0; i < 4; ++i) {
                gp_Pnt p1 = sk->to3D(c[i].x, c[i].y);
                gp_Pnt p2 = sk->to3D(c[(i + 1) % 4].x, c[(i + 1) % 4].y);
                rubberLines.push_back({{(float)p1.X(), (float)p1.Y(), (float)p1.Z()},
                                       {(float)p2.X(), (float)p2.Y(), (float)p2.Z()}});
            }
        }

        if (!clicked) return;

        bool doubleClick = (nowMs - lastClickMs < 350) && std::hypot(mx - lastClickX, my - lastClickY) < 6;
        lastClickMs = nowMs;
        lastClickX = mx;
        lastClickY = my;

        pushUndo();
        switch (tool) {
            case Tool::SketchLine: {
                if (doubleClick && pendingPts.size() >= 2) {
                    pendingPts.clear();
                    rubberLines.clear();
                    solveSketch();
                    break;
                }
                Id hit = hitSketchPoint({mx, my}, 9);
                Id np = doc.newId();
                if (pendingPts.empty()) {
                    if (hit) pendingPts.push_back(sk->pointPos(hit));
                    else pendingPts.push_back(cursorPlane);
                } else {
                    Vec2 a = pendingPts[0];
                    Id pa = doc.newId();
                    sk->addPoint(a.x, a.y, pa);
                    if (hit) {
                        // 终点吸附到已有点: 直接复用, 几何天然重合
                        np = hit;
                    } else {
                        sk->addPoint(cursorPlane.x, cursorPlane.y, np);
                    }
                    sk->addLine(pa, np, doc.newId());
                    pendingPts.clear();
                    pendingPts.push_back(sk->pointPos(np));
                    solveSketch();
                }
                break;
            }
            case Tool::SketchRect: {
                pendingPts.push_back(cursorPlane);
                if (pendingPts.size() == 2) {
                    Vec2 a = pendingPts[0], b = pendingPts[1];
                    Id p1 = doc.newId(), p2 = doc.newId(), p3 = doc.newId(), p4 = doc.newId();
                    sk->addPoint(a.x, a.y, p1);
                    sk->addPoint(b.x, a.y, p2);
                    sk->addPoint(b.x, b.y, p3);
                    sk->addPoint(a.x, b.y, p4);
                    Id l1 = doc.newId(), l2 = doc.newId(), l3 = doc.newId(), l4 = doc.newId();
                    sk->addLine(p1, p2, l1);
                    sk->addLine(p2, p3, l2);
                    sk->addLine(p3, p4, l3);
                    sk->addLine(p4, p1, l4);
                    auto addC = [&](CstType t, std::vector<Id> refs, double v = 0) {
                        Constraint c;
                        c.id = doc.newId();
                        c.type = t;
                        c.refs = refs;
                        c.value = v;
                        sk->constraints.push_back(c);
                    };
                    addC(CstType::Horizontal, {l1});
                    addC(CstType::Vertical, {l2});
                    addC(CstType::Horizontal, {l3});
                    addC(CstType::Vertical, {l4});
                    pendingPts.clear();
                    rubberLines.clear();
                    solveSketch();
                }
                break;
            }
            case Tool::SketchCircle: {
                pendingPts.push_back(cursorPlane);
                if (pendingPts.size() == 2) {
                    Vec2 c = pendingPts[0];
                    double r = dist(c, pendingPts[1]);
                    Id cid = addSketchCircle(c, r);
                    Constraint d;
                    d.id = doc.newId();
                    d.type = CstType::Radius;
                    d.refs = {cid};
                    d.value = r;
                    sk->constraints.push_back(d);
                    pendingPts.clear();
                    solveSketch();
                }
                break;
            }
            case Tool::SketchArc: {
                pendingPts.push_back(cursorPlane);
                if (pendingPts.size() == 3) {
                    Vec2 a = pendingPts[0], b = pendingPts[1], m = pendingPts[2];
                    double d = 2 * (a.x * (b.y - m.y) + b.x * (m.y - a.y) + m.x * (a.y - b.y));
                    if (std::fabs(d) > 1e-9) {
                        double ux = ((a.x * a.x + a.y * a.y) * (b.y - m.y) + (b.x * b.x + b.y * b.y) * (m.y - a.y) +
                                     (m.x * m.x + m.y * m.y) * (a.y - b.y)) / d;
                        double uy = ((a.x * a.x + a.y * a.y) * (m.x - b.x) + (b.x * b.x + b.y * b.y) * (a.x - m.x) +
                                     (m.x * m.x + m.y * m.y) * (b.x - a.x)) / d;
                        Vec2 c{ux, uy};
                        double r = dist(c, a);
                        double a0 = std::atan2(a.y - uy, a.x - ux);
                        double a1 = std::atan2(b.y - uy, b.x - ux);
                        double am = std::atan2(m.y - uy, m.x - ux);
                        double sweep = a1 - a0;
                        while (sweep < 0) sweep += 2 * M_PI;
                        while (am < a0) am += 2 * M_PI;
                        if (am > a0 + sweep) std::swap(a0, a1);
                        Id cid = doc.newId(), pid = doc.newId();
                        sk->addPoint(ux, uy, pid);
                        sk->addArc(pid, r, a0, a1, cid);
                        Id pa = doc.newId(), pb = doc.newId();
                        sk->addPoint(a.x, a.y, pa);
                        sk->addPoint(b.x, b.y, pb);
                        auto addC = [&](CstType t, std::vector<Id> refs, double v = 0) {
                            Constraint c2;
                            c2.id = doc.newId();
                            c2.type = t;
                            c2.refs = refs;
                            c2.value = v;
                            sk->constraints.push_back(c2);
                        };
                        addC(CstType::PointOnCircle, {pa, cid});
                        addC(CstType::PointOnCircle, {pb, cid});
                    }
                    pendingPts.clear();
                    solveSketch();
                }
                break;
            }
            case Tool::SketchPolygon: {
                pendingPts.push_back(cursorPlane);
                if (pendingPts.size() == 2) {
                    Vec2 c = pendingPts[0];
                    double r = dist(c, pendingPts[1]);
                    double a0 = std::atan2(pendingPts[1].y - c.y, pendingPts[1].x - c.x);
                    std::vector<Id> pids;
                    for (int i = 0; i < polygonSides; ++i) {
                        double a = a0 + 2 * M_PI * i / polygonSides;
                        Vec2 p{c.x + r * std::cos(a), c.y + r * std::sin(a)};
                        Id pid = doc.newId();
                        sk->addPoint(p.x, p.y, pid);
                        pids.push_back(pid);
                    }
                    for (int i = 0; i < polygonSides; ++i)
                        sk->addLine(pids[i], pids[(i + 1) % polygonSides], doc.newId());
                    pendingPts.clear();
                    solveSketch();
                }
                break;
            }
            default:
                break;
        }
        return;
    }

    if (mode != Mode::Object) return;

    // ================= Object 模式 =================
    if (axisDragging && lmb) {
        bool ok = false;
        Vec3 p = rayOnPlane(mx, my, gp_Pln(gp_Ax3(gp_Pnt(axisOrigin.x, axisOrigin.y, axisOrigin.z),
                                                  gp_Dir(axisDir.x, axisDir.y, axisDir.z))),
                            &ok);
        if (ok) {
            Vec3 d = p - axisOrigin;
            axisValue = (double)d.dot(axisDir);
        }
        return;
    }
    if (axisDragging && released) {
        axisDragging = false;
        return;
    }

    if (pull.active && lmb) {
        bool ok = false;
        gp_Pln dragPlane(gp_Ax3(gp_Pnt(pull.startPoint.x, pull.startPoint.y, pull.startPoint.z),
                                gp_Dir(cam.viewDir().x, cam.viewDir().y, cam.viewDir().z)));
        Vec3 p = rayOnPlane(mx, my, dragPlane, &ok);
        if (ok) {
            Vec3 n{(float)pull.plane.Axis().Direction().X(), (float)pull.plane.Axis().Direction().Y(),
                   (float)pull.plane.Axis().Direction().Z()};
            pull.value = (double)(p - pull.startPoint).dot(n);
            pull.inward = pull.value < 0;
        }
        return;
    }
    if (pull.active && released) {
        double v = std::fabs(pull.value);
        if (v > 1e-3) {
            gp_Pln pln = pull.plane;
            if (pull.inward)
                pln = gp_Pln(gp_Ax3(pull.plane.Location(),
                                    gp_Dir(pull.plane.Axis().Direction().Reversed()),
                                    pull.plane.XAxis().Direction()));
            TopoDS_Shape base = doc.body(pull.bodyId) ? doc.body(pull.bodyId)->result : TopoDS_Shape();
            gp_Vec dir(pln.Axis().Direction());
            dir *= v;
            TopoDS_Shape prism = BRepPrimAPI_MakePrism(pull.face, dir, Standard_False, Standard_True).Shape();
            pushUndo();
            Feature fb;
            fb.id = doc.newId();
            fb.type = FeatureType::Imported;
            fb.name = pull.inward ? "拖面切除" : "拖面凸台";
            Body* b = doc.body(pull.bodyId);
            if (b) {
                try {
                    if (pull.inward) {
                        BRepAlgoAPI_Cut cu(base, prism);
                        fb.result = cu.Shape();
                    } else {
                        BRepAlgoAPI_Fuse fu(base, prism);
                        fb.result = fu.Shape();
                    }
                    b->features.push_back(fb);
                    recompute();
                    showToast(fb.name + " 完成");
                } catch (Standard_Failure& e) {
                    showToast(e.GetMessageString() ? e.GetMessageString() : "拖面失败", true);
                }
            }
        }
        pull.active = false;
        return;
    }

    if (!clicked) return;

    switch (tool) {
        case Tool::Select: {
            SelRef ref;
            if (pickVertex(mx, my, ref)) {
                if (io.KeyShift) selVertices.push_back(ref);
                else {
                    clearSelection();
                    selVertices.push_back(ref);
                }
                showToast("已选择顶点");
            } else if (pickEdge(mx, my, ref)) {
                if (io.KeyShift) selEdges.push_back(ref);
                else {
                    clearSelection();
                    selEdges.push_back(ref);
                }
                showToast("已选择棱");
            } else {
                PickHit h = pickBody(mx, my);
                if (h.hit && h.faceIdx >= 0) {
                    const MeshData* md = doc.mesh(h.bodyId);
                    TopoDS_Shape s = md->faceMap(h.faceIdx + 1);
                    ref.bodyId = h.bodyId;
                    ref.kind = 0;
                    ref.shape = s;
                    ref.anchor = h.point;
                    ref.label = "面";
                    if (io.KeyShift) selFaces.push_back(ref);
                    else {
                        clearSelection();
                        selFaces.push_back(ref);
                    }
                } else {
                    clearSelection();
                }
            }
            break;
        }
        case Tool::Extrude: {
            PickHit h = pickBody(mx, my);
            if (h.hit && h.faceIdx >= 0) {
                const MeshData* md = doc.mesh(h.bodyId);
                TopoDS_Shape s = md->faceMap(h.faceIdx + 1);
                selFaces.clear();
                SelRef ref;
                ref.bodyId = h.bodyId;
                ref.shape = s;
                ref.kind = 0;
                ref.anchor = h.point;
                selFaces.push_back(ref);
            }
            Vec3 dir{0, 0, 1};
            if (!selFaces.empty()) {
                BRepAdaptor_Surface surf(TopoDS::Face(selFaces[0].shape), Standard_False);
                if (surf.GetType() == GeomAbs_Plane) {
                    gp_Dir n = surf.Plane().Axis().Direction();
                    dir = {(float)n.X(), (float)n.Y(), (float)n.Z()};
                }
            }
            axisDir = dir;
            axisOrigin = selFaces.empty() ? Vec3{} : selFaces[0].anchor;
            axisSketchId = activeSketchId;
            axisDragging = true;
            axisValue = extrudeInput;
            break;
        }
        case Tool::Fillet:
        case Tool::Chamfer: {
            SelRef ref;
            if (pickEdge(mx, my, ref)) {
                selEdges.push_back(ref);
                showToast("已加入圆角/倒角棱集: " + std::to_string(selEdges.size()));
            }
            break;
        }
        case Tool::Shell: {
            SelRef ref;
            gp_Pln pln;
            if (pickPlanarFace(mx, my, ref, &pln)) {
                selFaces.push_back(ref);
                showToast("抽壳开口面: " + std::to_string(selFaces.size()));
            }
            break;
        }
        case Tool::PushPull: {
            SelRef ref;
            gp_Pln pln;
            if (pickPlanarFace(mx, my, ref, &pln)) {
                pull.active = true;
                pull.bodyId = ref.bodyId;
                pull.face = TopoDS::Face(ref.shape);
                pull.plane = pln;
                pull.startPoint = ref.anchor;
                pull.value = 0;
                pull.inward = false;
            }
            break;
        }
        case Tool::Measure: {
            SelRef ref;
            bool got = pickVertex(mx, my, ref) || pickEdge(mx, my, ref) || pickPlanarFace(mx, my, ref, nullptr);
            if (!got) {
                PickHit h = pickBody(mx, my);
                if (h.hit && h.faceIdx >= 0) {
                    const MeshData* md = doc.mesh(h.bodyId);
                    ref.bodyId = h.bodyId;
                    ref.kind = 0;
                    ref.shape = md->faceMap(h.faceIdx + 1);
                    ref.anchor = h.point;
                    got = true;
                }
            }
            if (got) {
                if (!hasMeasureA || hasMeasureB) {
                    measureA = ref;
                    hasMeasureA = true;
                    hasMeasureB = false;
                } else {
                    measureB = ref;
                    hasMeasureB = true;
                    if (measureA.shape.ShapeType() == TopAbs_FACE && measureB.shape.ShapeType() == TopAbs_FACE) {
                        BRepAdaptor_Surface sa(TopoDS::Face(measureA.shape), Standard_False);
                        BRepAdaptor_Surface sb(TopoDS::Face(measureB.shape), Standard_False);
                        if (sa.GetType() == GeomAbs_Plane && sb.GetType() == GeomAbs_Plane)
                            measureRes = measureAngle(TopoDS::Face(measureA.shape), TopoDS::Face(measureB.shape));
                        else
                            measureRes = measureDistance(measureA.shape, measureB.shape);
                    } else {
                        measureRes = measureDistance(measureA.shape, measureB.shape);
                    }
                    showToast(measureRes.ok ? "测量: " + measureRes.text : measureRes.error, !measureRes.ok);
                }
            }
            break;
        }
        default:
            break;
    }
}

} // namespace cad
