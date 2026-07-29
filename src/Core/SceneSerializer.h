#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>

// =========================================================
//  Простой JSON сериализатор сцены
//  Сохраняет в .scene файл (JSON формат)
// =========================================================

// Вспомогательные функции для записи JSON
namespace JsonWriter {
    inline std::string vec3(const glm::vec3& v){
        return "{\"x\":" + std::to_string(v.x) + ",\"y\":" + std::to_string(v.y) + ",\"z\":" + std::to_string(v.z) + "}";
    }
    inline std::string str(const std::string& s){ return "\"" + s + "\""; }
    inline std::string num(float f){ return std::to_string(f); }
    inline std::string boolean(bool b){ return b ? "true" : "false"; }
}

// Вспомогательные функции для чтения JSON
namespace JsonReader {
    inline std::string getValue(const std::string& json, const std::string& key){
        std::string search = "\"" + key + "\":";
        size_t pos = json.find(search);
        if(pos == std::string::npos) return "";
        pos += search.size();
        // Пропускаем пробелы
        while(pos < json.size() && json[pos] == ' ') pos++;
        if(json[pos] == '"'){
            // Строка
            pos++;
            size_t end = json.find('"', pos);
            return json.substr(pos, end - pos);
        }
        if(json[pos] == '{'){
            // Объект
            size_t end = json.find('}', pos);
            return json.substr(pos, end - pos + 1);
        }
        // Число или boolean
        size_t end = json.find_first_of(",}\n", pos);
        return json.substr(pos, end - pos);
    }

    inline float getFloat(const std::string& json, const std::string& key){
        std::string v = getValue(json, key);
        if(v.empty()) return 0.f;
        try{ return std::stof(v); } catch(...){ return 0.f; }
    }

    inline bool getBool(const std::string& json, const std::string& key){
        return getValue(json, key) == "true";
    }

    inline glm::vec3 getVec3(const std::string& json, const std::string& key){
        std::string obj = getValue(json, key);
        if(obj.empty()) return glm::vec3(0);
        return glm::vec3(getFloat(obj,"x"), getFloat(obj,"y"), getFloat(obj,"z"));
    }

    // Извлекаем все объекты из JSON массива
    inline std::vector<std::string> getArray(const std::string& json, const std::string& key){
        std::vector<std::string> result;
        std::string search = "\"" + key + "\":[";
        size_t pos = json.find(search);
        if(pos == std::string::npos) return result;
        pos += search.size();
        int depth = 0;
        size_t start = pos;
        for(size_t i = pos; i < json.size(); i++){
            if(json[i] == '{') { if(depth == 0) start = i; depth++; }
            else if(json[i] == '}'){
                depth--;
                if(depth == 0) result.push_back(json.substr(start, i - start + 1));
            }
            else if(json[i] == ']' && depth == 0) break;
        }
        return result;
    }
}

// Структуры для сохранения (forward declaration из main.cpp)
// Используем шаблонные структуры чтобы не зависеть от типов main.cpp

struct SavedObject {
    std::string name;
    glm::vec3 pos, rot, scale, color;
    int type; // PrimitiveType
    std::string scriptPath, modelPath;
    bool active;
    bool hasScript, hasRigidBody;
    float mass; bool useGravity;
};

struct SavedLight {
    std::string name;
    glm::vec3 pos, color;
    float intensity, range;
    bool active;
};

struct SavedCamera {
    std::string name;
    glm::vec3 pos, rot;
    float fov;
    bool active, isPrimary;
};

struct SceneData {
    std::string sceneName = "Untitled";
    std::vector<SavedObject>  objects;
    std::vector<SavedLight>   lights;
    std::vector<SavedCamera>  cameras;
};

// =========================================================
//  Сохранение
// =========================================================
inline bool SaveScene(const std::string& filepath, const SceneData& data)
{
    std::ofstream f(filepath);
    if(!f.is_open()) return false;

    f << "{\n";
    f << "  \"sceneName\": " << JsonWriter::str(data.sceneName) << ",\n";

    // Objects
    f << "  \"objects\": [\n";
    for(int i = 0; i < (int)data.objects.size(); i++){
        auto& o = data.objects[i];
        f << "    {\n";
        f << "      \"name\": "       << JsonWriter::str(o.name)          << ",\n";
        f << "      \"type\": "       << o.type                           << ",\n";
        f << "      \"active\": "     << JsonWriter::boolean(o.active)    << ",\n";
        f << "      \"pos\": "        << JsonWriter::vec3(o.pos)          << ",\n";
        f << "      \"rot\": "        << JsonWriter::vec3(o.rot)          << ",\n";
        f << "      \"scale\": "      << JsonWriter::vec3(o.scale)        << ",\n";
        f << "      \"color\": "      << JsonWriter::vec3(o.color)        << ",\n";
        f << "      \"scriptPath\": " << JsonWriter::str(o.scriptPath)    << ",\n";
        f << "      \"modelPath\": "  << JsonWriter::str(o.modelPath)     << ",\n";
        f << "      \"hasScript\": "  << JsonWriter::boolean(o.hasScript) << ",\n";
        f << "      \"hasRigidBody\":" << JsonWriter::boolean(o.hasRigidBody) << ",\n";
        f << "      \"mass\": "       << JsonWriter::num(o.mass)          << ",\n";
        f << "      \"useGravity\": " << JsonWriter::boolean(o.useGravity)<< "\n";
        f << "    }" << (i < (int)data.objects.size()-1 ? "," : "") << "\n";
    }
    f << "  ],\n";

    // Lights
    f << "  \"lights\": [\n";
    for(int i = 0; i < (int)data.lights.size(); i++){
        auto& l = data.lights[i];
        f << "    {\n";
        f << "      \"name\": "      << JsonWriter::str(l.name)         << ",\n";
        f << "      \"active\": "    << JsonWriter::boolean(l.active)   << ",\n";
        f << "      \"pos\": "       << JsonWriter::vec3(l.pos)         << ",\n";
        f << "      \"color\": "     << JsonWriter::vec3(l.color)       << ",\n";
        f << "      \"intensity\": " << JsonWriter::num(l.intensity)    << ",\n";
        f << "      \"range\": "     << JsonWriter::num(l.range)        << "\n";
        f << "    }" << (i < (int)data.lights.size()-1 ? "," : "") << "\n";
    }
    f << "  ],\n";

    // Cameras
    f << "  \"cameras\": [\n";
    for(int i = 0; i < (int)data.cameras.size(); i++){
        auto& c = data.cameras[i];
        f << "    {\n";
        f << "      \"name\": "      << JsonWriter::str(c.name)          << ",\n";
        f << "      \"active\": "    << JsonWriter::boolean(c.active)    << ",\n";
        f << "      \"isPrimary\": " << JsonWriter::boolean(c.isPrimary) << ",\n";
        f << "      \"pos\": "       << JsonWriter::vec3(c.pos)          << ",\n";
        f << "      \"rot\": "       << JsonWriter::vec3(c.rot)          << ",\n";
        f << "      \"fov\": "       << JsonWriter::num(c.fov)           << "\n";
        f << "    }" << (i < (int)data.cameras.size()-1 ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";

    return true;
}

// =========================================================
//  Загрузка
// =========================================================
inline bool LoadScene(const std::string& filepath, SceneData& data)
{
    std::ifstream f(filepath);
    if(!f.is_open()) return false;

    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    data.sceneName = JsonReader::getValue(json, "sceneName");

    // Objects
    auto objs = JsonReader::getArray(json, "objects");
    for(auto& o : objs){
        SavedObject obj;
        obj.name       = JsonReader::getValue(o, "name");
        obj.type       = (int)JsonReader::getFloat(o, "type");
        obj.active     = JsonReader::getBool(o, "active");
        obj.pos        = JsonReader::getVec3(o, "pos");
        obj.rot        = JsonReader::getVec3(o, "rot");
        obj.scale      = JsonReader::getVec3(o, "scale");
        obj.color      = JsonReader::getVec3(o, "color");
        obj.scriptPath = JsonReader::getValue(o, "scriptPath");
        obj.modelPath  = JsonReader::getValue(o, "modelPath");
        obj.hasScript  = JsonReader::getBool(o, "hasScript");
        obj.hasRigidBody = JsonReader::getBool(o, "hasRigidBody");
        obj.mass       = JsonReader::getFloat(o, "mass");
        obj.useGravity = JsonReader::getBool(o, "useGravity");
        data.objects.push_back(obj);
    }

    // Lights
    auto lts = JsonReader::getArray(json, "lights");
    for(auto& l : lts){
        SavedLight lt;
        lt.name      = JsonReader::getValue(l, "name");
        lt.active    = JsonReader::getBool(l, "active");
        lt.pos       = JsonReader::getVec3(l, "pos");
        lt.color     = JsonReader::getVec3(l, "color");
        lt.intensity = JsonReader::getFloat(l, "intensity");
        lt.range     = JsonReader::getFloat(l, "range");
        data.lights.push_back(lt);
    }

    // Cameras
    auto cams = JsonReader::getArray(json, "cameras");
    for(auto& c : cams){
        SavedCamera cam;
        cam.name      = JsonReader::getValue(c, "name");
        cam.active    = JsonReader::getBool(c, "active");
        cam.isPrimary = JsonReader::getBool(c, "isPrimary");
        cam.pos       = JsonReader::getVec3(c, "pos");
        cam.rot       = JsonReader::getVec3(c, "rot");
        cam.fov       = JsonReader::getFloat(c, "fov");
        data.cameras.push_back(cam);
    }

    return true;
}