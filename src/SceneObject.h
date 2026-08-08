#pragma once
#include "Material.h"
// ── SceneObject, Ray, гизмо-хиттесты (вынесено из main.cpp) ──

struct SceneObject {
    std::string name;
    glm::vec3 pos={0,0,0},rot={0,0,0},scale={1,1,1},color={0.8f,0.6f,0.3f};
    PrimitiveType type=PrimitiveType::Cube;
    std::string modelPath;
    std::vector<std::string> scriptPaths;  // несколько скриптов на объект (как компоненты в Unity/Godot)
    std::shared_ptr<VE::Model> model;
    bool active=true; VE::EntityID ecsID=VE::NULL_ENTITY;
    bool hasScript=false, hasRigidBody=false;
    float mass=1.f; bool useGravity=true;
    int parentIndex=-1;
    glm::vec3 localOffset={0,0,0};
    std::string texturePath;     // оставлено для обратной совместимости со старыми сценами
    GLuint textureID=0;
    std::vector<Material> materials;  // список материалов (слот 0 = основной)
    int activeMaterial = 0;            // какой материал сейчас выбран в инспекторе
    std::vector<std::shared_ptr<VE::LuaEngine>> luaInstances; // Lua-движки для Play-режима (по одному на скрипт)
    float lookPitch=0.f; // взгляд камеры вверх/вниз (FPS) — НЕ вращает саму модель объекта
    // ── Скелетная анимация (если у model есть кости/клипы) ──
    int   animIndex   = -1;    // индекс текущего клипа в obj.model->animations, -1 = не играет
    float animTime    = 0.f;   // секунды с начала клипа
    bool  animPlaying = false;
    bool  animLoop    = true;
    float animSpeed   = 1.f;   // множитель скорости — Animation.SetAnimationSpeed из Lua
    // ── Кастомная покадровая анимация (работает для ЛЮБОГО объекта) ──
    std::vector<ObjectAnimClip> customClips;
    int   customClipIndex   = -1;
    float customAnimTime    = 0.f;
    bool  customAnimPlaying = false;
};

struct SavedTransform { glm::vec3 pos,rot,scale; };

struct ConsoleEntry { std::string msg; int level; };
std::vector<ConsoleEntry> consoleLog;
void logInfo (const std::string& m){ consoleLog.push_back({m,0}); }
void logWarn (const std::string& m){ consoleLog.push_back({m,1}); }
void logError(const std::string& m){ consoleLog.push_back({m,2}); }

// ── Консольные команды ──
static char g_CmdBuf[512] = {};
static std::vector<std::string> g_CmdHistory;
static int  g_CmdHistoryIdx    = -1;
static bool g_ConsoleFocusInput = false;

struct Ray { glm::vec3 origin,dir; };

// ── Реализация RaycastObjectUV (Ray и SceneObject тут уже полностью определены) ──
inline bool RaycastObjectUV(const Ray& worldRay, const SceneObject& obj, glm::vec2& outUV) {
    if (obj.type != PrimitiveType::Cube && obj.type != PrimitiveType::Plane) return false;
    glm::mat4 model=glm::mat4(1);
    model=glm::translate(model,obj.pos);
    model=glm::rotate(model,glm::radians(obj.rot.x),glm::vec3(1,0,0));
    model=glm::rotate(model,glm::radians(obj.rot.y),glm::vec3(0,1,0));
    model=glm::rotate(model,glm::radians(obj.rot.z),glm::vec3(0,0,1));
    model=glm::scale(model,obj.scale);
    glm::mat4 inv=glm::inverse(model);
    glm::vec3 lo=glm::vec3(inv*glm::vec4(worldRay.origin,1));
    glm::vec3 ld=glm::normalize(glm::vec3(inv*glm::vec4(worldRay.dir,0)));
    const auto& tris = (obj.type==PrimitiveType::Cube) ? GetCubeTrisForPaint() : GetPlaneTrisForPaint();
    float bestT=1e9f; bool hit=false; glm::vec2 bestUV(0,0);
    for (auto& tri : tris) {
        float t,u,v;
        if (RayTriIntersect(lo,ld,tri.p0,tri.p1,tri.p2,t,u,v) && t<bestT) {
            bestT=t; hit=true;
            bestUV = tri.uv0*(1.f-u-v) + tri.uv1*u + tri.uv2*v;
        }
    }
    if (hit) outUV=bestUV;
    return hit;
}
Ray screenToRay(double mx,double my,int w,int h,const glm::mat4& v,const glm::mat4& p){
    float nx=(2.f*mx/w)-1.f,ny=1.f-(2.f*my/h);
    glm::vec4 eye=glm::inverse(p)*glm::vec4(nx,ny,-1,1);
    eye=glm::vec4(eye.x,eye.y,-1,0);
    return{glm::vec3(glm::inverse(v)*glm::vec4(0,0,0,1)),glm::normalize(glm::vec3(glm::inverse(v)*eye))};
}
bool rayAABB(const Ray& r,glm::vec3 c,glm::vec3 hs,float& t){
    glm::vec3 mn=c-hs,mx=c+hs;float tmin=-1e9f,tmax=1e9f;
    for(int i=0;i<3;i++){
        if(fabs(r.dir[i])<1e-6f){if(r.origin[i]<mn[i]||r.origin[i]>mx[i])return false;}
        else{float t1=(mn[i]-r.origin[i])/r.dir[i],t2=(mx[i]-r.origin[i])/r.dir[i];
            if(t1>t2)std::swap(t1,t2);tmin=std::max(tmin,t1);tmax=std::min(tmax,t2);if(tmin>tmax)return false;}
    }
    t=tmin>0?tmin:tmax;return t>0;
}
float rayPlaneT(const Ray& r,glm::vec3 n,glm::vec3 p){
    float d=glm::dot(n,r.dir);if(fabs(d)<1e-6f)return-1;
    return glm::dot(n,p-r.origin)/d;
}
bool gizmoArrowHit(const Ray& ray,glm::vec3 op,glm::vec3 ax,float gs,float& t){
    glm::vec3 end=op+ax*gs;
    return rayAABB(ray,(glm::min(op,end)+glm::max(op,end))*.5f,(glm::max(op,end)-glm::min(op,end))*.5f+0.08f*gs,t);
}

