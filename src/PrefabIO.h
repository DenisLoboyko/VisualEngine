#pragma once
#include "SceneIO.h"   // переиспользуем JoinScripts/SplitScripts/jsonStr/jsonFloat/jsonInt
// ═══════════════════════════════════════════════════════
//   ПРЕФАБЫ — сохранение/загрузка ОДНОГО SceneObject в файл .veprefab
//
//   В отличие от Scene.Instantiate (клонирует объект, который уже стоит
//   в текущей сцене), префаб — это файл на диске: работает из ЛЮБОЙ сцены,
//   переживает перезапуск редактора, можно скидывать другим людям.
//
//   Формат — тот же однострочный JSON, что и в SceneIO.h, плюс физика/
//   коллайдер (которых в обычном SaveScene пока нет — они живут в
//   scene.registry, а не в самом SceneObject, поэтому передаются отдельно).
// ═══════════════════════════════════════════════════════

// Данные коллайдера — SceneObject сам их не хранит (они лежат в
// scene.registry по entity ID), поэтому вызывающий код (main.cpp) должен
// сам прочитать/применить ColliderComponent при сохранении/загрузке.
struct PrefabColliderInfo {
    bool  hasCollider = false;
    int   shape       = 0;      // 0=Box, 1=Sphere, 2=Capsule — как VE::ColliderComponent::ShapeType
    float hx = 0.5f, hy = 0.5f, hz = 0.5f; // half-size (Box)
    float radius = 0.5f;                    // Sphere/Capsule
    float height = 1.0f;                    // Capsule
    bool  isTrigger = false;
};

inline void SavePrefab(const std::string& path, const SceneObject& o, const PrefabColliderInfo& col)
{
    std::ofstream f(path);
    if (!f.is_open()) { logError("Не удалось создать префаб: " + path); return; }

    f << "{\n";
    f << "  \"name\":\"" + o.name + "\",\n";
    f << "  \"type\":" + std::to_string((int)o.type) + ",\n";
    f << "  \"modelPath\":\"" + o.modelPath + "\",\n";
    f << "  \"px\":" + std::to_string(o.pos.x) + ",\"py\":" + std::to_string(o.pos.y) + ",\"pz\":" + std::to_string(o.pos.z) + ",\n";
    f << "  \"rx\":" + std::to_string(o.rot.x) + ",\"ry\":" + std::to_string(o.rot.y) + ",\"rz\":" + std::to_string(o.rot.z) + ",\n";
    f << "  \"sx\":" + std::to_string(o.scale.x) + ",\"sy\":" + std::to_string(o.scale.y) + ",\"sz\":" + std::to_string(o.scale.z) + ",\n";
    f << "  \"cr\":" + std::to_string(o.color.r) + ",\"cg\":" + std::to_string(o.color.g) + ",\"cb\":" + std::to_string(o.color.b) + ",\n";
    f << "  \"scripts\":\"" + JoinScripts(o.scriptPaths) + "\",\n";
    f << "  \"texture\":\"" + o.texturePath + "\",\n";
    f << "  \"tilingX\":" + std::to_string(o.materials.empty() ? 1.f : o.materials[0].tilingX) + ",\n";
    f << "  \"tilingY\":" + std::to_string(o.materials.empty() ? 1.f : o.materials[0].tilingY) + ",\n";
    f << "  \"animSpeed\":" + std::to_string(o.animSpeed) + ",\n";
    // Физика
    f << "  \"hasRigidBody\":" + std::to_string(o.hasRigidBody ? 1 : 0) + ",\n";
    f << "  \"mass\":" + std::to_string(o.mass) + ",\n";
    f << "  \"useGravity\":" + std::to_string(o.useGravity ? 1 : 0) + ",\n";
    // Коллайдер
    f << "  \"hasCollider\":" + std::to_string(col.hasCollider ? 1 : 0) + ",\n";
    f << "  \"colShape\":" + std::to_string(col.shape) + ",\n";
    f << "  \"colHX\":" + std::to_string(col.hx) + ",\"colHY\":" + std::to_string(col.hy) + ",\"colHZ\":" + std::to_string(col.hz) + ",\n";
    f << "  \"colRadius\":" + std::to_string(col.radius) + ",\"colHeight\":" + std::to_string(col.height) + ",\n";
    f << "  \"colTrigger\":" + std::to_string(col.isTrigger ? 1 : 0) + "\n";
    f << "}\n";
    f.close();
    logInfo("Prefab saved: " + path);
}

// Заполняет out/colOut из файла. НЕ создаёт ECS-сущность и не грузит модель —
// это делает вызывающий код (main.cpp), как и для .obj/.fbx в ассет-браузере.
inline bool LoadPrefab(const std::string& path, SceneObject& out, PrefabColliderInfo& colOut)
{
    std::ifstream f(path);
    if (!f.is_open()) { logError("Префаб не найден: " + path); return false; }

    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();
    // Собираем все строки в одну — jsonStr/jsonFloat ищут по всему тексту, разбивка по строкам не нужна
    std::string line = content;
    for (auto& c : line) if (c == '\n' || c == '\r') c = ' ';

    out = SceneObject{};
    out.name       = jsonStr(line, "name");
    out.type       = (PrimitiveType)jsonInt(line, "type");
    out.modelPath  = jsonStr(line, "modelPath");
    out.pos        = { jsonFloat(line,"px"), jsonFloat(line,"py"), jsonFloat(line,"pz") };
    out.rot        = { jsonFloat(line,"rx"), jsonFloat(line,"ry"), jsonFloat(line,"rz") };
    out.scale      = { jsonFloat(line,"sx"), jsonFloat(line,"sy"), jsonFloat(line,"sz") };
    out.color      = { jsonFloat(line,"cr"), jsonFloat(line,"cg"), jsonFloat(line,"cb") };
    std::string scriptsJoined = jsonStr(line, "scripts");
    if (!scriptsJoined.empty()) { out.scriptPaths = SplitScripts(scriptsJoined); out.hasScript = true; }
    out.texturePath = jsonStr(line, "texture");
    if (!out.texturePath.empty()) out.textureID = VE::LoadTexture(out.texturePath);
    {
        Material m0; m0.name = "Default"; m0.texturePath = out.texturePath; m0.textureID = out.textureID; m0.color = out.color;
        float tx = jsonFloat(line,"tilingX"); m0.tilingX = tx > 0.f ? tx : 1.f;
        float ty = jsonFloat(line,"tilingY"); m0.tilingY = ty > 0.f ? ty : 1.f;
        out.materials.push_back(m0);
    }
    float animSp = jsonFloat(line,"animSpeed");
    out.animSpeed = (animSp != 0.f) ? animSp : 1.f;

    out.hasRigidBody = jsonInt(line, "hasRigidBody") == 1;
    out.mass         = jsonFloat(line, "mass");
    if (out.mass <= 0.f) out.mass = 1.f;
    out.useGravity   = jsonInt(line, "useGravity") == 1;

    colOut.hasCollider = jsonInt(line, "hasCollider") == 1;
    colOut.shape        = jsonInt(line, "colShape");
    colOut.hx = jsonFloat(line,"colHX"); colOut.hy = jsonFloat(line,"colHY"); colOut.hz = jsonFloat(line,"colHZ");
    colOut.radius = jsonFloat(line,"colRadius");
    colOut.height = jsonFloat(line,"colHeight");
    colOut.isTrigger = jsonInt(line, "colTrigger") == 1;

    // Если модель 3D — грузим её (как это делает двойной клик по .obj/.fbx в ассет-браузере)
    if (out.type == PrimitiveType::Model3D && !out.modelPath.empty()) {
        out.model = std::make_shared<VE::Model>();
        out.model->Load(out.modelPath);
    }

    return true;
}
