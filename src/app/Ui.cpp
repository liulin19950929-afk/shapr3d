// Ui.cpp —— Shapr3D 风格深色界面
#include "Ui.h"
#include "ImGuiBackend.h"
#include "../render/Math3D.h"
#include "../analysis/Measure.h"
#include "../core/ThreadPool.h"
#include <cstdio>
#include <cstring>

namespace cad::ui {

using ImGui::Begin;
using ImGui::Text;
using ImGui::Button;
using ImGui::SameLine;
using ImGui::End;
using ImGui::BeginPopupModal;
using ImGui::EndPopup;
using ImGui::InputText;
using ImGui::InputDouble;
using ImGui::Checkbox;
using ImGui::CollapsingHeader;
using ImGui::Selectable;
using ImGui::ColorEdit3;
using ImGui::SliderFloat;
using ImGui::BeginCombo;
using ImGui::EndCombo;
using ImGui::BulletText;
using ImGui::Bullet;
using ImGui::Separator;
using ImGui::SetTooltip;
using ImGui::IsItemHovered;
using ImGui::PushID;
using ImGui::PopID;
using ImGui::PushStyleColor;
using ImGui::PopStyleColor;
using ImGui::SetNextItemWidth;
using ImGui::SetNextWindowPos;
using ImGui::SetNextWindowSize;
using ImGui::TextColored;
using ImGui::TextDisabled;
using ImGui::GetWindowWidth;

static bool themeInit = false;

static void initTheme() {
    if (themeInit) return;
    themeInit = true;
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6;
    s.FrameRounding = 4;
    s.GrabRounding = 4;
    s.PopupRounding = 4;
    s.TabRounding = 4;
    s.ScrollbarRounding = 8;
    s.WindowPadding = {10, 8};
    s.FramePadding = {8, 5};
    s.ItemSpacing = {8, 6};
    s.WindowBorderSize = 0;
    s.FrameBorderSize = 0;
    s.TabBarBorderSize = 0;
    s.ChildRounding = 6;

    ImVec4* c = s.Colors;
    auto rgb = [](int r, int g, int b, int a = 255) {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    };
    c[ImGuiCol_WindowBg] = rgb(33, 35, 40);
    c[ImGuiCol_ChildBg] = rgb(39, 41, 47);
    c[ImGuiCol_PopupBg] = rgb(43, 45, 52, 250);
    c[ImGuiCol_Border] = rgb(60, 63, 70);
    c[ImGuiCol_FrameBg] = rgb(50, 53, 60);
    c[ImGuiCol_FrameBgHovered] = rgb(58, 62, 70);
    c[ImGuiCol_FrameBgActive] = rgb(64, 68, 78);
    c[ImGuiCol_TitleBg] = rgb(28, 30, 34);
    c[ImGuiCol_TitleBgActive] = rgb(28, 30, 34);
    c[ImGuiCol_MenuBarBg] = rgb(30, 32, 37);
    c[ImGuiCol_Text] = rgb(226, 228, 233);
    c[ImGuiCol_TextDisabled] = rgb(128, 132, 140);
    c[ImGuiCol_Header] = rgb(55, 60, 70);
    c[ImGuiCol_HeaderHovered] = rgb(64, 70, 82);
    c[ImGuiCol_HeaderActive] = rgb(72, 78, 92);
    c[ImGuiCol_Button] = rgb(52, 56, 64);
    c[ImGuiCol_ButtonHovered] = rgb(62, 78, 110);
    c[ImGuiCol_ButtonActive] = rgb(45, 90, 190);
    c[ImGuiCol_CheckMark] = rgb(80, 140, 250);
    c[ImGuiCol_SliderGrab] = rgb(80, 140, 250);
    c[ImGuiCol_SliderGrabActive] = rgb(110, 165, 255);
    c[ImGuiCol_Separator] = rgb(60, 63, 70);
    c[ImGuiCol_Tab] = rgb(39, 41, 47);
    c[ImGuiCol_TabHovered] = rgb(62, 78, 110);
    c[ImGuiCol_TabActive] = rgb(45, 90, 190);
    c[ImGuiCol_ResizeGrip] = rgb(60, 63, 70, 120);
    c[ImGuiCol_PlotLines] = rgb(80, 140, 250);
}

static const char* cstName(CstType t) {
    switch (t) {
        case CstType::Coincident: return "重合";
        case CstType::Horizontal: return "水平";
        case CstType::Vertical: return "垂直";
        case CstType::Parallel: return "平行";
        case CstType::Perpendicular: return "垂直(线)";
        case CstType::Distance: return "距离";
        case CstType::DistPtLine: return "点线距";
        case CstType::Length: return "长度";
        case CstType::Radius: return "半径";
        case CstType::Diameter: return "直径";
        case CstType::Angle: return "角度";
        case CstType::Equal: return "相等";
        case CstType::Midpoint: return "中点";
        case CstType::PointOnLine: return "点在线上";
        case CstType::PointOnCircle: return "点在圆上";
        case CstType::Concentric: return "同心";
        case CstType::Fix: return "固定";
    }
    return "?";
}

// ---------------- 顶栏 ----------------
static void drawTopBar(Application& app) {
    if (!Begin("##topbar", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
                   ImGuiWindowFlags_NoSavedSettings)) {
        End();
        return;
    }
    ImGui::SetWindowPos({0, 0});
    ImGui::SetWindowSize({ImGui::GetIO().DisplaySize.x, 62});
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("文件")) {
            if (ImGui::MenuItem("新建工程", "Ctrl+N")) {
                app.pushUndo();
                app.doc = Document();
                buildWelcomeDocument(app.doc);
                app.recompute();
                app.frameAll();
            }
            if (ImGui::MenuItem("打开工程...", "Ctrl+O")) ImGui::OpenPopup("##openProject");
            if (ImGui::MenuItem("保存工程", "Ctrl+S")) {
                std::string path = app.doc.filePath.empty() ? app.doc.name + ".scn" : app.doc.filePath;
                if (app.doc.saveToFile(path)) app.showToast("已保存: " + path);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("导入模型")) {
                if (ImGui::MenuItem("STEP / IGES / BREP / STL...")) ImGui::OpenPopup("##importModel");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("导出模型")) {
                if (ImGui::MenuItem("STEP (.step)")) app.exportModel(app.doc.name + ".step");
                if (ImGui::MenuItem("IGES (.iges)")) app.exportModel(app.doc.name + ".iges");
                if (ImGui::MenuItem("STL (.stl)")) app.exportModel(app.doc.name + ".stl");
                if (ImGui::MenuItem("OBJ (.obj)")) app.exportModel(app.doc.name + ".obj");
                if (ImGui::MenuItem("BREP (.brep)")) app.exportModel(app.doc.name + ".brep");
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("视图截图 (PNG)", "F12"))
                app.screenshot("screenshot_" + std::to_string((int)(Stopwatch::nowMs() / 1000)) + ".png");
            if (ImGui::MenuItem("加载示例模型")) {
                app.pushUndo();
                app.doc = Document();
                buildWelcomeDocument(app.doc);
                app.recompute();
                app.frameAll();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("编辑")) {
            if (ImGui::MenuItem("撤销", "Ctrl+Z", false, !app.undoStack.empty())) app.doUndo();
            if (ImGui::MenuItem("重做", "Ctrl+Y", false, !app.redoStack.empty())) app.doRedo();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("视图")) {
            if (ImGui::MenuItem("全部显示", "Home")) app.frameAll();
            ImGui::Separator();
            if (ImGui::MenuItem("前视", "1")) app.alignView(0);
            if (ImGui::MenuItem("上视", "2")) app.alignView(4);
            if (ImGui::MenuItem("左视", "3")) app.alignView(2);
            if (ImGui::MenuItem("轴测", "0")) app.alignView(6);
            ImGui::Separator();
            static const char* modes[] = {"着色+边线", "着色", "线框"};
            int m = (int)app.rsettings.mode;
            if (ImGui::BeginCombo("显示模式", modes[m])) {
                for (int i = 0; i < 3; ++i)
                    if (ImGui::Selectable(modes[i], m == i)) app.rsettings.mode = (ViewMode)i;
                ImGui::EndCombo();
            }
            ImGui::Checkbox("网格", &app.rsettings.showGrid);
            ImGui::Checkbox("坐标轴", &app.rsettings.showAxes);
            ImGui::Checkbox("地面阴影", &app.rsettings.showShadow);
            ImGui::SliderFloat("阴影强度", &app.rsettings.shadowStrength, 0.f, 0.8f);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("工具")) {
            if (ImGui::MenuItem("测量", "M")) app.tool = Tool::Measure;
            if (ImGui::MenuItem("同步拖面", "P")) app.tool = Tool::PushPull;
            if (ImGui::MenuItem("网格吸附", "G", &app.gridSnap)) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("帮助")) {
            ImGui::TextUnformatted("快捷键:");
            ImGui::BulletText("S 草图  E 拉伸  R 旋转");
            ImGui::BulletText("F 圆角  C 倒角  T 抽壳");
            ImGui::BulletText("B 布尔并  P 拖面  M 测量");
            ImGui::BulletText("右键拖动=旋转 中键=平移 滚轮=缩放");
            ImGui::BulletText("1/2/3/0 前/上/左/轴测视图");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 模式切换页签
    SameLine(0, 24);
    ImGui::PushStyleColor(ImGuiCol_Tab, {0, 0, 0, 0});
    const char* modeNames[] = {"建模", "草图", "制图"};
    for (int i = 0; i < 3; ++i) {
        bool sel = (int)app.mode == i;
        if (sel) ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.72f, 1.f, 1});
        if (ImGui::Button(modeNames[i], {64, 24})) {
            if (i == 0) {
                if (app.mode == Mode::Sketch) app.exitSketch();
                app.mode = Mode::Object;
            } else if (i == 1) {
                if (app.activeSketchId) app.enterSketch(app.activeSketchId);
                else {
                    gp_Pln pln = planeXY();
                    if (!app.selFaces.empty()) {
                        BRepAdaptor_Surface surf(TopoDS::Face(app.selFaces[0].shape), Standard_False);
                        if (surf.GetType() == GeomAbs_Plane) pln = surf.Plane();
                    }
                    app.startNewSketch(pln);
                }
            } else {
                if (app.mode == Mode::Sketch) app.exitSketch();
                app.mode = Mode::Drawing;
                if (!app.drawingValid) app.generateDrawing();
            }
        }
        if (sel) ImGui::PopStyleColor();
        SameLine(0, 6);
    }
    ImGui::PopStyleColor();
    End();

    // ---- 文件对话框们 ----
    static char pathBuf[512] = "";
    auto dialog = [&](const char* title, const char* defaultPath, std::function<void(const char*)> onOk) {
        ImGui::SetNextWindowSize({520, 140}, ImGuiCond_Appearing);
        if (BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetNextItemWidth(440);
            if (pathBuf[0] == 0 && defaultPath) snprintf(pathBuf, sizeof(pathBuf), "%s", defaultPath);
            ImGui::InputText("##path", pathBuf, sizeof(pathBuf));
            SameLine();
            if (Button("确定")) {
                onOk(pathBuf);
                pathBuf[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            SameLine();
            if (Button("取消")) {
                pathBuf[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            EndPopup();
        }
    };
    dialog("##openProject", "model.scn", [&](const char* p) {
        if (app.doc.loadFromFile(p)) {
            app.recompute();
            app.frameAll();
            app.showToast("已打开: " + std::string(p));
        } else
            app.showToast("打开失败", true);
    });
    dialog("##importModel", "part.step", [&](const char* p) { app.importModel(p); });
}

// ---------------- 左侧工具条 ----------------
static void toolBtn(Application& app, Tool t, const char* label, const char* tip) {
    bool active = app.tool == t;
    if (active) ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.35f, 0.75f, 1});
    if (Button(label, {58, 0})) {
        app.tool = t;
        if (t == Tool::Measure) {
            app.hasMeasureA = app.hasMeasureB = false;
        }
    }
    if (active) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && tip) ImGui::SetTooltip("%s", tip);
}

static void drawToolbar(Application& app) {
    ImGui::SetNextWindowPos({0, 62});
    ImGui::SetNextWindowSize({76, ImGui::GetIO().DisplaySize.y - 62 - 26});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.13f, 0.14f, 0.16f, 1});
    if (!Begin("##toolbar", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        End();
        ImGui::PopStyleColor();
        return;
    }
    if (app.mode == Mode::Sketch) {
        Text("草图");
        toolBtn(app, Tool::Select, "选择", "选择/拖动 (ESC)");
        toolBtn(app, Tool::SketchLine, "直线", "直线 L");
        toolBtn(app, Tool::SketchRect, "矩形", "矩形 R");
        toolBtn(app, Tool::SketchCircle, "圆", "圆 C");
        toolBtn(app, Tool::SketchArc, "圆弧", "三点圆弧 A");
        toolBtn(app, Tool::SketchPolygon, "多边形", "多边形(中心+顶点)");
    } else if (app.mode == Mode::Drawing) {
        Text("制图");
        toolBtn(app, Tool::DraftLine, "直线", "2D 直线");
        toolBtn(app, Tool::DraftCircle, "圆", "2D 圆");
        toolBtn(app, Tool::DraftDim, "尺寸", "2D 尺寸标注");
        toolBtn(app, Tool::DraftText, "文字", "2D 文字注记");
    } else {
        Text("建模");
        toolBtn(app, Tool::Select, "选择", "选择");
        toolBtn(app, Tool::Extrude, "拉伸", "拉伸 E");
        toolBtn(app, Tool::Revolve, "旋转", "旋转 R");
        toolBtn(app, Tool::Fillet, "圆角", "圆角 F");
        toolBtn(app, Tool::Chamfer, "倒角", "倒角 C");
        toolBtn(app, Tool::Shell, "抽壳", "抽壳 T");
        ImGui::Separator();
        if (Button("布尔并", {58, 0})) app.booleanOp(0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("选择多个面(Shift)后点击 B");
        if (Button("布尔差", {58, 0})) app.booleanOp(1);
        if (Button("布尔交", {58, 0})) app.booleanOp(2);
        ImGui::Separator();
        toolBtn(app, Tool::PushPull, "拖面", "同步建模: 拖动面 P");
        toolBtn(app, Tool::Measure, "测量", "测量 M");
    }
    End();
    ImGui::PopStyleColor();
}

// ---------------- 特征树 ----------------
static void drawTree(Application& app) {
    if (!Begin("##tree", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        End();
        return;
    }
    ImGui::TextUnformatted("特征树");
    ImGui::Separator();

    for (auto& b : app.doc.bodies) {
        ImGui::PushID((int)b.id);
        bool open = ImGui::CollapsingHeader(b.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        SameLine();
        ImGui::Checkbox("##vis", &b.visible);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("显示/隐藏");
        if (open) {
            for (auto it = b.features.rbegin(); it != b.features.rend(); ++it) {
                Feature& f = *it;
                ImGui::PushID((int)f.id);
                ImGui::Bullet();
                bool sel = app.selectedFeatureId == f.id;
                if (!f.error.empty()) ImGui::PushStyleColor(ImGuiCol_Text, {0.95f, 0.45f, 0.35f, 1});
                if (ImGui::Selectable((f.displayLabel() + (f.active ? "" : " (已抑制)")).c_str(), &sel)) {
                    app.selectedFeatureId = sel ? f.id : kInvalidId;
                }
                if (!f.error.empty()) ImGui::PopStyleColor();
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("抑制/恢复")) f.active = !f.active;
                    if (ImGui::MenuItem("删除特征")) app.deleteFeature(f.id);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextUnformatted("草图");
    for (auto& s : app.doc.sketches) {
        ImGui::PushID((int)s.id);
        ImGui::Bullet();
        char label[128];
        snprintf(label, sizeof(label), "%s (%d 图元/%d 约束)", s.name.c_str(), (int)(s.lines.size() + s.circles.size() + s.arcs.size()), (int)s.constraints.size());
        bool sel = app.activeSketchId == s.id;
        if (ImGui::Selectable(label, &sel)) {
            if (sel) app.enterSketch(s.id);
        }
        SameLine();
        ImGui::Checkbox("##vis", &s.visible);
        ImGui::PopID();
    }
    if (Button("+ 新建草图 (XY)")) app.startNewSketch(planeXY());
    SameLine();
    if (Button("+ XZ")) app.startNewSketch(planeXZ());
    SameLine();
    if (Button("+ YZ")) app.startNewSketch(planeYZ());
    End();
}

// ---------------- 属性面板 ----------------
static void drawProperties(Application& app) {
    if (!Begin("##props", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        End();
        return;
    }
    ImGui::TextUnformatted("属性");
    ImGui::Separator();

    // ---- 工具参数 ----
    if (app.mode == Mode::Object && app.tool == Tool::Extrude) {
        ImGui::TextUnformatted("拉伸");
        ImGui::SetNextItemWidth(-60);
        ImGui::InputDouble("距离 (mm)", &app.extrudeInput, 1, 10, "%.2f");
        ImGui::Checkbox("反转方向", &app.axisFlip);
        ImGui::Checkbox("切除模式", &app.axisCut);
        if (Button("应用拉伸", {-60, 0})) {
            bool fromSketch = false;
            SketchDef* sk = app.doc.sketch(app.activeSketchId);
            if (sk && !sk->isEmpty()) {
                TopoDS_Shape faces;
                if (sk->buildProfileFaces(faces).empty()) fromSketch = true;
            }
            if (fromSketch) {
                Feature f;
                f.type = FeatureType::Extrude;
                f.sketchId = app.activeSketchId;
                f.p1 = app.extrudeInput;
                f.opMode = app.axisCut ? 1 : 0;
                f.flip = app.axisFlip;
                Id bodyId = app.selFaces.empty() ? kInvalidId : app.selFaces[0].bodyId;
                app.commitFeature(f, bodyId);
                app.showToast("拉伸完成");
            } else if (!app.selFaces.empty()) {
                // 面直接拉伸
                TopoDS_Face face = TopoDS::Face(app.selFaces[0].shape);
                BRepAdaptor_Surface surf(face, Standard_False);
                gp_Vec dir{0, 0, 1};
                if (surf.GetType() == GeomAbs_Plane) dir = gp_Vec(surf.Plane().Axis().Direction());
                if (app.axisFlip) dir.Reverse();
                TopoDS_Shape prism = BRepPrimAPI_MakePrism(face, dir * app.extrudeInput, Standard_False, Standard_True).Shape();
                app.pushUndo();
                Feature fb;
                fb.id = app.doc.newId();
                fb.type = FeatureType::Imported;
                fb.name = "面拉伸";
                Body* b = app.doc.body(app.selFaces[0].bodyId);
                try {
                    if (b && !b->result.IsNull()) {
                        BRepAlgoAPI_Fuse fu(b->result, prism);
                        fb.result = fu.Shape();
                    } else fb.result = prism;
                    b->features.push_back(fb);
                    app.recompute();
                    app.showToast("面拉伸完成");
                } catch (Standard_Failure& e) {
                    app.showToast(e.GetMessageString() ? e.GetMessageString() : "失败", true);
                }
            } else {
                app.showToast("请选择面或使用含封闭轮廓的草图", true);
            }
        }
        ImGui::Separator();
    }
    if (app.mode == Mode::Object && app.tool == Tool::Revolve) {
        ImGui::TextUnformatted("旋转");
        ImGui::SetNextItemWidth(-60);
        ImGui::InputDouble("角度 (°)", &app.revolveAngle, 5, 45, "%.1f");
        if (Button("应用旋转", {-60, 0})) {
            SketchDef* sk = app.doc.sketch(app.activeSketchId);
            if (sk) {
                Feature f;
                f.type = FeatureType::Revolve;
                f.sketchId = app.activeSketchId;
                f.p1 = app.revolveAngle;
                f.opMode = 0;
                // 轴: 选第一条构造线或草图 X 轴
                for (auto& l : sk->lines)
                    if (l.construction) { f.axisLineId = l.id; break; }
                app.commitFeature(f);
            } else
                app.showToast("请先选择草图", true);
        }
        ImGui::Separator();
    }
    if ((app.tool == Tool::Fillet || app.tool == Tool::Chamfer) && app.mode == Mode::Object) {
        bool fil = app.tool == Tool::Fillet;
        ImGui::TextUnformatted(fil ? "圆角" : "倒角");
        ImGui::Text("已选棱: %d", (int)app.selEdges.size());
        ImGui::SetNextItemWidth(-60);
        if (fil) ImGui::InputDouble("半径 (mm)", &app.filletInput, 0.5, 5, "%.2f");
        else ImGui::InputDouble("距离 (mm)", &app.chamferInput, 0.5, 5, "%.2f");
        if (Button(fil ? "应用圆角" : "应用倒角", {-60, 0})) {
            if (app.selEdges.empty()) app.showToast("请先选择棱", true);
            else {
                Feature f;
                f.type = fil ? FeatureType::Fillet : FeatureType::Chamfer;
                f.p1 = fil ? app.filletInput : app.chamferInput;
                for (auto& e : app.selEdges) f.edgeAnchors.push_back({e.anchor.x, e.anchor.y, e.anchor.z});
                Id bodyId = app.selEdges[0].bodyId;
                app.commitFeature(f, bodyId);
                app.selEdges.clear();
            }
        }
        if (Button("清空棱选择", {-60, 0})) app.selEdges.clear();
        ImGui::Separator();
    }
    if (app.tool == Tool::Shell && app.mode == Mode::Object) {
        ImGui::TextUnformatted("抽壳");
        ImGui::Text("开口面: %d", (int)app.selFaces.size());
        ImGui::SetNextItemWidth(-60);
        ImGui::InputDouble("壁厚 (mm)", &app.shellInput, 0.5, 5, "%.2f");
        if (Button("应用抽壳", {-60, 0})) {
            if (app.selFaces.empty()) app.showToast("请选择要移除的面", true);
            else {
                Feature f;
                f.type = FeatureType::Shell;
                f.p1 = app.shellInput;
                for (auto& s : app.selFaces) f.faceAnchors.push_back({s.anchor.x, s.anchor.y, s.anchor.z});
                app.commitFeature(f, app.selFaces[0].bodyId);
                app.selFaces.clear();
            }
        }
        ImGui::Separator();
    }

    // ---- 测量结果 ----
    if (app.hasMeasureA) {
        ImGui::TextUnformatted("测量");
        ImGui::Text("A: %s (%.1f, %.1f, %.1f)", app.measureA.label.c_str(), app.measureA.anchor.x,
                    app.measureA.anchor.y, app.measureA.anchor.z);
        if (app.hasMeasureB) {
            ImGui::Text("B: %s (%.1f, %.1f, %.1f)", app.measureB.label.c_str(), app.measureB.anchor.x,
                        app.measureB.anchor.y, app.measureB.anchor.z);
            ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.85f, 0.5f, 1});
            ImGui::Text("%s: %s", app.measureRes.kind.c_str(), app.measureRes.text.c_str());
            ImGui::PopStyleColor();
        } else
            ImGui::TextDisabled("点击第二个图元...");
        ImGui::Separator();
    }

    // ---- 选中特征参数 ----
    if (app.selectedFeatureId) {
        Feature* f = app.doc.feature(app.selectedFeatureId);
        if (f) {
            ImGui::Text("特征: %s", f->displayLabel().c_str());
            ImGui::Checkbox("启用", &f->active);
            bool changed = false;
            auto paramInput = [&](const char* name, double* v) {
                ImGui::SetNextItemWidth(-60);
                if (ImGui::InputDouble(name, v, 0.5, 5, "%.3f")) changed = true;
            };
            switch (f->type) {
                case FeatureType::Box:
                    paramInput("长", &f->p1);
                    paramInput("宽", &f->p2);
                    paramInput("高", &f->p3);
                    break;
                case FeatureType::Cylinder:
                    paramInput("半径", &f->p1);
                    paramInput("高", &f->p2);
                    break;
                case FeatureType::Sphere: paramInput("半径", &f->p1); break;
                case FeatureType::Torus:
                    paramInput("主半径", &f->p1);
                    paramInput("管半径", &f->p2);
                    break;
                case FeatureType::Cone:
                    paramInput("下半径", &f->p1);
                    paramInput("上半径", &f->p2);
                    paramInput("高", &f->p3);
                    break;
                case FeatureType::Extrude:
                case FeatureType::Revolve:
                    paramInput(f->type == FeatureType::Extrude ? "距离" : "角度", &f->p1);
                    ImGui::Checkbox("切除", (bool*)&f->opMode);
                    ImGui::Checkbox("反转", &f->flip);
                    break;
                case FeatureType::Fillet: paramInput("半径", &f->p1); break;
                case FeatureType::Chamfer: paramInput("距离", &f->p1); break;
                case FeatureType::Shell: paramInput("壁厚", &f->p1); break;
                default: ImGui::TextDisabled("(该特征无参数)"); break;
            }
            if (Button("应用参数修改")) {
                app.pushUndo();
                app.recompute();
                app.showToast("特征已更新");
            }
            ImGui::Separator();
        }
    }

    // ---- 实体质量属性 ----
    Id bodyId = kInvalidId;
    if (!app.selFaces.empty()) bodyId = app.selFaces[0].bodyId;
    else if (!app.selBodies.empty()) bodyId = app.selBodies[0].bodyId;
    else if (!app.doc.bodies.empty()) bodyId = app.doc.bodies[0].id;
    if (Body* b = app.doc.body(bodyId)) {
        ImGui::Text("实体: %s", b->name.c_str());
        if (!b->result.IsNull()) {
            MassProps mp = massProperties(b->result, b->material.density);
            ImGui::PushStyleColor(ImGuiCol_Text, {0.62f, 0.72f, 0.9f, 1});
            ImGui::TextUnformatted(mp.text().c_str());
            ImGui::PopStyleColor();
            double x0, y0, z0, x1, y1, z1;
            mp.bbox.Get(x0, y0, z0, x1, y1, z1);
            ImGui::Text("包围盒: %.2f x %.2f x %.2f mm", x1 - x0, y1 - y0, z1 - z0);
        }
    }
    End();
}

// ---------------- 草图约束面板 ----------------
static void drawSketchPanel(Application& app) {
    if (!Begin("##sketchPanel", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        End();
        return;
    }
    SketchDef* sk = app.doc.sketch(app.activeSketchId);
    if (!sk) {
        Text("无活动草图");
        End();
        return;
    }
    ImGui::Text("草图: %s", sk->name.c_str());
    const char* st = "";
    ImVec4 col = {1, 1, 1, 1};
    switch (app.lastSolve.status) {
        case SolveStatus::Ok: st = "完全约束"; col = {0.4f, 0.9f, 0.45f, 1}; break;
        case SolveStatus::UnderConstrained: st = "欠约束"; col = {0.95f, 0.75f, 0.3f, 1}; break;
        case SolveStatus::OverConstrained: st = "过约束!"; col = {0.95f, 0.4f, 0.3f, 1}; break;
        case SolveStatus::Failed: st = "求解失败"; col = {0.95f, 0.4f, 0.3f, 1}; break;
        default: st = "-"; break;
    }
    ImGui::TextColored(col, "状态: %s (自由度 %d)", st, app.lastSolve.dof);

    // 实体尺寸编辑
    if (ImGui::CollapsingHeader("图元尺寸", ImGuiTreeNodeFlags_DefaultOpen)) {
        int li = 0;
        for (auto& l : sk->lines) {
            if (l.construction) continue;
            li++;
            ImGui::PushID((int)l.id);
            double len = dist(sk->pointPos(l.p1), sk->pointPos(l.p2));
            ImGui::Text("线%d", li);
            SameLine();
            ImGui::SetNextItemWidth(-70);
            if (ImGui::InputDouble("##len", &len, 0, 0, "%.2f")) {
                // 加/改长度约束
                bool found = false;
                for (auto& c : sk->constraints)
                    if (c.type == CstType::Length && c.refs.size() == 1 && c.refs[0] == l.id) {
                        c.value = len;
                        found = true;
                    }
                if (!found) {
                    Constraint c;
                    c.id = app.doc.newId();
                    c.type = CstType::Length;
                    c.refs = {l.id};
                    c.value = len;
                    sk->constraints.push_back(c);
                }
                app.solveSketch();
            }
            ImGui::PopID();
        }
        for (auto& c : sk->circles) {
            ImGui::PushID((int)c.id);
            double r = c.r;
            ImGui::Text("圆");
            SameLine();
            ImGui::SetNextItemWidth(-70);
            if (ImGui::InputDouble("半径##r", &r, 0, 0, "%.2f")) {
                for (auto& cc : sk->constraints)
                    if (cc.type == CstType::Radius && cc.refs.size() == 1 && cc.refs[0] == c.id) cc.value = r;
                // 无则添加
                bool found = false;
                for (auto& cc : sk->constraints)
                    if (cc.type == CstType::Radius && cc.refs.size() == 1 && cc.refs[0] == c.id) found = true;
                if (!found) {
                    Constraint cc;
                    cc.id = app.doc.newId();
                    cc.type = CstType::Radius;
                    cc.refs = {c.id};
                    cc.value = r;
                    sk->constraints.push_back(cc);
                }
                app.solveSketch();
            }
            ImGui::PopID();
        }
    }

    // 约束添加
    if (ImGui::CollapsingHeader("添加约束", ImGuiTreeNodeFlags_DefaultOpen)) {
        static int refA = 0, refB = 0;
        static double cstVal = 10.0;
        std::vector<std::string> names;
        std::vector<std::vector<Id>> refIds;
        for (auto& l : sk->lines) {
            names.push_back("线");
            refIds.push_back({l.id});
        }
        for (auto& c : sk->circles) {
            names.push_back("圆");
            refIds.push_back({c.id});
        }
        for (auto& a : sk->arcs) {
            names.push_back("弧");
            refIds.push_back({a.id});
        }
        for (auto& p : sk->points) {
            names.push_back("点");
            refIds.push_back({p.id});
        }
        auto combo = [&](const char* label, int* idx) {
            if (names.empty()) return;
            if (*idx >= (int)names.size()) *idx = 0;
            ImGui::SetNextItemWidth(90);
            if (ImGui::BeginCombo(label, (names[*idx] + std::to_string(*idx + 1)).c_str())) {
                for (int i = 0; i < (int)names.size(); ++i)
                    if (ImGui::Selectable((names[i] + std::to_string(i + 1)).c_str(), i == *idx)) *idx = i;
                ImGui::EndCombo();
            }
        };
        combo("A", &refA);
        combo("B", &refB);
        ImGui::SetNextItemWidth(90);
        ImGui::InputDouble("值", &cstVal, 1, 10, "%.2f");
        auto addC = [&](CstType t, double v = 0) {
            if (refA >= (int)refIds.size() || refB >= (int)refIds.size()) return;
            Constraint c;
            c.id = app.doc.newId();
            c.type = t;
            c.refs = {refIds[refA][0], refIds[refB][0]};
            c.value = v;
            sk->constraints.push_back(c);
            app.solveSketch();
        };
        if (Button("重合", {72, 0})) addC(CstType::Coincident);
        SameLine();
        if (Button("水平", {72, 0})) {
            if (refA < (int)refIds.size()) {
                Constraint c;
                c.id = app.doc.newId();
                c.type = CstType::Horizontal;
                c.refs = {refIds[refA][0]};
                sk->constraints.push_back(c);
                app.solveSketch();
            }
        }
        if (Button("垂直", {72, 0})) {
            if (refA < (int)refIds.size()) {
                Constraint c;
                c.id = app.doc.newId();
                c.type = CstType::Vertical;
                c.refs = {refIds[refA][0]};
                sk->constraints.push_back(c);
                app.solveSketch();
            }
        }
        SameLine();
        if (Button("平行", {72, 0})) addC(CstType::Parallel);
        if (Button("垂直线", {72, 0})) addC(CstType::Perpendicular);
        SameLine();
        if (Button("相等", {72, 0})) addC(CstType::Equal);
        if (Button("距离", {72, 0})) addC(CstType::Distance, cstVal);
        SameLine();
        if (Button("角度", {72, 0})) addC(CstType::Angle, cstVal * M_PI / 180.0);
        if (Button("中点", {72, 0})) addC(CstType::Midpoint);
        SameLine();
        if (Button("同心", {72, 0})) addC(CstType::Concentric);
        if (Button("固定", {72, 0})) {
            if (refA < (int)refIds.size()) {
                Vec2 p = sk->pointPos(refIds[refA][0]);
                Constraint c;
                c.id = app.doc.newId();
                c.type = CstType::Fix;
                c.refs = {refIds[refA][0]};
                c.value = p.x;
                c.value2 = p.y;
                sk->constraints.push_back(c);
                app.solveSketch();
            }
        }
    }

    // 约束列表
    if (ImGui::CollapsingHeader("约束列表", ImGuiTreeNodeFlags_DefaultOpen)) {
        int i = 0;
        for (auto it = sk->constraints.begin(); it != sk->constraints.end();) {
            ImGui::PushID(i);
            ImGui::Text("%d. %s", ++i, cstName(it->type));
            if (it->value != 0 && it->type != CstType::Fix) {
                SameLine(150);
                ImGui::Text("%.2f", it->type == CstType::Angle ? it->value * 180 / M_PI : it->value);
            }
            SameLine(ImGui::GetWindowWidth() - 60);
            if (Button("x")) {
                it = sk->constraints.erase(it);
                app.solveSketch();
                ImGui::PopID();
                continue;
            }
            ImGui::PopID();
            ++it;
        }
    }
    End();
}

// ---------------- 材质面板 ----------------
static void drawMaterialPanel(Application& app) {
    if (!Begin("##material", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        End();
        return;
    }
    ImGui::TextUnformatted("材质 / 渲染");
    ImGui::Separator();
    Id bodyId = app.selFaces.empty() ? (app.doc.bodies.empty() ? 0 : app.doc.bodies[0].id) : app.selFaces[0].bodyId;
    Body* b = app.doc.body(bodyId);
    if (!b) {
        Text("无实体");
        End();
        return;
    }
    static const struct {
        const char* name;
        float color[3];
        float metal, rough;
        double density;
    } presets[] = {
        {"铝合金", {0.72f, 0.74f, 0.78f}, 1.0f, 0.35f, 2.70},
        {"不锈钢", {0.78f, 0.79f, 0.80f}, 1.0f, 0.22f, 7.85},
        {"铜", {0.86f, 0.55f, 0.32f}, 1.0f, 0.30f, 8.96},
        {"钛合金", {0.68f, 0.66f, 0.65f}, 1.0f, 0.42f, 4.51},
        {"ABS 塑料", {0.85f, 0.85f, 0.84f}, 0.0f, 0.55f, 1.05},
        {"碳纤维", {0.18f, 0.19f, 0.22f}, 0.2f, 0.38f, 1.60},
        {"橙色工程塑料", {0.90f, 0.45f, 0.10f}, 0.0f, 0.50f, 1.20},
    };
    int pi = 0;
    for (auto& p : presets) {
        ImGui::PushID(pi);
        ImGui::ColorButton("##sw", {p.color[0], p.color[1], p.color[2], 1}, ImGuiColorEditFlags_None, {26, 26});
        SameLine();
        if (Button(p.name, {110, 0})) {
            b->material.name = p.name;
            memcpy(b->material.color, p.color, sizeof(p.color));
            b->material.metallic = p.metal;
            b->material.roughness = p.rough;
            b->material.density = p.density;
        }
        ImGui::PopID();
        pi++;
    }
    ImGui::Separator();
    ImGui::ColorEdit3("颜色", b->material.color);
    ImGui::SliderFloat("金属度", &b->material.metallic, 0, 1);
    ImGui::SliderFloat("粗糙度", &b->material.roughness, 0.03f, 1);
    ImGui::InputDouble("密度 g/cm³", &b->material.density);
    End();
}

// ---------------- 状态栏 & 提示 ----------------
static void drawStatusBar(Application& app) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, io.DisplaySize.y - 26});
    ImGui::SetNextWindowSize({io.DisplaySize.x, 26});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.11f, 0.12f, 0.14f, 1});
    if (!Begin("##status", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        End();
        ImGui::PopStyleColor();
        return;
    }
    ImGui::TextUnformatted(app.statusHint());
    SameLine(io.DisplaySize.x - 560);
    ImGui::TextDisabled("撤销 %d/重做 %d | 线程 x%d | %.0f FPS", (int)app.undoStack.size(),
                        (int)app.redoStack.size(), (int)ThreadPool::hardwareThreads(), io.Framerate);
    End();
    ImGui::PopStyleColor();

    // Toast
    if (!app.toast.empty() && Stopwatch::nowMs() < app.toastUntil) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 ts = ImGui::CalcTextSize(app.toast.c_str());
        float w = io.DisplaySize.x, y = io.DisplaySize.y - 64;
        dl->AddRectFilled({w / 2 - ts.x / 2 - 16, y - 8}, {w / 2 + ts.x / 2 + 16, y + ts.y + 8},
                          IM_COL32(30, 34, 42, 235), 8);
        dl->AddText({w / 2 - ts.x / 2, y}, IM_COL32(235, 238, 245, 255), app.toast.c_str());
    }
}

void drawAll(Application& app) {
    initTheme();
    drawTopBar(app);
    if (app.mode != Mode::Drawing) {
        drawToolbar(app);
        // 右侧面板
        ImGuiIO& io = ImGui::GetIO();
        float rw = 320;
        ImGui::SetNextWindowPos({io.DisplaySize.x - rw, 62});
        ImGui::SetNextWindowSize({rw, io.DisplaySize.y - 62 - 26});
        if (ImGui::BeginTabBar("##rightTabs")) {
            // 用子窗口容纳
            ImGui::EndTabBar();
        }
        static ImGuiID dockId = ImGui::GetID("##rightDock");
        (void)dockId;
        // 简化: 用标签页风格手绘
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.15f, 0.16f, 0.18f, 1});
        ImGui::SetNextWindowPos({io.DisplaySize.x - rw, 62});
        ImGui::SetNextWindowSize({rw, io.DisplaySize.y - 62 - 26});
        if (!Begin("##rightRoot", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            End();
            ImGui::PopStyleColor();
        } else {
            if (ImGui::BeginTabBar("##rt")) {
                if (ImGui::BeginTabItem("特征树")) {
                    ImGui::BeginChild("##tc", {0, 0}, ImGuiChildFlags_None);
                    drawTree(app);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(app.mode == Mode::Sketch ? "约束" : "属性")) {
                    ImGui::BeginChild("##pc", {0, 0}, ImGuiChildFlags_None);
                    if (app.mode == Mode::Sketch) drawSketchPanel(app);
                    else drawProperties(app);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("材质")) {
                    ImGui::BeginChild("##mc", {0, 0}, ImGuiChildFlags_None);
                    drawMaterialPanel(app);
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            End();
            ImGui::PopStyleColor();
        }
        drawStatusBar(app);
    } else {
        // 制图模式顶栏
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos({io.DisplaySize.x - 380, 70});
        ImGui::SetNextWindowSize({360, 240});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.15f, 0.16f, 0.18f, 0.96f});
        if (!Begin("##dwgPanel", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoSavedSettings)) {
            End();
        } else {
            ImGui::TextUnformatted("工程图");
            if (Button("生成四视图 (HLR)", {-60, 0})) app.generateDrawing();
            ImGui::Separator();
            static char partName[128] = "";
            if (partName[0] == 0) snprintf(partName, sizeof(partName), "%s", app.drawing.partName.c_str());
            ImGui::InputText("图名", partName, sizeof(partName));
            app.drawing.partName = partName;
            static char material[128] = "";
            if (material[0] == 0) snprintf(material, sizeof(material), "%s", app.drawing.material.c_str());
            ImGui::InputText("材料", material, sizeof(material));
            app.drawing.material = material;
            ImGui::Separator();
            if (Button("导出 SVG")) app.exportDrawing(app.drawing.partName + ".svg", "svg");
            SameLine();
            if (Button("导出 DXF")) app.exportDrawing(app.drawing.partName + ".dxf", "dxf");
            SameLine();
            if (Button("导出 PDF")) app.exportDrawing(app.drawing.partName + ".pdf", "pdf");
            ImGui::Separator();
            ImGui::TextDisabled("2D 制图工具在左侧工具条; 滚轮缩放, 中键平移");
            char buf[64];
            snprintf(buf, sizeof(buf), "缩放 %.0f%%", app.drawZoom * 100);
            ImGui::TextUnformatted(buf);
            End();
        }
        ImGui::PopStyleColor();
        drawStatusBar(app);
    }
}

// ---------------- 工程图画布 ----------------
static ImU32 sheetCol(int v) {
    return IM_COL32(v, v, v, 255);
}

void drawDrawingCanvas(Application& app, ImDrawList* dl, float w, float h) {
    (void)w;
    (void)h;
    // 画布变换: 图纸 mm -> 屏幕
    auto toScreen = [&](Vec2 p) -> ImVec2 {
        return {app.drawPan.x + p.x * app.drawZoom, app.drawPan.y + (app.drawing.sheetH - p.y) * app.drawZoom};
    };
    float sheetW = app.drawing.sheetW * app.drawZoom;
    float sheetH = app.drawing.sheetH * app.drawZoom;
    ImVec2 o{app.drawPan.x, app.drawPan.y};

    // 纸张阴影 + 白纸
    dl->AddRectFilled({o.x + 6, o.y + 8}, {o.x + sheetW + 6, o.y + sheetH + 8}, IM_COL32(0, 0, 0, 70), 2);
    dl->AddRectFilled(o, {o.x + sheetW, o.y + sheetH}, IM_COL32(250, 250, 250, 255), 2);
    dl->AddRect(o, {o.x + sheetW, o.y + sheetH}, IM_COL32(60, 60, 60, 255));

    if (!app.drawingValid) {
        dl->AddText({o.x + 30, o.y + 40}, IM_COL32(120, 120, 120, 255), "点击右侧[生成四视图 (HLR)]由 3D 模型创建工程图");
        return;
    }

    // 图框
    Vec2 b0{10, 10}, b1{app.drawing.sheetW - 10, app.drawing.sheetH - 10};
    dl->AddRect(toScreen(b0), toScreen(b1), sheetCol(40), 0, 0, 1.2f);
    // 标题栏
    double tbX = app.drawing.sheetW - 130, tbY = 10, tbW = 120, tbH = 40;
    dl->AddRect(toScreen({tbX, tbY}), toScreen({tbX + tbW, tbY + tbH}), sheetCol(40));
    dl->AddLine(toScreen({tbX, tbY + 14}), toScreen({tbX + tbW, tbY + 14}), sheetCol(40));

    auto strokePolys = [&](const std::vector<DrawPoly>& ps) {
        for (auto& p : ps) {
            if (p.pts.empty()) continue;
            ImU32 col = p.hidden ? IM_COL32(120, 128, 140, 255) : IM_COL32(25, 34, 48, 255);
            if (p.hidden) {
                // 虚线
                for (size_t i = 1; i < p.pts.size(); ++i) {
                    ImVec2 a = toScreen(p.pts[i - 1]), b = toScreen(p.pts[i]);
                    float len = std::hypot(b.x - a.x, b.y - a.y);
                    int segs = std::max(1, (int)(len / 6));
                    for (int s2 = 0; s2 < segs; s2 += 2) {
                        float t0 = (float)s2 / segs, t1 = std::min(1.f, (float)(s2 + 1) / segs);
                        dl->AddLine({a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0},
                                    {a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1}, col, 1.0f);
                    }
                }
            } else {
                for (size_t i = 1; i < p.pts.size(); ++i)
                    dl->AddLine(toScreen(p.pts[i - 1]), toScreen(p.pts[i]), col, 1.2f);
            }
        }
    };
    for (auto& v : app.drawing.views) strokePolys(v.polies);
    strokePolys(app.drawing.drafts);
    for (auto& c : app.drawing.circles) {
        ImVec2 ctr = toScreen(c.center);
        dl->AddCircle(ctr, c.r * app.drawZoom, IM_COL32(25, 34, 48, 255), 48, 1.2f);
    }
    for (auto& t : app.drawing.texts) {
        ImVec2 p = toScreen(t.pos);
        float fs = std::max(9.f, (float)(t.height * app.drawZoom * 1.6f));
        ImFont* font = ImGui::GetFont();
        if (t.align == 1) {
            ImVec2 sz = ImGui::CalcTextSize(t.text.c_str());
            p.x -= sz.x * fs / ImGui::GetFontSize() / 2;
        }
        dl->AddText(font, fs, p, IM_COL32(25, 34, 48, 255), t.text.c_str());
    }

    // 2D 制图交互(视口坐标 -> 图纸坐标)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    ImVec2 mp = ImGui::GetMousePos();
    Vec2 sheetPt{(mp.x - app.drawPan.x) / app.drawZoom,
                 app.drawing.sheetH - (mp.y - app.drawPan.y) / app.drawZoom};
    app.drawCursor = sheetPt;
    static std::vector<Vec2> draftPts;
    bool clicked = ImGui::IsMouseClicked(0);

    if (app.tool == Tool::DraftLine && clicked && sheetPt.x > 0 && sheetPt.x < app.drawing.sheetW) {
        draftPts.push_back(sheetPt);
        if (draftPts.size() == 2) {
            DrawPoly p;
            p.pts = draftPts;
            app.drawing.drafts.push_back(p);
            draftPts.clear();
        }
    }
    if (app.tool == Tool::DraftCircle && clicked) {
        draftPts.push_back(sheetPt);
        if (draftPts.size() == 2) {
            DrawCircle c;
            c.center = draftPts[0];
            c.r = dist(draftPts[0], draftPts[1]);
            app.drawing.circles.push_back(c);
            draftPts.clear();
        }
    }
    if (app.tool == Tool::DraftDim && clicked) {
        draftPts.push_back(sheetPt);
        if (draftPts.size() == 2) {
            DrawDim d;
            d.a = draftPts[0];
            d.b = draftPts[1];
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f", dist(d.a, d.b));
            DrawText t;
            t.pos = {(d.a.x + d.b.x) / 2, (d.a.y + d.b.y) / 2 + 6};
            t.text = buf;
            t.align = 1;
            app.drawing.texts.push_back(t);
            DrawPoly p;
            p.pts = draftPts;
            app.drawing.drafts.push_back(p);
            draftPts.clear();
        }
    }
    if (app.tool == Tool::DraftText && clicked) {
        DrawText t;
        t.pos = sheetPt;
        t.text = "注记";
        t.height = 3.5;
        app.drawing.texts.push_back(t);
    }
    // 橡皮筋
    if (draftPts.size() == 1) {
        ImVec2 a = toScreen(draftPts[0]);
        dl->AddLine(a, mp, IM_COL32(220, 90, 30, 255), 1.2f);
    }
    // 光标十字
    dl->AddLine({mp.x - 10, mp.y}, {mp.x + 10, mp.y}, IM_COL32(30, 120, 230, 180));
    dl->AddLine({mp.x, mp.y - 10}, {mp.x, mp.y + 10}, IM_COL32(30, 120, 230, 180));

    // 平移(中键)
    static ImVec2 panStart{0, 0};
    static bool panning = false;
    if (ImGui::IsMouseClicked(2)) {
        panning = true;
        panStart = mp;
    }
    if (ImGui::IsMouseReleased(2)) panning = false;
    if (panning) {
        app.drawPan.x += mp.x - panStart.x;
        app.drawPan.y += mp.y - panStart.y;
        panStart = mp;
    }
}

} // namespace cad::ui
