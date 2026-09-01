// Document.cpp —— 重算调度 / 序列化 / 欢迎文档
#include "Document.h"
#include "MeshBuilder.h"
#include "../core/Json.h"
#include "../core/ThreadPool.h"
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <sstream>
#include <unordered_map>

namespace cad {

using namespace cad::json;
namespace { using V = cad::json::Value;
inline cad::json::ValuePtr N(double d) { return V::mkNum(d); }
inline cad::json::ValuePtr S(const std::string& x) { return V::mkStr(x); }
inline cad::json::ValuePtr B(bool b) { return V::mkBool(b); }
inline cad::json::ValuePtr A() { return V::mkArr(); }
inline cad::json::ValuePtr O() { return V::mkObj(); }
}

Body& Document::addBody(const std::string& nm) {
    Body b;
    b.id = newId();
    b.name = nm;
    bodies.push_back(std::move(b));
    return bodies.back();
}

SketchDef& Document::addSketch(const gp_Pln& pln, const std::string& nm) {
    SketchDef s;
    s.id = newId();
    s.name = nm;
    s.plane = pln;
    sketches.push_back(std::move(s));
    return sketches.back();
}

void Document::removeBody(Id id) {
    bodies.erase(std::remove_if(bodies.begin(), bodies.end(),
                                [id](const Body& b) { return b.id == id; }),
                 bodies.end());
    meshes_.erase(id);
}

void Document::removeSketch(Id id) {
    sketches.erase(std::remove_if(sketches.begin(), sketches.end(),
                                  [id](const SketchDef& s) { return s.id == id; }),
                   sketches.end());
    // 引用该草图的特征一并移除
    for (auto& b : bodies)
        b.features.erase(std::remove_if(b.features.begin(), b.features.end(),
                                        [id](const Feature& f) {
                                            return f.sketchId == id;
                                        }),
                         b.features.end());
}

// ---------- 重算 ----------
void Document::recomputeBody(Body& b, MeshData& outMesh) {
    ShapeLookup lookup = [this](Id bodyId) -> TopoDS_Shape {
        Body* ob = body(bodyId);
        if (!ob || ob->features.empty()) return {};
        return ob->features.back().result;
    };
    TopoDS_Shape cur;
    b.error.clear();
    for (auto& f : b.features) {
        if (!f.active) continue;
        f.result = executeFeature(f, cur, lookup, *this);
        f.dirty = false;
        if (!f.error.empty()) {
            b.error = f.name + ": " + f.error;
            break; // 失败: 保留上一个成功形状
        }
        cur = f.result;
    }
    b.result = cur;
    outMesh = buildMesh(cur, meshDeflection);
}

void Document::recomputeAll() {
    Stopwatch sw;
    sw.start();
    // 简单依赖排序: 布尔引用的实体先算
    std::unordered_map<Id, bool> done;
    for (auto& b : bodies) done[b.id] = false;

    size_t remaining = bodies.size();
    while (remaining > 0) {
        std::vector<Body*> ready;
        for (auto& b : bodies) {
            if (done[b.id]) continue;
            bool depsOk = true;
            for (auto& f : b.features)
                if (f.type == FeatureType::Boolean && f.targetBody) {
                    Body* tb = body(f.targetBody);
                    if (tb && !done[tb->id]) { depsOk = false; break; }
                }
            if (depsOk) ready.push_back(&b);
        }
        if (ready.empty()) { // 依赖环: 强制解开
            for (auto& b : bodies)
                if (!done[b.id]) { ready.push_back(&b); break; }
        }
        // 并行重算无依赖实体(多线程 CPU); 网格结果主线程合并, 避免容器竞争
        std::vector<MeshData> newMeshes(ready.size());
        if (ready.size() == 1) {
            recomputeBody(*ready[0], newMeshes[0]);
        } else {
            std::vector<std::future<void>> futs;
            for (size_t i = 0; i < ready.size(); ++i) {
                Body* b = ready[i];
                MeshData* md = &newMeshes[i];
                futs.push_back(globalPool().submit([this, b, md] { recomputeBody(*b, *md); }));
            }
            for (auto& f : futs) f.get();
        }
        for (size_t i = 0; i < ready.size(); ++i) {
            meshes_[ready[i]->id] = std::move(newMeshes[i]);
            done[ready[i]->id] = true;
            --remaining;
        }
    }
    ++revision;
    LOGI("文档重算完成 (%.1f ms, %zu 实体, 线程 x%u)", sw.stop(), bodies.size(), globalPool().size());
}

bool Document::sceneBounds(Bnd_Box& out) const {
    bool any = false;
    for (auto& b : bodies) {
        if (!b.visible || b.features.empty()) continue;
        const TopoDS_Shape& s = b.features.back().result;
        if (s.IsNull()) continue;
        BRepBndLib::Add(s, out);
        any = true;
    }
    return any;
}

// ---------- 序列化 ----------
namespace {

ValuePtr serTrsf(const gp_Trsf& t) {
    auto v = Value::mkObj();
    const gp_Mat& m = t.VectorialPart(); // 含缩放
    const gp_XYZ& l = t.TranslationPart();
    double a[9] = {m.Value(1, 1), m.Value(1, 2), m.Value(1, 3), m.Value(2, 1), m.Value(2, 2),
                   m.Value(2, 3), m.Value(3, 1), m.Value(3, 2), m.Value(3, 3)};
    auto arr = Value::mkArr();
    for (double x : a) push(arr, N(x));
    setV(v, "rot", arr);
    set(v, "tx", l.X());
    set(v, "ty", l.Y());
    set(v, "tz", l.Z());
    return v;
}
gp_Trsf deserTrsf(const ValuePtr& v) {
    gp_Trsf t;
    if (!v) return t;
    auto rot = v->get("rot");
    if (rot && rot->arr->size() == 9) {
        double tx = v->getNum("tx"), ty = v->getNum("ty"), tz = v->getNum("tz");
        const auto& r = *rot->arr;
        t.SetValues(r[0]->num, r[1]->num, r[2]->num, tx,
                    r[3]->num, r[4]->num, r[5]->num, ty,
                    r[6]->num, r[7]->num, r[8]->num, tz);
    }
    return t;
}

ValuePtr serPnt(const gp_Pnt& p) {
    auto a = Value::mkArr();
    push(a, N(p.X()));
    push(a, N(p.Y()));
    push(a, N(p.Z()));
    return a;
}
gp_Pnt deserPnt(const ValuePtr& v) {
    if (!v || v->arr->size() < 3) return {};
    return {v->arr->at(0)->num, v->arr->at(1)->num, v->arr->at(2)->num};
}

} // namespace

std::string Document::serialize() const {
    auto root = Value::mkObj();
    set(root, "app", "shapr3d-desktop");
    set(root, "version", 1.0);
    set(root, "name", name);
    set(root, "nextId", (double)nextId);
    set(root, "deflection", meshDeflection);

    auto js = Value::mkArr();
    for (auto& s : sketches) {
        auto o = Value::mkObj();
        set(o, "id", (double)s.id);
        set(o, "name", s.name);
        auto pl = Value::mkObj();
        setV(pl, "origin", serPnt(s.plane.Location()));
        setV(pl, "normal", serPnt(gp_Pnt(s.plane.Axis().Direction().XYZ())));
        setV(pl, "xdir", serPnt(gp_Pnt(s.plane.XAxis().Direction().XYZ())));
        setV(pl, "ydir", serPnt(gp_Pnt(s.plane.YAxis().Direction().XYZ())));
        setV(o, "plane", pl);
        auto pts = Value::mkArr();
        for (auto& p : s.points) {
            auto po = Value::mkObj();
            set(po, "id", (double)p.id);
            set(po, "x", p.x);
            set(po, "y", p.y);
            set(po, "fixed", p.fixed);
            push(pts, po);
        }
        setV(o, "points", pts);
        auto lns = Value::mkArr();
        for (auto& l : s.lines) {
            auto lo = Value::mkObj();
            set(lo, "id", (double)l.id);
            set(lo, "p1", (double)l.p1);
            set(lo, "p2", (double)l.p2);
            set(lo, "construction", l.construction);
            push(lns, lo);
        }
        setV(o, "lines", lns);
        auto crs = Value::mkArr();
        for (auto& c : s.circles) {
            auto co = Value::mkObj();
            set(co, "id", (double)c.id);
            set(co, "center", (double)c.center);
            set(co, "r", c.r);
            push(crs, co);
        }
        setV(o, "circles", crs);
        auto ars = Value::mkArr();
        for (auto& a : s.arcs) {
            auto ao = Value::mkObj();
            set(ao, "id", (double)a.id);
            set(ao, "center", (double)a.center);
            set(ao, "r", a.r);
            set(ao, "a0", a.a0);
            set(ao, "a1", a.a1);
            push(ars, ao);
        }
        setV(o, "arcs", ars);
        auto cts = Value::mkArr();
        for (auto& c : s.constraints) {
            auto co = Value::mkObj();
            set(co, "id", (double)c.id);
            set(co, "type", (int)c.type);
            auto refs = Value::mkArr();
            for (Id r : c.refs) push(refs, N((double)r));
            setV(co, "refs", refs);
            set(co, "v", c.value);
            set(co, "v2", c.value2);
            push(cts, co);
        }
        setV(o, "constraints", cts);
        push(js, o);
    }
    setV(root, "sketches", js);

    auto jb = Value::mkArr();
    for (auto& b : bodies) {
        auto bo = Value::mkObj();
        set(bo, "id", (double)b.id);
        set(bo, "name", b.name);
        set(bo, "visible", b.visible);
        auto mt = Value::mkObj();
        set(mt, "name", b.material.name);
        auto col = Value::mkArr();
        push(col, N(b.material.color[0]));
        push(col, N(b.material.color[1]));
        push(col, N(b.material.color[2]));
        setV(mt, "color", col);
        set(mt, "metallic", b.material.metallic);
        set(mt, "roughness", b.material.roughness);
        set(mt, "density", b.material.density);
        setV(bo, "material", mt);
        auto fs = Value::mkArr();
        for (auto& f : b.features) {
            auto fo = Value::mkObj();
            set(fo, "id", (double)f.id);
            set(fo, "type", featureTypeName(f.type));
            set(fo, "name", f.name);
            set(fo, "active", f.active);
            set(fo, "p1", f.p1);
            set(fo, "p2", f.p2);
            set(fo, "p3", f.p3);
            set(fo, "sketchId", (double)f.sketchId);
            set(fo, "axisLineId", (double)f.axisLineId);
            set(fo, "opMode", f.opMode);
            set(fo, "targetBody", (double)f.targetBody);
            set(fo, "flip", f.flip);
            setV(fo, "trsf", serTrsf(f.trsf));
            auto ea = Value::mkArr();
            for (auto& p : f.edgeAnchors) push(ea, serPnt(p));
            setV(fo, "edgeAnchors", ea);
            auto fa = Value::mkArr();
            for (auto& p : f.faceAnchors) push(fa, serPnt(p));
            setV(fo, "faceAnchors", fa);
            if (f.type == FeatureType::Imported && !f.result.IsNull()) {
                // 导入体的几何不参与重算, 必须随文档内嵌保存 (ASCII BREP)
                std::stringstream ss;
                BRepTools::Write(f.result, ss, Standard_False, Standard_False,
                                 TopTools_FormatVersion_VERSION_1);
                set(fo, "importBrep", ss.str());
            }
            push(fs, fo);
        }
        setV(bo, "features", fs);
        push(jb, bo);
    }
    setV(root, "bodies", jb);
    return dump(root);
}

bool Document::deserialize(const std::string& text) {
    try {
        auto root = Parser::parse(text);
        if (!root || root->getStr("app") != "shapr3d-desktop") return false;
        bodies.clear();
        sketches.clear();
        meshes_.clear();
        name = root->getStr("name", "未命名");
        nextId = (Id)root->getNum("nextId", 1);
        meshDeflection = root->getNum("deflection", 0.15);

        if (auto js = root->get("sketches"))
            for (auto& so : *js->arr) {
                SketchDef s;
                s.id = (Id)so->getNum("id");
                s.name = so->getStr("name", "草图");
                if (auto pl = so->get("plane")) {
                    gp_Pnt o = deserPnt(pl->get("origin"));
                    gp_Pnt n = deserPnt(pl->get("normal"));
                    gp_Pnt xd = deserPnt(pl->get("xdir"));
                    gp_Pnt yd = deserPnt(pl->get("ydir"));
                    s.plane = gp_Pln(gp_Ax3(o, gp_Dir(n.X(), n.Y(), n.Z()), gp_Dir(xd.X(), xd.Y(), xd.Z())));
                    (void)yd;
                }
                if (auto pts = so->get("points"))
                    for (auto& po : *pts->arr)
                        s.points.push_back({(Id)po->getNum("id"), po->getNum("x"), po->getNum("y"), po->getBool("fixed")});
                if (auto lns = so->get("lines"))
                    for (auto& lo : *lns->arr)
                        s.lines.push_back({(Id)lo->getNum("id"), (Id)lo->getNum("p1"), (Id)lo->getNum("p2"), lo->getBool("construction")});
                if (auto crs = so->get("circles"))
                    for (auto& co : *crs->arr)
                        s.circles.push_back({(Id)co->getNum("id"), (Id)co->getNum("center"), co->getNum("r")});
                if (auto ars = so->get("arcs"))
                    for (auto& ao : *ars->arr)
                        s.arcs.push_back({(Id)ao->getNum("id"), (Id)ao->getNum("center"), ao->getNum("r"), ao->getNum("a0"), ao->getNum("a1")});
                if (auto cts = so->get("constraints"))
                    for (auto& co : *cts->arr) {
                        Constraint c;
                        c.id = (Id)co->getNum("id");
                        c.type = (CstType)(int)co->getNum("type");
                        if (auto refs = co->get("refs"))
                            for (auto& r : *refs->arr) c.refs.push_back((Id)r->num);
                        c.value = co->getNum("v");
                        c.value2 = co->getNum("v2");
                        s.constraints.push_back(c);
                    }
                sketches.push_back(std::move(s));
            }

        if (auto jb = root->get("bodies"))
            for (auto& bo : *jb->arr) {
                Body b;
                b.id = (Id)bo->getNum("id");
                b.name = bo->getStr("name", "实体");
                b.visible = bo->getBool("visible", true);
                if (auto mt = bo->get("material")) {
                    b.material.name = mt->getStr("name", "铝");
                    if (auto col = mt->get("color"))
                        for (int i = 0; i < 3 && i < (int)col->arr->size(); ++i)
                            b.material.color[i] = (float)col->arr->at(i)->num;
                    b.material.metallic = (float)mt->getNum("metallic", 1);
                    b.material.roughness = (float)mt->getNum("roughness", 0.35);
                    b.material.density = mt->getNum("density", 2.7);
                }
                if (auto fs = bo->get("features"))
                    for (auto& fo : *fs->arr) {
                        Feature f;
                        f.id = (Id)fo->getNum("id");
                        f.type = featureTypeFromName(fo->getStr("type"));
                        f.name = fo->getStr("name");
                        f.active = fo->getBool("active", true);
                        f.p1 = fo->getNum("p1");
                        f.p2 = fo->getNum("p2");
                        f.p3 = fo->getNum("p3");
                        f.sketchId = (Id)fo->getNum("sketchId");
                        f.axisLineId = (Id)fo->getNum("axisLineId");
                        f.opMode = (int)fo->getNum("opMode");
                        f.targetBody = (Id)fo->getNum("targetBody");
                        f.flip = fo->getBool("flip");
                        f.trsf = deserTrsf(fo->get("trsf"));
                        if (auto ea = fo->get("edgeAnchors"))
                            for (auto& e : *ea->arr) f.edgeAnchors.push_back(deserPnt(e));
                        if (auto fa = fo->get("faceAnchors"))
                            for (auto& e : *fa->arr) f.faceAnchors.push_back(deserPnt(e));
                        if (auto ibv = fo->get("importBrep"); ibv && ibv->type == json::Value::String) {
                            // 导入体: 从内嵌 BREP 恢复几何 (重算时 Imported 直接返回 result)
                            std::stringstream ss(ibv->str);
                            BRep_Builder builder;
                            TopoDS_Shape sh;
                            BRepTools::Read(sh, ss, builder);
                            f.result = sh;
                            f.dirty = false;
                        }
                        f.dirty = true;
                        b.features.push_back(std::move(f));
                    }
                bodies.push_back(std::move(b));
            }
        return true;
    } catch (std::exception& e) {
        LOGE("文档解析失败: %s", e.what());
        return false;
    }
}

bool Document::saveToFile(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    std::string s = serialize();
    fwrite(s.data(), 1, s.size(), fp);
    fclose(fp);
    filePath = path;
    LOGI("已保存: %s", path.c_str());
    return true;
}

bool Document::loadFromFile(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::string s;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) s.append(buf, n);
    fclose(fp);
    if (!deserialize(s)) return false;
    filePath = path;
    name = filePath;
    auto slash = name.find_last_of("/");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    auto dot = name.find_last_of(".");
    if (dot != std::string::npos) name = name.substr(0, dot);
    recomputeAll();
    return true;
}

// ---------- 欢迎文档 ----------
void buildWelcomeDocument(Document& doc) {
    // 一块带孔法兰盘: 拉伸 + 圆角 + 拉伸切除
    Body& b = doc.addBody("法兰盘");
    b.material = {"铝合金", {0.72f, 0.74f, 0.78f}, 1.0f, 0.35f, 2.70};

    // 草图1: 主轮廓(圆 直径60)
    SketchDef& s1 = doc.addSketch(planeXY(), "轮廓");
    Id c0 = doc.newId();
    s1.addPoint(0, 0, c0);
    s1.setPointFixed(c0);
    s1.addCircle(c0, 30, doc.newId());

    Feature& ext = b.features.emplace_back();
    ext.id = doc.newId();
    ext.type = FeatureType::Extrude;
    ext.name = "底板";
    ext.sketchId = s1.id;
    ext.p1 = 8;
    ext.opMode = 0;

    // 草图2: 中心孔 + 4 个螺栓孔
    SketchDef& s2 = doc.addSketch(planeXY(), "孔位");
    Id cc = doc.newId();
    s2.addPoint(0, 0, cc);
    s2.setPointFixed(cc);
    s2.addCircle(cc, 10, doc.newId());
    for (int i = 0; i < 4; ++i) {
        double a = M_PI / 2 * i + M_PI / 4;
        Id cp = doc.newId();
        s2.addPoint(22 * std::cos(a), 22 * std::sin(a), cp);
        s2.addCircle(cp, 3.5, doc.newId());
        Constraint c;
        c.id = doc.newId();
        c.type = CstType::Distance;
        c.refs = {cc, cp};
        c.value = 22;
        s2.constraints.push_back(c);
    }

    Feature& holes = b.features.emplace_back();
    holes.id = doc.newId();
    holes.type = FeatureType::Extrude;
    holes.name = "孔组";
    holes.sketchId = s2.id;
    holes.p1 = 8;
    holes.opMode = 1;

    Feature& fil = b.features.emplace_back();
    fil.id = doc.newId();
    fil.type = FeatureType::Fillet;
    fil.name = "边缘圆角";
    fil.p1 = 2;
    // 锚点: 底面外缘两条圆边(取圆上采样点)
    fil.edgeAnchors.push_back({30, 0, 8});
    fil.edgeAnchors.push_back({-30, 0, 8});

    doc.recomputeAll();
}

} // namespace cad
