// Document.h —— 文档模型: 实体(特征链) + 草图 + 材质 + 重算调度
#pragma once
#include <unordered_map>
#include "../core/Common.h"
#include "Feature.h"
#include "../sketch/SketchDef.h"

namespace cad {

struct Body {
    Id id = kInvalidId;
    std::string name = "实体";
    std::vector<Feature> features;   // 顺序 = 特征历史
    Material material;
    bool visible = true;
    bool hiddenByOp = false;         // 布尔切除后自动隐藏的工具体
    std::string error;               // 重算错误
    TopoDS_Shape result;             // 链末端形状(缓存)
};

// 网格缓存(渲染/拾取共用, 数据与 GL 解耦)
struct MeshData {
    std::vector<float> verts;      // 3N
    std::vector<float> normals;    // 3N
    std::vector<uint32_t> faceStart; // 每个面的起始三角索引
    std::vector<uint32_t> faceCount; // 每个面的三角数
    TopTools_IndexedMapOfShape faceMap; // 面索引 -> TopoDS_Face
    std::vector<std::vector<float>> edgeLines; // 每条棱折线 3K
    bool empty() const { return verts.empty(); }
    void clear() { *this = MeshData(); }
};

class Document {
public:
    std::vector<Body> bodies;
    std::vector<SketchDef> sketches;
    Id nextId = 1;
    std::string filePath;            // 工程文件路径
    std::string name = "未命名";
    double meshDeflection = 0.15;    // 网格精度(mm)
    uint64_t revision = 0;           // 重算版本号(渲染缓存失效用)

    Id newId() { return nextId++; }

    // ---- 访问 ----
    Body* body(Id id) {
        for (auto& b : bodies) if (b.id == id) return &b;
        return nullptr;
    }
    SketchDef* sketch(Id id) {
        for (auto& s : sketches) if (s.id == id) return &s;
        return nullptr;
    }
    Feature* feature(Id fid) {
        for (auto& b : bodies)
            for (auto& f : b.features)
                if (f.id == fid) return &f;
        return nullptr;
    }
    Body* bodyOfFeature(Id fid) {
        for (auto& b : bodies)
            for (auto& f : b.features)
                if (f.id == fid) return &b;
        return nullptr;
    }

    // ---- 结构操作 ----
    Body& addBody(const std::string& nm = "实体");
    SketchDef& addSketch(const gp_Pln& pln, const std::string& nm = "草图");
    void removeBody(Id id);
    void removeSketch(Id id);

    // ---- 重算(多线程) ----
    void recomputeAll();
    // 网格缓存
    const MeshData* mesh(Id bodyId) const {
        auto it = meshes_.find(bodyId);
        return it == meshes_.end() ? nullptr : &it->second;
    }
    MeshData* meshMut(Id bodyId) {
        auto it = meshes_.find(bodyId);
        return it == meshes_.end() ? nullptr : &it->second;
    }

    // 实体总包围盒(显示自适应)
    bool sceneBounds(Bnd_Box& out) const;

    // ---- 快照(撤销/重做) ----
    std::string serialize() const;
    bool deserialize(const std::string& json);

    // ---- 文件 ----
    bool saveToFile(const std::string& path);
    bool loadFromFile(const std::string& path);

private:
    std::unordered_map<Id, MeshData> meshes_;
    void recomputeBody(Body& b, MeshData& outMesh);
};

// 示例模型(欢迎文档)
void buildWelcomeDocument(Document& doc);

} // namespace cad
