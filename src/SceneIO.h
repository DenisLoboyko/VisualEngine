#pragma once
#include "SceneObject.h"
// ── Сохранение/загрузка сцены в JSON (вынесено из main.cpp) ──



// ═══════════════════════════════════════════════════════
//   СОХРАНЕНИЕ / ЗАГРУЗКА СЦЕНЫ (простой JSON без библиотек)
// ═══════════════════════════════════════════════════════
// ── Несколько скриптов на объект храним в одной строке через ";" ──
std::string JoinScripts(const std::vector<std::string>& scripts){
    std::string out;
    for(size_t i=0;i<scripts.size();i++){ out+=scripts[i]; if(i+1<scripts.size()) out+=";"; }
    return out;
}
std::vector<std::string> SplitScripts(const std::string& s){
    std::vector<std::string> out;
    size_t start=0;
    while(true){
        size_t p=s.find(';',start);
        std::string part = (p==std::string::npos) ? s.substr(start) : s.substr(start,p-start);
        if(!part.empty()) out.push_back(part);
        if(p==std::string::npos) break;
        start=p+1;
    }
    return out;
}

void SaveScene(const std::string& path,
    const std::vector<SceneObject>& objects,
    const std::vector<LightObject>& lights,
    const std::vector<CameraObject>& cameras)
{
    std::ofstream f(path);
    if(!f.is_open()){ return; }
    f << "{\n";
    // Objects
    f << "  \"objects\": [\n";
    for(int i=0;i<(int)objects.size();i++){
        auto& o=objects[i];
        f << "    {";
        f << "\"name\":\""+o.name+"\",";
        f << "\"type\":" +std::to_string((int)o.type)+",";
        f << "\"px\":"+std::to_string(o.pos.x)+",\"py\":"+std::to_string(o.pos.y)+",\"pz\":"+std::to_string(o.pos.z)+",";
        f << "\"rx\":"+std::to_string(o.rot.x)+",\"ry\":"+std::to_string(o.rot.y)+",\"rz\":"+std::to_string(o.rot.z)+",";
        f << "\"sx\":"+std::to_string(o.scale.x)+",\"sy\":"+std::to_string(o.scale.y)+",\"sz\":"+std::to_string(o.scale.z)+",";
        f << "\"cr\":"+std::to_string(o.color.r)+",\"cg\":"+std::to_string(o.color.g)+",\"cb\":"+std::to_string(o.color.b)+",";
        f << "\"scripts\":\""+JoinScripts(o.scriptPaths)+"\",";
        f << "\"texture\":\""+o.texturePath+"\",";
        f << "\"tilingX\":"+std::to_string(o.materials.empty()?1.f:o.materials[0].tilingX)+",";
        f << "\"tilingY\":"+std::to_string(o.materials.empty()?1.f:o.materials[0].tilingY);
        f << "}";
        if(i<(int)objects.size()-1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Lights
    f << "  \"lights\": [\n";
    for(int i=0;i<(int)lights.size();i++){
        auto& l=lights[i];
        f << "    {";
        f << "\"name\":\""+l.name+"\",";
        f << "\"px\":"+std::to_string(l.pos.x)+",\"py\":"+std::to_string(l.pos.y)+",\"pz\":"+std::to_string(l.pos.z)+",";
        f << "\"cr\":"+std::to_string(l.color.r)+",\"cg\":"+std::to_string(l.color.g)+",\"cb\":"+std::to_string(l.color.b)+",";
        f << "\"intensity\":"+std::to_string(l.intensity)+",\"range\":"+std::to_string(l.range);
        f << "}";
        if(i<(int)lights.size()-1) f << ",";
        f << "\n";
    }
    f << "  ],\n";
    // Cameras
    f << "  \"cameras\": [\n";
    for(int i=0;i<(int)cameras.size();i++){
        auto& c=cameras[i];
        f << "    {";
        f << "\"name\":\""+c.name+"\",";
        f << "\"px\":"+std::to_string(c.pos.x)+",\"py\":"+std::to_string(c.pos.y)+",\"pz\":"+std::to_string(c.pos.z)+",";
        f << "\"fov\":"+std::to_string(c.fov)+",\"primary\":"+std::to_string(c.isPrimary?1:0);
        f << "}";
        if(i<(int)cameras.size()-1) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    f.close();
}

std::string jsonStr(const std::string& s, const std::string& key){
    // Extract string value for key from simple json line
    std::string search = "\""+key+"\"\":\"";
    // Try string value
    auto p = s.find("\""+key+"\": \"");
    if(p==std::string::npos) p=s.find("\""+key+"\":\"");
    if(p==std::string::npos) return "";
    p=s.find("\"",p+key.size()+3);
    if(p==std::string::npos) return "";
    p++;
    auto e=s.find("\"",p);
    return e==std::string::npos?"":s.substr(p,e-p);
}
float jsonFloat(const std::string& s, const std::string& key){
    auto p=s.find("\""+key+"\":"); 
    if(p==std::string::npos) return 0.f;
    p+=key.size()+3;
    try{ return std::stof(s.substr(p)); } catch(...){ return 0.f; }
}
int jsonInt(const std::string& s, const std::string& key){
    return (int)jsonFloat(s,key);
}

void LoadScene(const std::string& path,
    std::vector<SceneObject>& objects,
    std::vector<LightObject>& lights,
    std::vector<CameraObject>& cameras,
    int& sel, SelectionType& selType)
{
    std::ifstream f(path);
    if(!f.is_open()) return;
    objects.clear(); lights.clear(); cameras.clear();
    sel=-1; selType=SelectionType::None;
    std::string line, section="";
    while(std::getline(f,line)){
        if(line.find("\"objects\"")!=std::string::npos) section="obj";
        else if(line.find("\"lights\"")!=std::string::npos) section="lit";
        else if(line.find("\"cameras\"")!=std::string::npos) section="cam";
        else if(line.find("{")!=std::string::npos && line.find("name")!=std::string::npos){
            if(section=="obj"){
                SceneObject o;
                o.name=jsonStr(line,"name");
                o.type=(PrimitiveType)jsonInt(line,"type");
                o.pos={jsonFloat(line,"px"),jsonFloat(line,"py"),jsonFloat(line,"pz")};
                o.rot={jsonFloat(line,"rx"),jsonFloat(line,"ry"),jsonFloat(line,"rz")};
                o.scale={jsonFloat(line,"sx"),jsonFloat(line,"sy"),jsonFloat(line,"sz")};
                o.color={jsonFloat(line,"cr"),jsonFloat(line,"cg"),jsonFloat(line,"cb")};
                std::string scriptsJoined = jsonStr(line,"scripts");
                if(!scriptsJoined.empty()){
                    o.scriptPaths = SplitScripts(scriptsJoined);
                } else {
                    std::string legacy = jsonStr(line,"script"); // старые сцены, один скрипт
                    if(!legacy.empty()) o.scriptPaths.push_back(legacy);
                }
                o.hasScript=!o.scriptPaths.empty();
                o.texturePath=jsonStr(line,"texture");
                if(!o.texturePath.empty()) o.textureID=VE::LoadTexture(o.texturePath);
                {
                    Material m0; m0.name="Default"; m0.texturePath=o.texturePath; m0.textureID=o.textureID; m0.color=o.color;
                    float tx=jsonFloat(line,"tilingX"); m0.tilingX = tx>0.f ? tx : 1.f;
                    float ty=jsonFloat(line,"tilingY"); m0.tilingY = ty>0.f ? ty : 1.f;
                    o.materials.push_back(m0);
                }
                o.ecsID=scene.CreateEntity(o.name);
                scene.GetTransform(o.ecsID).Position=o.pos;
                scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                objects.push_back(o);
            } else if(section=="lit"){
                LightObject l;
                l.name=jsonStr(line,"name");
                l.pos={jsonFloat(line,"px"),jsonFloat(line,"py"),jsonFloat(line,"pz")};
                l.color={jsonFloat(line,"cr"),jsonFloat(line,"cg"),jsonFloat(line,"cb")};
                l.intensity=jsonFloat(line,"intensity");
                l.range=jsonFloat(line,"range");
                l.ecsID=scene.CreateEntity(l.name);
                scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);
                lights.push_back(l);
            } else if(section=="cam"){
                CameraObject c;
                c.name=jsonStr(line,"name");
                c.pos={jsonFloat(line,"px"),jsonFloat(line,"py"),jsonFloat(line,"pz")};
                c.fov=jsonFloat(line,"fov");
                c.isPrimary=jsonInt(line,"primary")==1;
                c.ecsID=scene.CreateEntity(c.name);
                scene.registry.AddComponent<VE::CameraComponent>(c.ecsID,c.isPrimary);
                cameras.push_back(c);
            }
        }
    }
    if(!objects.empty()){sel=0;selType=SelectionType::Object;}
}

