#pragma once
#include "SceneObject.h"
#include "UI2D.h"
#include "Sprites2D.h"
extern std::vector<UIElement> uiElements;
extern std::vector<Sprite2D> sprites2D;
// в”Ђв”Ђ РЎРѕС…СЂР°РЅРµРЅРёРµ/Р·Р°РіСЂСѓР·РєР° СЃС†РµРЅС‹ РІ JSON (РІС‹РЅРµСЃРµРЅРѕ РёР· main.cpp) в”Ђв”Ђ



// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
//   РЎРћРҐР РђРќР•РќРР• / Р—РђР“Р РЈР—РљРђ РЎР¦Р•РќР« (РїСЂРѕСЃС‚РѕР№ JSON Р±РµР· Р±РёР±Р»РёРѕС‚РµРє)
// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
// в”Ђв”Ђ РќРµСЃРєРѕР»СЊРєРѕ СЃРєСЂРёРїС‚РѕРІ РЅР° РѕР±СЉРµРєС‚ С…СЂР°РЅРёРј РІ РѕРґРЅРѕР№ СЃС‚СЂРѕРєРµ С‡РµСЂРµР· ";" в”Ђв”Ђ
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
    f << "  ],\n";
    f << "  ],\n  \"ui\": [\n";
    for(int i=0;i<(int)uiElements.size();i++){
        auto& u=uiElements[i];
        f << "    {\"name\":\""<<u.name<<"\",";
        f << "\"type\":"<<(int)u.type<<",";
        f << "\"anchorX\":"<<u.anchor.x<<",\"anchorY\":"<<u.anchor.y<<",";
        f << "\"sizeX\":"<<u.size.x<<",\"sizeY\":"<<u.size.y<<",";
        f << "\"sizeScaleX\":"<<u.sizeScale.x<<",\"sizeScaleY\":"<<u.sizeScale.y<<",";
        f << "\"offsetX\":"<<u.posOffset.x<<",\"offsetY\":"<<u.posOffset.y<<",";
        f << "\"pivotX\":"<<u.anchorPoint.x<<",\"pivotY\":"<<u.anchorPoint.y<<",";
        f << "\"cr\":"<<u.color.r<<",\"cg\":"<<u.color.g<<",\"cb\":"<<u.color.b<<",\"ca\":"<<u.color.a<<",";
        f << "\"text\":\""<<u.text<<"\",";
        f << "\"fontSize\":"<<u.fontSize<<",";
        f << "\"z\":"<<u.z<<",\"parent\":"<<u.parentIndex<<",";
        f << "\"rounding\":"<<u.cornerRadius<<",";
        f << "\"transparency\":"<<u.transparency<<",";
        f << "\"visible\":"<<(u.visible?1:0)<<",\"fx\":"<<(u.fx?1:0)<<",";
        f << "\"texPath\":\""<<u.texPath<<"\"}";
        if(i<(int)uiElements.size()-1) f << ",";
        f << "\n";
    }
    f << "  ],\n  \"sprites\": [\n";
    for(int i=0;i<(int)sprites2D.size();i++){
        auto& s=sprites2D[i];
        f << "    {\"name\":\""<<s.name<<"\",";
        f << "\"px\":"<<s.pos.x<<",\"py\":"<<s.pos.y<<",";
        f << "\"sx\":"<<s.scale.x<<",\"sy\":"<<s.scale.y<<",";
        f << "\"z\":"<<s.z<<",";
        f << "\"texPath\":\""<<s.texPath<<"\"}";
        if(i<(int)sprites2D.size()-1) f << ",";
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
int jsonIntDef(const std::string& s, const std::string& key, int def){
    auto p=s.find("\""+key+"\":");
    if(p==std::string::npos) return def;
    p+=key.size()+3;
    try{ return (int)std::stof(s.substr(p)); } catch(...){ return def; }
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
    uiElements.clear(); sprites2D.clear();
    sel=-1; selType=SelectionType::None;
    std::string line, section="";
    while(std::getline(f,line)){
        if(line.find("\"objects\"")!=std::string::npos) section="obj";
        else if(line.find("\"lights\"")!=std::string::npos) section="lit";
        else if(line.find("\"cameras\"")!=std::string::npos) section="cam";
        else if(line.find("\"ui\"")!=std::string::npos) section="ui";
        else if(line.find("\"sprites\"")!=std::string::npos) section="spr";
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
                    std::string legacy = jsonStr(line,"script"); // СЃС‚Р°СЂС‹Рµ СЃС†РµРЅС‹, РѕРґРёРЅ СЃРєСЂРёРїС‚
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
            } else if(section=="ui"){
                UIElement u;
                u.name=jsonStr(line,"name");
                u.type=(UIElement::Type)jsonIntDef(line,"type",0);
                u.anchor={jsonFloat(line,"anchorX"),jsonFloat(line,"anchorY")};
                u.size={jsonFloat(line,"sizeX"),jsonFloat(line,"sizeY")};
                u.sizeScale={jsonFloat(line,"sizeScaleX"),jsonFloat(line,"sizeScaleY")};
                u.posOffset={jsonFloat(line,"offsetX"),jsonFloat(line,"offsetY")};
                u.anchorPoint={jsonFloat(line,"pivotX"),jsonFloat(line,"pivotY")};
                u.color={jsonFloat(line,"cr"),jsonFloat(line,"cg"),jsonFloat(line,"cb"),jsonFloat(line,"ca")};
                u.text=jsonStr(line,"text");
                u.fontSize=jsonFloat(line,"fontSize");
                u.z=jsonInt(line,"z");
                u.parentIndex=jsonIntDef(line,"parent",-1);
                u.cornerRadius=jsonFloat(line,"rounding");
                u.transparency=jsonFloat(line,"transparency");
                u.visible=jsonIntDef(line,"visible",1)==1;
                u.fx=jsonIntDef(line,"fx",0)==1;
                u.texPath=jsonStr(line,"texPath");
                if(!u.texPath.empty()) u.tex=VE::LoadTextureRaw(u.texPath);
                uiElements.push_back(u);
            } else if(section=="spr"){
                Sprite2D s;
                s.name=jsonStr(line,"name");
                s.pos={jsonFloat(line,"px"),jsonFloat(line,"py")};
                s.scale={jsonFloat(line,"sx"),jsonFloat(line,"sy")};
                s.z=jsonInt(line,"z");
                s.texPath=jsonStr(line,"texPath");
                if(!s.texPath.empty()) s.tex=VE::LoadTextureRaw(s.texPath);
                sprites2D.push_back(s);
            }
        }
    }
    if(!objects.empty()){sel=0;selType=SelectionType::Object;}
}

