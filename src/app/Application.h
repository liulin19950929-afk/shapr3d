// Application.h —— 应用状态机: 建模/草图/制图 三大模式与交互工具
#pragma once
#include "../kernel/Document.h"
#include "../render/Renderer.h"
#include "../sketch/Solver.h"
#include "../drawing/Drawing.h"
#include "../analysis/Measure.h"
#include <GLFW/glfw3.h>

namespace cad {

enum class Mode { Object, Sketch, Drawing };

enum class Tool {
    Select,
    SketchLine, SketchRect, SketchCircle, SketchArc, SketchPolygon,
    Extrude, Revolve, Fillet, Chamfer, Shell,
    BoolUnion, BoolCut, BoolInter, PushPull, MoveBody, Measure,
    DraftLine, DraftCircle, DraftDim, DraftText // 制图模式
};

// 拾取引用(面/棱/顶点/实体)
struct SelRef {
    Id bodyId = kInvalidId;
    int kind = 0;          // 0=面 1=棱 2=顶点 3=体
    TopoDS_Shape shape;
    Vec3 anchor;           // 世界锚点
    std::string label;
};

struct PickHit {
    bool hit = false;
    Id bodyId = kInvalidId;
    Vec3 point;
    double dist = 0;
    int triIdx = -1;
    int faceIdx = -1;
};

class Application {
public:
    Document doc;
    Renderer renderer;
    RenderSettings rsettings;
    Camera cam;

    Mode mode = Mode::Object;
    Tool tool = Tool::Select;
    Tool prevTool = Tool::Select;

    // 选择集
    std::vector<SelRef> selFaces, selEdges, selVertices, selBodies;
    Id selectedFeatureId = kInvalidId;
    Id activeSketchId = kInvalidId;

    // 草图交互状态
    std::vector<Vec2> pendingPts;      // 当前工具已放置点(2D)
    Id dragPointId = kInvalidId;
    bool dragging = false;
    Vec2 cursorPlane;                  // 光标在草图平面上的 2D 坐标
    bool gridSnap = true;
    std::vector<std::pair<Vec3, Vec3>> rubberLines;  // 橡皮筋
    std::vector<Vec3> snapMarkers;
    int polygonSides = 6;
    SolveResult lastSolve;

    // 拉伸/抽拉交互
    bool axisDragging = false;
    double axisValue = 10.0;
    Vec3 axisOrigin, axisDir;
    Id axisSketchId = kInvalidId;
    bool axisCut = false;
    bool axisFlip = false;
    double revolveAngle = 360.0;

    // 同步建模(拖面)
    struct PullState {
        bool active = false;
        Id bodyId = kInvalidId;
        TopoDS_Face face;
        gp_Pln plane;
        Vec3 startPoint;
        double value = 0;
        bool inward = false;
    } pull;

    // 测量
    SelRef measureA, measureB;
    bool hasMeasureA = false, hasMeasureB = false;
    MeasureResult measureRes;

    // 工程图
    Drawing drawing;
    bool drawingValid = false;
    Vec2 drawPan{40, 40};
    float drawZoom = 1.0f;
    Vec2 drawCursor;

    // 参数输入缓冲
    double extrudeInput = 10.0;
    double filletInput = 2.0;
    double chamferInput = 1.0;
    double shellInput = 1.0;
    char textInput[256] = {0};

    // 撤销
    std::vector<std::string> undoStack, redoStack;
    size_t undoLimit = 64;

    // 状态提示
    std::string toast;
    double toastUntil = 0;

    // 运行
    int run();

    // ---- 供 UI 调用 ----
    void showToast(const std::string& msg, bool error = false);
    void pushUndo();
    void doUndo();
    void doRedo();
    void recompute();
    void clearSelection();

    // 特征操作
    Id commitFeature(Feature f, Id bodyId = kInvalidId);   // 追加特征并重算
    void deleteFeature(Id fid);
    void booleanOp(int op); // 0并 1差 2交 (基于 selBodies 去重)

    // 草图操作
    void enterSketch(Id sketchId);
    void startNewSketch(const gp_Pln& pln);
    void exitSketch();
    Id addSketchLine(Vec2 a, Vec2 b, bool construction = false, Id* outP1 = nullptr, Id* outP2 = nullptr);
    Id addSketchCircle(Vec2 c, double r);
    void solveSketch();
    Id hitSketchPoint(Vec2 p, double tolPx); // 屏幕距离拾取
    void addConstraintUI(CstType t, double value = 0); // 基于当前选择(草图实体)

    // 制图
    void generateDrawing();
    void exportDrawing(const std::string& path, const std::string& kind);
    void exportModel(const std::string& path);
    void importModel(const std::string& path);
    void screenshot(const std::string& path);
    void frameAll();
    void alignView(int v); // 0前 1后 2左 3右 4上 5下 6轴测

    // 拾取
    PickHit pickBody(float sx, float sy);
    bool pickEdge(float sx, float sy, SelRef& out);
    bool pickVertex(float sx, float sy, SelRef& out);
    bool pickPlanarFace(float sx, float sy, SelRef& out, gp_Pln* plnOut);
    Vec3 worldAt(float sx, float sy, double z = 0);   // 射线与 z=0 平面交点
    Vec3 rayOnPlane(float sx, float sy, const gp_Pln& pln, bool* ok = nullptr);

    // 每帧交互处理(点击/拖拽/快捷键)
    void handleFrameInput(GLFWwindow* win, float mx, float my, bool lmb, bool lmbPrev, bool overUi);
    Vec2 snapSketchPoint(const gp_Pln& pln, float mx, float my);

    // 视口信息
    float viewW() const { return (float)renderer.width(); }
    float viewH() const { return (float)renderer.height(); }
    Vec3 projectPoint(const Vec3& p) const { return renderer.project(p, cam, viewW(), viewH()); }

    const char* statusHint() const;
};

} // namespace cad
