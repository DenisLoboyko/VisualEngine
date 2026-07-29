#define IMGUI_DEFINE_MATH_OPERATORS
#define MINIAUDIO_IMPLEMENTATION
#include "Core/AudioEngine.h"
#include "Core/SceneManager.h"
#include "Core/HUD.h"
#include "Core/SaveSystem.h"
#include "Core/BuildSystem.h"

#include "Core/Window.h"
#include "Core/Shader.h"
#include "Core/Camera.h"
#include "Core/Grid.h"
#include "Core/Primitives.h"
#include "Core/Skybox.h"
#include "Core/Model.h"
#include "Core/ScriptEditor.h"
#include "Platform/Windows/WindowsWindow.h"
#include "ECS/Scene.h"
#include "Physics/PhysicsMaterial.h"
#include "Physics/RigidbodyComponent.h"
#include "Physics/ColliderComponent.h"
#include "Physics/Physics.h"

#include <glad/glad.h>
#include "Core/TextureLoader.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>   // DockBuilder API
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

const char* vertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
out vec3 FragPos; out vec3 Normal; out vec2 TexCoord;
uniform mat4 model,view,projection;
void main(){
    FragPos=vec3(model*vec4(aPos,1.0));
    Normal=mat3(transpose(inverse(model)))*aNormal;
    TexCoord=aTexCoord;
    gl_Position=projection*view*vec4(FragPos,1.0);
})";

// ── Skinned-версия для моделей со скелетной анимацией ──
// Те же атрибуты + boneIDs/weights, вершина смешивается матрицами костей
// ДО обычной model-матрицы. Фрагментный шейдер общий с обычными объектами.
const char* vertSkinnedSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec4 aBoneIDs;   // приходят как float, приводим к int
layout(location=4) in vec4 aWeights;
out vec3 FragPos; out vec3 Normal; out vec2 TexCoord;
uniform mat4 model,view,projection;
const int MAX_BONES=100;
uniform mat4 boneMatrices[MAX_BONES];
void main(){
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    float totalWeight = 0.0;
    for(int i=0;i<4;i++){
        int id = int(aBoneIDs[i]);
        float w = aWeights[i];
        if(id<0 || w<=0.0) continue;
        mat4 bm = boneMatrices[id];
        skinnedPos += w * (bm * vec4(aPos,1.0));
        skinnedNormal += w * mat3(bm) * aNormal;
        totalWeight += w;
    }
    if(totalWeight < 0.001){ skinnedPos = vec4(aPos,1.0); skinnedNormal = aNormal; }

    FragPos = vec3(model*skinnedPos);
    Normal = mat3(transpose(inverse(model))) * skinnedNormal;
    TexCoord = aTexCoord;
    gl_Position = projection*view*vec4(FragPos,1.0);
})";
const char* fragSrc = R"(
#version 330 core
in vec3 FragPos,Normal; in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform bool useTexture;
uniform vec3 objectColor,viewPos;
uniform vec3  lightPos[8];
uniform vec3  lightColor[8];
uniform float lightIntensity[8];
uniform float lightRange[8];
uniform int   lightCount;
uniform vec3  fogColor;
uniform float fogDensity;
uniform vec3  sunDir;        // направление НА солнце (нормализовано)
uniform vec3  sunColor;      // цвет солнца (тёплый днём, тёмный ночью)
uniform float sunIntensity;  // общая яркость солнца (0 ночью)
uniform float ambientStrength;
void main(){
    vec3 baseColor = useTexture ? texture(uTexture,TexCoord).rgb * objectColor : objectColor;
    vec3 norm=normalize(Normal);
    vec3 vd=normalize(viewPos-FragPos);
    vec3 result=vec3(ambientStrength)*baseColor;

    // ── Направленный свет солнца — освещает всю сцену одинаково, как в реальности ──
    if (sunIntensity > 0.001) {
        float sunDiff = max(dot(norm, sunDir), 0.0);
        float sunSpec = pow(max(dot(vd, reflect(-sunDir,norm)),0.0), 32);
        result += (sunDiff*0.9 + sunSpec*0.25) * baseColor * sunColor * sunIntensity;
    }

    for(int i=0;i<lightCount;i++){
        vec3 ld=normalize(lightPos[i]-FragPos);
        float dist=length(lightPos[i]-FragPos);
        float att=clamp(1.0-dist/lightRange[i],0.0,1.0); att*=att;
        float diff=max(dot(norm,ld),0.0);
        float spec=pow(max(dot(vd,reflect(-ld,norm)),0.0),32);
        result+=(diff*0.8+spec*0.3)*baseColor*lightColor[i]*lightIntensity[i]*att;
    }
    if (fogDensity > 0.0001) {
        float camDist = length(viewPos-FragPos);
        float fogFactor = clamp(1.0 - exp(-camDist*fogDensity*0.04), 0.0, 1.0);
        result = mix(result, fogColor, fogFactor);
    }
    FragColor=vec4(result,1.0);
})";
const char* outlineVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 model,view,projection; uniform float outlineSize;
void main(){ gl_Position=projection*view*model*vec4(aPos+aNormal*outlineSize,1.0); })";
const char* outlineFrag = R"(
#version 330 core
out vec4 FragColor; uniform vec4 outlineColor;
void main(){ FragColor=outlineColor; })";
const char* gridVert = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model,view,projection;
void main(){ gl_Position=projection*view*model*vec4(aPos,1.0); })";
const char* gridFrag = R"(
#version 330 core
out vec4 FragColor; uniform vec3 gridColor;
void main(){ FragColor=vec4(gridColor,1.0); })";
const char* gizmoVert = R"(
#version 330 core
layout(location=0) in vec3 aPos; uniform mat4 mvp;
void main(){ gl_Position=mvp*vec4(aPos,1.0); })";
const char* gizmoFrag = R"(
#version 330 core
out vec4 FragColor; uniform vec4 color;
void main(){ FragColor=color; })";
const char* skyboxVert = R"(
#version 330 core
layout(location=0) in vec3 aPos; out vec3 TexCoords;
uniform mat4 view,projection;
void main(){ TexCoords=aPos; vec4 pos=projection*view*vec4(aPos,1.0); gl_Position=pos.xyww; })";
const char* skyboxFrag = R"(
#version 330 core
in vec3 TexCoords; out vec4 FragColor;
uniform vec3 sunDir;          // направление на солнце (нормализовано)
uniform float time;           // секунды (пока не используется, оставлено на будущее)

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453123); }

void main(){
    vec3 dir = normalize(TexCoords);

    // ── День/ночь по высоте солнца ──
    float sunH   = sunDir.y;
    float dayF   = smoothstep(-0.2, 0.25, sunH);          // 0=ночь, 1=день
    float duskF  = clamp(1.0 - abs(sunH)*2.5, 0.0, 1.0);  // пик на восходе/закате

    vec3 dayZenith    = vec3(0.20, 0.50, 0.90);
    vec3 dayHorizon   = vec3(0.65, 0.80, 0.95);
    vec3 nightZenith  = vec3(0.010,0.012,0.035);
    vec3 nightHorizon = vec3(0.030,0.035,0.070);
    vec3 duskHorizon  = vec3(1.00, 0.55, 0.28);

    vec3 zenith  = mix(nightZenith,  dayZenith,  dayF);
    vec3 horizon = mix(nightHorizon, dayHorizon, dayF);
    horizon = mix(horizon, duskHorizon, duskF*0.75);

    float h = clamp(dir.y, -1.0, 1.0);
    float horizonBlend = smoothstep(0.0, 0.55, max(h,0.0));
    vec3 skyColor = mix(horizon, zenith, horizonBlend);

    // ── Солнце ──
    float sunDot = dot(dir, normalize(sunDir));
    float sunDisc = smoothstep(0.9993, 0.9998, sunDot);
    float sunGlow = pow(max(sunDot,0.0), 26.0) * 0.55;
    vec3 sunColor = mix(vec3(1.0,0.65,0.35), vec3(1.0,0.97,0.85), dayF);
    skyColor += (sunDisc*1.4 + sunGlow) * sunColor * step(-0.05, sunH);

    // ── Луна (противоположна солнцу, видна ночью) ──
    vec3 moonDir = -normalize(sunDir);
    float moonDot = dot(dir, moonDir);
    float moonDisc = smoothstep(0.9990, 0.9996, moonDot);
    float moonGlow = pow(max(moonDot,0.0), 40.0) * 0.15;
    skyColor += (moonDisc + moonGlow) * vec3(0.85,0.87,1.0) * (1.0-dayF);

    // ── Звёзды ночью — маленькие круглые точки, не целые ячейки ──
    float lon = atan(dir.z, dir.x);           // -pi..pi
    float lat = asin(clamp(dir.y,-1.0,1.0));  // -pi/2..pi/2
    vec2 starUV = vec2(lon, lat) * 120.0;
    vec2 starCell = floor(starUV);
    vec2 starLocal = fract(starUV) - 0.5;     // позиция внутри ячейки, центр = (0,0)
    float starPick = hash(starCell);
    float hasStar  = step(0.985, starPick);
    // случайное смещение точки внутри ячейки, чтобы не были строго по сетке
    vec2 starOffset = vec2(hash(starCell+vec2(3.1,1.7)), hash(starCell+vec2(7.2,9.4))) - 0.5;
    float starDist  = length(starLocal - starOffset*0.4);
    float starDot   = smoothstep(0.16, 0.0, starDist) * hasStar;
    float twinkle   = 0.6 + 0.4*hash(starCell+vec2(time*0.0001,0.0));
    float stars = starDot * twinkle * (1.0-dayF) * smoothstep(0.0,0.3,h);
    skyColor += stars;

    FragColor = vec4(skyColor,1.0);
})";

enum class PrimitiveType { Cube, Sphere, Cylinder, Pyramid, Capsule, Plane, Model3D };
enum class GizmoMode { Select, Move, Rotate, Scale };
enum class GizmoAxis { None, X, Y, Z };
enum class SelectionType { None, Object, Light, Camera, Environment };

struct LightObject {
    std::string name; glm::vec3 pos={0,3,0},color={1,1,1};
    float intensity=1.f,range=10.f; bool active=true;
    VE::EntityID ecsID=VE::NULL_ENTITY;
};
struct CameraObject {
    std::string name="GameCamera"; glm::vec3 pos={0,2,5},rot={0,0,0};
    float fov=45.f; bool active=true,isPrimary=true;
    VE::EntityID ecsID=VE::NULL_ENTITY;
    int followTargetIndex=-1;            // индекс в objects[], -1 = свободный полёт
    glm::vec3 followOffset={0,1.6f,0};   // смещение от followTarget (высота глаз)
};
// ── Material: цвет + текстура + параметры поверхности ──
struct Material {
    std::string name = "Material";
    glm::vec3   color = {1.f,1.f,1.f};
    std::string texturePath;
    GLuint      textureID = 0;
    float       roughness = 0.5f;   // 0=зеркало, 1=матовый
    float       metallic  = 0.0f;   // 0=диэлектрик, 1=металл
    float       tilingX = 1.f, tilingY = 1.f; // повтор текстуры
    std::string assetPath; // путь к .mat файлу на диске (если материал сохранён как ассет)
};

// ── Сохранить материал в .mat файл (простой текстовый формат) ──
inline void SaveMaterial(const std::string& path, const Material& m) {
    std::ofstream f(path);
    f << "name=" << m.name << "\n";
    f << "color=" << m.color.x << "," << m.color.y << "," << m.color.z << "\n";
    f << "texture=" << m.texturePath << "\n";
    f << "roughness=" << m.roughness << "\n";
    f << "metallic=" << m.metallic << "\n";
    f << "tilingX=" << m.tilingX << "\n";
    f << "tilingY=" << m.tilingY << "\n";
}

// ── Загрузить материал из .mat файла ──
inline Material LoadMaterial(const std::string& path) {
    Material m;
    m.assetPath = path;
    m.name = fs::path(path).stem().string();
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        if (key=="name") m.name = val;
        else if (key=="color") {
            sscanf(val.c_str(), "%f,%f,%f", &m.color.x, &m.color.y, &m.color.z);
        }
        else if (key=="texture") {
            m.texturePath = val;
            if (!val.empty() && fs::exists(val)) m.textureID = VE::LoadTexture(val);
        }
        else if (key=="roughness") m.roughness = std::stof(val);
        else if (key=="metallic")  m.metallic  = std::stof(val);
        else if (key=="tilingX")   m.tilingX   = std::stof(val);
        else if (key=="tilingY")   m.tilingY   = std::stof(val);
    }
    return m;
}

// ═══════════════════════════════════════════════════════
//   КАСТОМНАЯ ПОКАДРОВАЯ АНИМАЦИЯ ОБЪЕКТА (как Animation window в Unity)
//   Работает с ЛЮБЫМ объектом (куб, сфера, свет...) — двигает/крутит/
//   масштабирует его по ключевым кадрам. Независима от скелетной
//   анимации импортированных моделей (та живёт в Model.h).
// ═══════════════════════════════════════════════════════
struct ObjectKeyframe {
    float time = 0.f; // секунды от начала клипа
    glm::vec3 pos{0}, rot{0}, scale{1};
};
struct ObjectAnimClip {
    std::string name = "Clip";
    std::vector<ObjectKeyframe> keys;
    bool loop = true;
};

// Линейная интерполяция позы объекта в момент t внутри клипа.
// Кадры должны быть отсортированы по времени (сортируем при добавлении).
void SampleObjectClip(ObjectAnimClip& clip, float t, glm::vec3& outPos, glm::vec3& outRot, glm::vec3& outScale) {
    if (clip.keys.empty()) { outPos=glm::vec3(0); outRot=glm::vec3(0); outScale=glm::vec3(1); return; }
    if (clip.keys.size()==1) { outPos=clip.keys[0].pos; outRot=clip.keys[0].rot; outScale=clip.keys[0].scale; return; }
    if (t <= clip.keys.front().time) { outPos=clip.keys.front().pos; outRot=clip.keys.front().rot; outScale=clip.keys.front().scale; return; }
    if (t >= clip.keys.back().time)  { outPos=clip.keys.back().pos;  outRot=clip.keys.back().rot;  outScale=clip.keys.back().scale;  return; }
    for (size_t i=0;i+1<clip.keys.size();i++) {
        if (t >= clip.keys[i].time && t <= clip.keys[i+1].time) {
            float span = clip.keys[i+1].time - clip.keys[i].time;
            float f = span>0.f ? (t-clip.keys[i].time)/span : 0.f;
            outPos   = glm::mix(clip.keys[i].pos,   clip.keys[i+1].pos,   f);
            outRot   = glm::mix(clip.keys[i].rot,   clip.keys[i+1].rot,   f);
            outScale = glm::mix(clip.keys[i].scale, clip.keys[i+1].scale, f);
            return;
        }
    }
}

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

VE::Camera camera(glm::vec3(0,3,8));
VE::Camera gameCamera(glm::vec3(0,2,5));
float lastX=640,lastY=360,deltaTime=0,lastFrame=0;
bool firstMouse=true,rightMouseDown=false;
bool leftDown=false,leftClickThisFrame=false;
double clickX=0,clickY=0,mouseX=0,mouseY=0;
static int g_DragHoverObj = -1; // объект под курсором во время drag&drop
double g_RawMouseDX=0,g_RawMouseDY=0; // дельта мыши за кадр, для FPS-камеры из Lua
double g_LastRawMouseX=0,g_LastRawMouseY=0; bool g_RawMouseFirst=true;
bool g_MouseCaptured=false; // курсор спрятан и зациклен (FPS look) — клик по Game / Esc переключают
GizmoMode gizmoMode=GizmoMode::Move;
GizmoAxis dragAxis=GizmoAxis::None;
glm::vec3 dragStartPos,dragStartRot,dragStartScale,dragStartHit;
bool isPlaying=false,isPaused=false;

// ═══════════════════════════════════════════════════════
//   ENVIRONMENT — процедурное небо (время суток, облака)
// ═══════════════════════════════════════════════════════
float g_TimeOfDay   = 12.0f;  // часы, 0..24 (6=рассвет, 12=полдень, 18=закат, 0/24=полночь)
float g_FogDensity = 0.0f;                          // 0..1 — плотность тумана (0 = выкл)
glm::vec3 g_FogColor = glm::vec3(0.6f,0.65f,0.7f);  // цвет тумана
float g_SunIntensity   = 1.0f;   // множитель яркости солнца (0 = выключить)
float g_AmbientStrength = 0.12f; // фоновая подсветка (чтобы тени не были чёрными)
std::vector<SceneObject>* g_LuaObjectsPtr = nullptr; // указывает на objects[] из main(), для Animation API

glm::vec3 ComputeSunDir(float timeOfDay){
    float frac  = timeOfDay/24.0f;
    float angle = frac*2.0f*3.14159265f - 3.14159265f*0.5f;
    return glm::normalize(glm::vec3(cosf(angle), sinf(angle), 0.25f));
}

void drawProceduralSky(unsigned int shaderID, const glm::mat4& view, const glm::mat4& proj,
                        unsigned int skyVAO, glm::vec3 sunDir, float timeSec)
{
    glDepthFunc(GL_LEQUAL);
    glUseProgram(shaderID);
    glm::mat4 skyView = glm::mat4(glm::mat3(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderID,"view"),1,GL_FALSE,glm::value_ptr(skyView));
    glUniformMatrix4fv(glGetUniformLocation(shaderID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(shaderID,"sunDir"),sunDir.x,sunDir.y,sunDir.z);
    glUniform1f(glGetUniformLocation(shaderID,"time"),timeSec);
    glBindVertexArray(skyVAO);
    glDrawArrays(GL_TRIANGLES,0,36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}
std::vector<std::string> g_DroppedFiles; // пути файлов, перетащенных из Windows в окно движка
std::vector<SavedTransform> savedTransforms;
ImVec2 g_VpPos(0,0),g_VpSize(940,600);

// ═══════════════════════════════════════════════════════
//   EDITOR PREFERENCES — как EditorSettings в Godot
// ═══════════════════════════════════════════════════════
struct EditorPrefs {
    // General
    bool  autosaveEnabled   = true;
    float autosaveMinutes   = 5.0f;
    std::string defaultProjectPath = "";
    // Interface
    ImVec4 accentColor = ImVec4(0.28f,0.16f,0.50f,1.f);
    float  uiScale = 1.0f;
    // Viewport
    float camSpeed       = 5.0f;
    float camSensitivity = 0.1f;
    bool  invertY         = false;

    static std::string PrefsPath(){ return "editor_prefs.cfg"; }

    void Save(){
        std::ofstream f(PrefsPath());
        if(!f) return;
        f << "autosaveEnabled=" << (autosaveEnabled?1:0) << "\n";
        f << "autosaveMinutes=" << autosaveMinutes << "\n";
        f << "defaultProjectPath=" << defaultProjectPath << "\n";
        f << "accentR=" << accentColor.x << "\n";
        f << "accentG=" << accentColor.y << "\n";
        f << "accentB=" << accentColor.z << "\n";
        f << "uiScale=" << uiScale << "\n";
        f << "camSpeed=" << camSpeed << "\n";
        f << "camSensitivity=" << camSensitivity << "\n";
        f << "invertY=" << (invertY?1:0) << "\n";
    }

    void Load(){
        std::ifstream f(PrefsPath());
        if(!f) return;
        std::string line;
        while(std::getline(f,line)){
            auto eq=line.find('=');
            if(eq==std::string::npos) continue;
            std::string key=line.substr(0,eq), val=line.substr(eq+1);
            if(key=="autosaveEnabled") autosaveEnabled = (val=="1");
            else if(key=="autosaveMinutes") autosaveMinutes = std::stof(val);
            else if(key=="defaultProjectPath") defaultProjectPath = val;
            else if(key=="accentR") accentColor.x = std::stof(val);
            else if(key=="accentG") accentColor.y = std::stof(val);
            else if(key=="accentB") accentColor.z = std::stof(val);
            else if(key=="uiScale") uiScale = std::stof(val);
            else if(key=="camSpeed") camSpeed = std::stof(val);
            else if(key=="camSensitivity") camSensitivity = std::stof(val);
            else if(key=="invertY") invertY = (val=="1");
        }
    }
};
EditorPrefs g_Prefs;
bool g_ShowPreferences = false;

// ── Player mode: движок запущен как самостоятельная игра (без редактора) ──
// Активируется аргументом командной строки: VisualEngine.exe --play "Assets/scene.vescene"
bool g_PlayerMode = false;
std::string g_PlayerScenePath = "";
std::string g_OverrideProjectRoot = ""; // передаётся из VisualHub через --project
bool g_PlayerAutoPlayPending = false;

GLFWcursorposfun g_PrevCursorPosCallback = nullptr; // коллбэк ImGui, который нужно пробросить дальше

void mouse_callback(GLFWwindow* w,double x,double y){
    mouseX=x;mouseY=y;
    // Сырая дельта для Lua (НЕ зависит от rightMouseDown — нужен всегда в Play)
    if(g_RawMouseFirst){g_LastRawMouseX=x;g_LastRawMouseY=y;g_RawMouseFirst=false;}
    g_RawMouseDX=x-g_LastRawMouseX; g_RawMouseDY=y-g_LastRawMouseY;
    g_LastRawMouseX=x; g_LastRawMouseY=y;
    // Пробрасываем событие в ImGui — иначе его собственная обработка мыши сломается
    if(g_PrevCursorPosCallback) g_PrevCursorPosCallback(w,x,y);
    if(!rightMouseDown)return;
    if(firstMouse){lastX=x;lastY=y;firstMouse=false;}
    camera.ProcessMouse(x-lastX,lastY-y);lastX=x;lastY=y;
}
void mouse_button_callback(GLFWwindow* w,int btn,int action,int){
    if(btn==GLFW_MOUSE_BUTTON_RIGHT){rightMouseDown=(action==GLFW_PRESS);firstMouse=true;}
    if(btn==GLFW_MOUSE_BUTTON_LEFT){
        if(action==GLFW_PRESS){leftDown=true;leftClickThisFrame=true;double cx,cy;glfwGetCursorPos(w,&cx,&cy);clickX=cx;clickY=cy;}
        else{leftDown=false;dragAxis=GizmoAxis::None;}
    }
}

unsigned int setupCubeVAO(){
    float v[]={
        -0.5f,-0.5f,-0.5f,0,0,-1, 0,0,  0.5f,-0.5f,-0.5f,0,0,-1, 1,0,  0.5f,0.5f,-0.5f,0,0,-1, 1,1,  -0.5f,0.5f,-0.5f,0,0,-1, 0,1,
        -0.5f,-0.5f,0.5f,0,0,1, 0,0,  0.5f,-0.5f,0.5f,0,0,1, 1,0,  0.5f,0.5f,0.5f,0,0,1, 1,1,  -0.5f,0.5f,0.5f,0,0,1, 0,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,0,  -0.5f,0.5f,-0.5f,-1,0,0, 1,0,  -0.5f,0.5f,0.5f,-1,0,0, 1,1,  -0.5f,-0.5f,0.5f,-1,0,0, 0,1,
        0.5f,-0.5f,-0.5f,1,0,0, 0,0,  0.5f,0.5f,-0.5f,1,0,0, 1,0,  0.5f,0.5f,0.5f,1,0,0, 1,1,  0.5f,-0.5f,0.5f,1,0,0, 0,1,
        -0.5f,-0.5f,-0.5f,0,-1,0, 0,0,  0.5f,-0.5f,-0.5f,0,-1,0, 1,0,  0.5f,-0.5f,0.5f,0,-1,0, 1,1,  -0.5f,-0.5f,0.5f,0,-1,0, 0,1,
        -0.5f,0.5f,-0.5f,0,1,0, 0,0,  0.5f,0.5f,-0.5f,0,1,0, 1,0,  0.5f,0.5f,0.5f,0,1,0, 1,1,  -0.5f,0.5f,0.5f,0,1,0, 0,1
    };
    unsigned int idx[]={0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
    unsigned int VAO,VBO,EBO;
    glGenVertexArrays(1,&VAO);glGenBuffers(1,&VBO);glGenBuffers(1,&EBO);glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));glEnableVertexAttribArray(2);
    glBindVertexArray(0);return VAO;
}
unsigned int buildArrowVAO(int& cnt){
    std::vector<float> v;v.insert(v.end(),{0,0,0,0,0.65f,0});
    for(int i=0;i<12;i++){float a=2*3.14159f*i/12,b=2*3.14159f*(i+1)/12;v.insert(v.end(),{0.04f*cos(a),0.65f,0.04f*sin(a),0.04f*cos(b),0.65f,0.04f*sin(b),0,1,0});}
    cnt=v.size()/3;unsigned int VAO,VBO;
    glGenVertexArrays(1,&VAO);glGenBuffers(1,&VBO);glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glBindVertexArray(0);return VAO;
}
void drawMesh(const SceneObject& obj,unsigned int cubeVAO,VE::Mesh& sph,VE::Mesh& cyl,VE::Mesh& pyr,VE::Mesh& cap,VE::Mesh& pln){
    switch(obj.type){
        case PrimitiveType::Cube:     glBindVertexArray(cubeVAO);glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Sphere:   glBindVertexArray(sph.VAO);glDrawElements(GL_TRIANGLES,sph.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Cylinder: glBindVertexArray(cyl.VAO);glDrawElements(GL_TRIANGLES,cyl.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Pyramid:  glBindVertexArray(pyr.VAO);glDrawElements(GL_TRIANGLES,pyr.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Capsule:  glBindVertexArray(cap.VAO);glDrawElements(GL_TRIANGLES,cap.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Plane:    glBindVertexArray(pln.VAO);glDrawElements(GL_TRIANGLES,pln.indexCount,GL_UNSIGNED_INT,0);break;
        case PrimitiveType::Model3D:  if(obj.model&&obj.model->loaded)obj.model->Draw();break;
    }
}
void openInVSCode(const std::string& path){
    std::string cmd="start \"\" code \""+path+"\"";
    system(cmd.c_str());
}

VE::Scene scene;
void addObject(std::vector<SceneObject>& objects,PrimitiveType type,int& sel,SelectionType& selType){
    const char* n[]={"Cube","Sphere","Cylinder","Pyramid","Capsule","Plane","Model"};
    SceneObject o;
    o.name=std::string(n[(int)type])+"_"+std::to_string(objects.size()+1);
    o.pos=glm::vec3(0,0.5f,0);o.type=type;o.color=glm::vec3(0.4f,0.6f,0.9f);
    o.ecsID=scene.CreateEntity(o.name);
    scene.GetTransform(o.ecsID).Position=o.pos;
    scene.GetTransform(o.ecsID).Scale=o.scale;
    scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
    objects.push_back(o);sel=(int)objects.size()-1;selType=SelectionType::Object;
    logInfo("Created "+o.name);
}

void drawRing(unsigned int sid,glm::vec3 center,int axis,float gs,glm::vec4 col,const glm::mat4& vp){
    const int SEG=64; std::vector<float> pts;
    for(int j=0;j<=SEG;j++){float a=2.f*3.14159f*j/SEG,cs=cos(a)*gs,sn=sin(a)*gs;
        if(axis==0) pts.insert(pts.end(),{0,cs,sn});
        else if(axis==1) pts.insert(pts.end(),{cs,0,sn});
        else pts.insert(pts.end(),{cs,sn,0});}
    unsigned int rVAO,rVBO;glGenVertexArrays(1,&rVAO);glGenBuffers(1,&rVBO);
    glBindVertexArray(rVAO);glBindBuffer(GL_ARRAY_BUFFER,rVBO);
    glBufferData(GL_ARRAY_BUFFER,pts.size()*sizeof(float),pts.data(),GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);glEnableVertexAttribArray(0);
    glm::mat4 m=glm::translate(glm::mat4(1),center);
    glUniformMatrix4fv(glGetUniformLocation(sid,"mvp"),1,GL_FALSE,glm::value_ptr(vp*m));
    glUniform4f(glGetUniformLocation(sid,"color"),col.r,col.g,col.b,col.a);
    glLineWidth(2.5f);glDrawArrays(GL_LINE_STRIP,0,SEG+1);
    glDeleteVertexArrays(1,&rVAO);glDeleteBuffers(1,&rVBO);
}
// Рисует значок камеры или света как billboard, повёрнутый к камере.
// Вместо плоского цветного квадрата — узнаваемая иконка на тёмной круглой подложке.
void drawBillboard(unsigned int sid,glm::vec3 pos,glm::vec4 col,float size,const glm::mat4& view,const glm::mat4& proj,bool isLight){
    glm::vec3 right=glm::normalize(glm::vec3(view[0][0],view[1][0],view[2][0]));
    glm::vec3 up=glm::normalize(glm::vec3(view[0][1],view[1][1],view[2][1]));
    float R=size*0.5f;

    auto bp=[&](float x,float y)->glm::vec3{ return pos+right*(x*R)+up*(y*R); };

    unsigned int vao=0,vbo=0;
    glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);
    glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glUniformMatrix4fv(glGetUniformLocation(sid,"mvp"),1,GL_FALSE,glm::value_ptr(proj*view*glm::mat4(1)));

    auto upload=[&](const std::vector<float>& v){
        glBufferData(GL_ARRAY_BUFFER,v.size()*sizeof(float),v.data(),GL_DYNAMIC_DRAW);
    };
    auto setColor=[&](glm::vec4 c){
        glUniform4f(glGetUniformLocation(sid,"color"),c.r,c.g,c.b,c.a);
    };

    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    // ── 1. Тёмная круглая подложка для контраста на любом фоне ──
    {
        const int SEG=20;
        std::vector<float> f; f.reserve((SEG+2)*3);
        glm::vec3 c0=pos; f.insert(f.end(),{c0.x,c0.y,c0.z});
        for(int i=0;i<=SEG;i++){
            float a=2.f*3.14159265f*(float)i/SEG;
            glm::vec3 v=bp(cosf(a),sinf(a));
            f.insert(f.end(),{v.x,v.y,v.z});
        }
        upload(f); setColor(glm::vec4(0.03f,0.03f,0.04f,0.55f));
        glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));
    }

    if(isLight){
        // ── 2a. Лампочка: кружок-«голова» + основание + лучи ──
        {
            const int SEG=14;
            std::vector<float> f; f.reserve((SEG+2)*3);
            glm::vec3 c0=bp(0,0.10f); f.insert(f.end(),{c0.x,c0.y,c0.z});
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(cosf(a)*0.34f, 0.10f+sinf(a)*0.34f);
                f.insert(f.end(),{v.x,v.y,v.z});
            }
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));
        }
        {
            glm::vec3 a=bp(-0.10f,-0.24f),b=bp(0.10f,-0.24f),c=bp(0.10f,-0.44f),d=bp(-0.10f,-0.44f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col*0.65f+glm::vec4(0,0,0,0.35f));
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            std::vector<float> f;
            for(int i=0;i<6;i++){
                float a=2.f*3.14159265f*((float)i/6.f)+0.5f;
                glm::vec3 p0=bp(cosf(a)*0.42f, 0.10f+sinf(a)*0.42f);
                glm::vec3 p1=bp(cosf(a)*0.62f, 0.10f+sinf(a)*0.62f);
                f.insert(f.end(),{p0.x,p0.y,p0.z,p1.x,p1.y,p1.z});
            }
            upload(f); setColor(col);
            glLineWidth(2.f);
            glDrawArrays(GL_LINES,0,(GLsizei)(f.size()/3));
        }
    } else {
        // ── 2b. Камера: корпус + объектив + видоискатель ──
        {
            glm::vec3 a=bp(-0.44f,-0.20f),b=bp(0.30f,-0.20f),c=bp(0.30f,0.20f),d=bp(-0.44f,0.20f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            glm::vec3 a=bp(-0.16f,0.20f),b=bp(0.06f,0.20f),c=bp(0.06f,0.36f),d=bp(-0.16f,0.36f);
            std::vector<float> f={a.x,a.y,a.z,b.x,b.y,b.z,c.x,c.y,c.z, c.x,c.y,c.z,d.x,d.y,d.z,a.x,a.y,a.z};
            upload(f); setColor(col);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        {
            const int SEG=16;
            std::vector<float> f; f.reserve((SEG+2)*3);
            glm::vec3 c0=bp(0.30f,0.0f); f.insert(f.end(),{c0.x,c0.y,c0.z});
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(0.30f+cosf(a)*0.20f, sinf(a)*0.20f);
                f.insert(f.end(),{v.x,v.y,v.z});
            }
            upload(f); setColor(glm::vec4(0.04f,0.04f,0.05f,1.f));
            glDrawArrays(GL_TRIANGLE_FAN,0,(GLsizei)(f.size()/3));

            std::vector<float> ring; ring.reserve((SEG+1)*3);
            for(int i=0;i<=SEG;i++){
                float a=2.f*3.14159265f*(float)i/SEG;
                glm::vec3 v=bp(0.30f+cosf(a)*0.20f, sinf(a)*0.20f);
                ring.insert(ring.end(),{v.x,v.y,v.z});
            }
            upload(ring); setColor(col);
            glLineWidth(1.5f);
            glDrawArrays(GL_LINE_LOOP,0,(GLsizei)(ring.size()/3));
        }
    }

    // ── 3. Тонкий контур подложки поверх всего — чтобы значок не сливался с фоном ──
    {
        const int SEG=20;
        std::vector<float> f; f.reserve((SEG+1)*3);
        for(int i=0;i<=SEG;i++){
            float a=2.f*3.14159265f*(float)i/SEG;
            glm::vec3 v=bp(cosf(a),sinf(a));
            f.insert(f.end(),{v.x,v.y,v.z});
        }
        upload(f); setColor(glm::vec4(col.r,col.g,col.b,0.55f));
        glLineWidth(1.2f);
        glDrawArrays(GL_LINE_LOOP,0,(GLsizei)(f.size()/3));
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDeleteVertexArrays(1,&vao);glDeleteBuffers(1,&vbo);
}

void renderScene(std::vector<SceneObject>& objects,int sel,bool isGameView,
    VE::Shader& shader,VE::Shader& skinnedShader,VE::Shader& outlineShader,VE::Shader& gridShader,
    VE::Shader& gizmoShader,VE::Shader& skyboxShader,
    VE::Skybox& skybox,VE::Grid& grid,
    unsigned int cubeVAO,VE::Mesh& sph,VE::Mesh& cyl,VE::Mesh& pyr,VE::Mesh& cap,VE::Mesh& pln,
    unsigned int arrowVAO,int arrowCnt,
    VE::Camera& cam,float aspect,GizmoMode gizmoMode,GizmoAxis dragAxis,
    bool showSkybox,bool showGrid,bool showGizmos,float gs,
    std::vector<LightObject>& lights,std::vector<CameraObject>& sceneCameras,
    int selLight,int selCamera,SelectionType selType,int excludeIndex=-1)
{
    glClearColor(0.10f,0.10f,0.13f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    glm::mat4 view=cam.GetViewMatrix(),proj=cam.GetProjectionMatrix(aspect),vp=proj*view;
    if(showSkybox) drawProceduralSky(skyboxShader.ID,view,proj,skybox.VAO,ComputeSunDir(g_TimeOfDay),(float)glfwGetTime());
    if(showGrid&&!isGameView) grid.Draw(gridShader.ID,view,proj);

    // ── Направленный свет солнца: направление/цвет/яркость зависят от времени суток ──
    glm::vec3 sunDir = ComputeSunDir(g_TimeOfDay);
    float sunH = sunDir.y;
    float sunDayF = glm::clamp((sunH+0.20f)/0.45f, 0.f, 1.f);
    glm::vec3 sunCol = glm::mix(glm::vec3(0.05f,0.06f,0.12f), glm::vec3(1.0f,0.95f,0.85f), sunDayF);
    float sunFinalIntensity = sunDayF * g_SunIntensity;

    shader.Use();
    glUniformMatrix4fv(glGetUniformLocation(shader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(shader.ID,"viewPos"),cam.Position.x,cam.Position.y,cam.Position.z);
    glUniform3f(glGetUniformLocation(shader.ID,"fogColor"),g_FogColor.x,g_FogColor.y,g_FogColor.z);
    glUniform1f(glGetUniformLocation(shader.ID,"fogDensity"),g_FogDensity);
    glUniform3f(glGetUniformLocation(shader.ID,"sunDir"),sunDir.x,sunDir.y,sunDir.z);
    glUniform3f(glGetUniformLocation(shader.ID,"sunColor"),sunCol.x,sunCol.y,sunCol.z);
    glUniform1f(glGetUniformLocation(shader.ID,"sunIntensity"),sunFinalIntensity);
    glUniform1f(glGetUniformLocation(shader.ID,"ambientStrength"),g_AmbientStrength);
    int lCount=(int)std::min(lights.size(),(size_t)8);
    glUniform1i(glGetUniformLocation(shader.ID,"lightCount"),lCount);
    for(int i=0;i<lCount;i++){
        std::string idx="["+std::to_string(i)+"]";
        glUniform3f(glGetUniformLocation(shader.ID,("lightPos"+idx).c_str()),lights[i].pos.x,lights[i].pos.y,lights[i].pos.z);
        glUniform3f(glGetUniformLocation(shader.ID,("lightColor"+idx).c_str()),lights[i].color.r,lights[i].color.g,lights[i].color.b);
        glUniform1f(glGetUniformLocation(shader.ID,("lightIntensity"+idx).c_str()),lights[i].intensity);
        glUniform1f(glGetUniformLocation(shader.ID,("lightRange"+idx).c_str()),lights[i].range);
    }
    glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);

    // ── Те же общие uniform'ы (вид/проекция/свет/туман) настраиваем и на skinned-шейдере ──
    skinnedShader.Use();
    glUniformMatrix4fv(glGetUniformLocation(skinnedShader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(skinnedShader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"viewPos"),cam.Position.x,cam.Position.y,cam.Position.z);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"sunDir"),sunDir.x,sunDir.y,sunDir.z);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"sunColor"),sunCol.x,sunCol.y,sunCol.z);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"sunIntensity"),sunFinalIntensity);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"ambientStrength"),g_AmbientStrength);
    glUniform3f(glGetUniformLocation(skinnedShader.ID,"fogColor"),g_FogColor.x,g_FogColor.y,g_FogColor.z);
    glUniform1f(glGetUniformLocation(skinnedShader.ID,"fogDensity"),g_FogDensity);
    glUniform1i(glGetUniformLocation(skinnedShader.ID,"lightCount"),lCount);
    for(int i=0;i<lCount;i++){
        std::string idx="["+std::to_string(i)+"]";
        glUniform3f(glGetUniformLocation(skinnedShader.ID,("lightPos"+idx).c_str()),lights[i].pos.x,lights[i].pos.y,lights[i].pos.z);
        glUniform3f(glGetUniformLocation(skinnedShader.ID,("lightColor"+idx).c_str()),lights[i].color.r,lights[i].color.g,lights[i].color.b);
        glUniform1f(glGetUniformLocation(skinnedShader.ID,("lightIntensity"+idx).c_str()),lights[i].intensity);
        glUniform1f(glGetUniformLocation(skinnedShader.ID,("lightRange"+idx).c_str()),lights[i].range);
    }
    shader.Use();
    for(int i=0;i<(int)objects.size();i++){
        if(!objects[i].active) continue;
        if(i==excludeIndex) continue; // своё тело не рисуем от первого лица
        auto& obj=objects[i];
        if(scene.IsAlive(obj.ecsID)){auto& t=scene.GetTransform(obj.ecsID);t.Position=obj.pos;t.Rotation=obj.rot;t.Scale=obj.scale;}
        bool isSel=(selType==SelectionType::Object&&i==sel);
        if(!isGameView&&isSel){glStencilFunc(GL_ALWAYS,1,0xFF);glStencilMask(0xFF);}
        else{glStencilFunc(GL_ALWAYS,0,0xFF);glStencilMask(0x00);}
        glm::mat4 model=glm::mat4(1);
        model=glm::translate(model,obj.pos);
        model=glm::rotate(model,glm::radians(obj.rot.x),glm::vec3(1,0,0));
        model=glm::rotate(model,glm::radians(obj.rot.y),glm::vec3(0,1,0));
        model=glm::rotate(model,glm::radians(obj.rot.z),glm::vec3(0,0,1));
        model=glm::scale(model,obj.scale);

        bool useSkinning = (obj.type==PrimitiveType::Model3D && obj.model && obj.model->hasSkeleton && obj.animIndex>=0);
        VE::Shader& activeShader = useSkinning ? skinnedShader : shader;
        activeShader.Use();

        if (useSkinning) {
            auto boneMats = obj.model->GetBoneMatrices(obj.animIndex, obj.animTime, obj.animLoop);
            int n = std::min((int)boneMats.size(), VE::MAX_BONES);
            for (int b=0;b<n;b++) {
                std::string u = "boneMatrices["+std::to_string(b)+"]";
                glUniformMatrix4fv(glGetUniformLocation(activeShader.ID,u.c_str()),1,GL_FALSE,glm::value_ptr(boneMats[b]));
            }
        }

        glUniformMatrix4fv(glGetUniformLocation(activeShader.ID,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(activeShader.ID,"objectColor"),obj.color.r,obj.color.g,obj.color.b);
        GLuint texToBind=(obj.textureID!=0)?obj.textureID:VE::GetWhiteTexture();
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texToBind);
        glUniform1i(glGetUniformLocation(activeShader.ID,"uTexture"),0);
        glUniform1i(glGetUniformLocation(activeShader.ID,"useTexture"),obj.textureID!=0);
        drawMesh(obj,cubeVAO,sph,cyl,pyr,cap,pln);
        if (useSkinning) shader.Use(); // возвращаем основной шейдер для следующих объектов
    }
    if(!isGameView&&selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()){
        glStencilFunc(GL_NOTEQUAL,1,0xFF);glStencilMask(0x00);glDisable(GL_DEPTH_TEST);
        auto& obj=objects[sel];
        glm::mat4 model=glm::translate(glm::mat4(1),obj.pos);
        model=glm::rotate(model,glm::radians(obj.rot.x),glm::vec3(1,0,0));
        model=glm::rotate(model,glm::radians(obj.rot.y),glm::vec3(0,1,0));
        model=glm::rotate(model,glm::radians(obj.rot.z),glm::vec3(0,0,1));
        model=glm::scale(model,obj.scale);
        outlineShader.Use();
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"model"),1,GL_FALSE,glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(outlineShader.ID,"projection"),1,GL_FALSE,glm::value_ptr(proj));
        glUniform1f(glGetUniformLocation(outlineShader.ID,"outlineSize"),0.012f);
        glUniform4f(glGetUniformLocation(outlineShader.ID,"outlineColor"),.35f,.65f,1,1);
        drawMesh(obj,cubeVAO,sph,cyl,pyr,cap,pln);
        glStencilMask(0xFF);glStencilFunc(GL_ALWAYS,0,0xFF);glEnable(GL_DEPTH_TEST);
    }
    if(!isGameView&&showGizmos){
        glClear(GL_DEPTH_BUFFER_BIT);
        gizmoShader.Use();
        for(int i=0;i<(int)lights.size();i++){
            bool isSel=(selType==SelectionType::Light&&i==selLight);
            float d=glm::length(lights[i].pos-cam.Position);
            float iconSize=glm::clamp(d*0.10f,0.28f,1.4f);
            drawBillboard(gizmoShader.ID,lights[i].pos,isSel?glm::vec4(1,1,0,1):glm::vec4(1,.85f,.1f,1),iconSize,view,proj,true);
            if(isSel) drawRing(gizmoShader.ID,lights[i].pos,1,lights[i].range,glm::vec4(1,.85f,.1f,.4f),vp);
        }
        for(int i=0;i<(int)sceneCameras.size();i++){
            bool isSel=(selType==SelectionType::Camera&&i==selCamera);
            glm::vec3 camWorldPos = sceneCameras[i].pos;
            if(sceneCameras[i].followTargetIndex>=0 && sceneCameras[i].followTargetIndex<(int)objects.size())
                camWorldPos = objects[sceneCameras[i].followTargetIndex].pos + sceneCameras[i].followOffset;
            float d=glm::length(camWorldPos-cam.Position);
            float iconSize=glm::clamp(d*0.10f,0.28f,1.4f);
            drawBillboard(gizmoShader.ID,camWorldPos,isSel?glm::vec4(.3f,.9f,1,1):glm::vec4(.2f,.7f,1,1),iconSize,view,proj,false);
        }
        glm::vec3 gPos(0);bool showGiz=false;
        if(selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()){gPos=objects[sel].pos;showGiz=true;}
        else if(selType==SelectionType::Light&&selLight>=0&&selLight<(int)lights.size()){gPos=lights[selLight].pos;showGiz=true;}
        else if(selType==SelectionType::Camera&&selCamera>=0&&selCamera<(int)sceneCameras.size()){
            auto& sc=sceneCameras[selCamera];
            gPos = (sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size())
                 ? objects[sc.followTargetIndex].pos + sc.followOffset
                 : sc.pos;
            showGiz=true;
        }
        if(showGiz&&gizmoMode!=GizmoMode::Select){
            if(gizmoMode==GizmoMode::Move||gizmoMode==GizmoMode::Scale){
                glBindVertexArray(arrowVAO);
                glm::vec4 cols[3]={glm::vec4(1,.15f,.15f,1),glm::vec4(.15f,1,.15f,1),glm::vec4(.15f,.4f,1,1)};
                float rA[3]={-90,0,90};glm::vec3 rX[3]={glm::vec3(0,0,1),glm::vec3(0,1,0),glm::vec3(1,0,0)};
                for(int i=0;i<3;i++){
                    glm::vec4 col=dragAxis==(GizmoAxis)(i+1)?glm::vec4(1,1,.2f,1):cols[i];
                    glm::mat4 m=glm::translate(glm::mat4(1),gPos);
                    m=glm::rotate(m,glm::radians(rA[i]),rX[i]);m=glm::scale(m,glm::vec3(gs));
                    glUniformMatrix4fv(glGetUniformLocation(gizmoShader.ID,"mvp"),1,GL_FALSE,glm::value_ptr(vp*m));
                    glUniform4f(glGetUniformLocation(gizmoShader.ID,"color"),col.r,col.g,col.b,col.a);
                    glDrawArrays(GL_TRIANGLES,2,arrowCnt-2);glLineWidth(2.f);glDrawArrays(GL_LINES,0,2);
                }
            } else if(gizmoMode==GizmoMode::Rotate){
                glm::vec4 rc[3]={glm::vec4(1,.15f,.15f,1),glm::vec4(.15f,1,.15f,1),glm::vec4(.15f,.4f,1,1)};
                for(int i=0;i<3;i++){
                    glm::vec4 col=dragAxis==(GizmoAxis)(i+1)?glm::vec4(1,1,.2f,1):rc[i];
                    drawRing(gizmoShader.ID,gPos,i,gs,col,vp);
                }
            }
        }
    }
}

bool DragFloat3XYZ(const char* label,float* v,float speed=0.05f){
    bool changed=false;
    ImGui::PushID(label);
    float w=(ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize("X").x*3-ImGui::GetStyle().ItemSpacing.x*5)/3;
    ImGui::TextDisabled("%s",label);
    ImGui::SameLine(80);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.5f,.1f,.1f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##x",&v[0],speed))changed=true;
    ImGui::PopStyleColor();ImGui::SameLine(0,3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.1f,.4f,.1f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##y",&v[1],speed))changed=true;
    ImGui::PopStyleColor();ImGui::SameLine(0,3);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(.1f,.2f,.5f,1));
    ImGui::SetNextItemWidth(w);if(ImGui::DragFloat("##z",&v[2],speed))changed=true;
    ImGui::PopStyleColor();
    ImGui::PopID();
    return changed;
}


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
        f << "\"texture\":\""+o.texturePath+"\"";
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

int main(int argc, char** argv)
{
    std::cout<<"Engine starting..."<<std::endl;

    // ── Разбор аргументов командной строки: --play "путь/к/сцене" / --project "путь" ──
    for (int i=1;i<argc;i++) {
        std::string a = argv[i];
        if (a=="--play" && i+1<argc) {
            g_PlayerMode = true;
            g_PlayerScenePath = argv[i+1];
            i++;
        }
        else if (a=="--project" && i+1<argc) {
            g_OverrideProjectRoot = argv[i+1];
            i++;
        }
    }
    // ── Если аргумент не передан — ищем player.cfg рядом с .exe ──
    // (так собранная через Build игра запускается просто двойным
    //  кликом, без необходимости вручную прописывать аргументы)
    if (!g_PlayerMode && fs::exists("player.cfg")) {
        std::ifstream pf("player.cfg");
        std::string scenePath;
        if (pf && std::getline(pf, scenePath) && !scenePath.empty()) {
            g_PlayerMode = true;
            g_PlayerScenePath = scenePath;
        }
    }

    g_Prefs.Load();
    camera.Speed       = g_Prefs.camSpeed;
    camera.Sensitivity = g_Prefs.camSensitivity;
    // Init GLFW temporarily to get monitor size, Window::Create will reinit safely
    glfwInit();
    int screenW = 1920, screenH = 1080;
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor) {
        const GLFWvidmode* vidmode = glfwGetVideoMode(primaryMonitor);
        if (vidmode) { screenW = vidmode->width; screenH = vidmode->height; }
    }
    std::string windowTitle = g_PlayerMode ? "Game" : "VisualEngine v0.1";
    VE::Window* window=VE::Window::Create(VE::WindowProps(windowTitle, screenW, screenH));
    GLFWwindow* native=(GLFWwindow*)window->GetNativeWindow();
    glfwMaximizeWindow(native);
    glfwGetWindowSize(native, &screenW, &screenH);
    glfwSetMouseButtonCallback(native,mouse_button_callback);
    glfwSetWindowFocusCallback(native,[](GLFWwindow*,int focused){
        if(focused){rightMouseDown=false;firstMouse=true;}
    });
    glfwSetDropCallback(native,[](GLFWwindow*,int count,const char** paths){
        std::cout<<"[DEBUG] Drop callback fired! count="<<count<<"\n";
        for(int i=0;i<count;i++){
            std::cout<<"[DEBUG]   path="<<paths[i]<<"\n";
            g_DroppedFiles.push_back(paths[i]);
        }
    });

    IMGUI_CHECKVERSION();ImGui::CreateContext();
    VE::InitWhiteTexture();
    // ── Load logo texture ──
    static GLuint g_LogoTex = 0;
    if (g_LogoTex == 0 && fs::exists("logo.png")) {
        g_LogoTex = VE::LoadTextureRaw("logo.png");
        if (g_LogoTex) logInfo("Logo loaded");
    }
    ImGuiIO& io=ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(native,true);
    // ── Наш коллбэк мыши регистрируем ПОСЛЕ ImGui и запоминаем его коллбэк —
    //    иначе ImGui_ImplGlfw_InitForOpenGL(...,true) тихо подменяет
    //    glfwSetCursorPosCallback своим, и g_RawMouseDX/DY (для FPS-камеры
    //    из Lua) вообще перестают обновляться.
    g_PrevCursorPosCallback = glfwSetCursorPosCallback(native, mouse_callback);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Fonts: using default ImGui font

    VE::Shader shader(vertSrc,fragSrc);
    VE::Shader skinnedShader(vertSkinnedSrc,fragSrc);
    VE::Shader outlineShader(outlineVert,outlineFrag);
    VE::Shader gridShader(gridVert,gridFrag);
    VE::Shader gizmoShader(gizmoVert,gizmoFrag);
    VE::Shader skyboxShader(skyboxVert,skyboxFrag);
    VE::Grid grid(20);

    std::string skyboxBase=(fs::current_path()/"assets"/"skybox"/"").string();
    VE::Skybox skybox({skyboxBase+"right.jpg",skyboxBase+"left.jpg",skyboxBase+"top.jpg",skyboxBase+"bottom.jpg",skyboxBase+"front.jpg",skyboxBase+"back.jpg"});

    unsigned int cubeVAO=setupCubeVAO();
    VE::Mesh sphere=VE::CreateSphere(),cylinder=VE::CreateCylinder(),pyramid=VE::CreatePyramid(),capsule=VE::CreateCapsule(),plane=VE::CreatePlane();
    int arrowCnt=0;unsigned int arrowVAO=buildArrowVAO(arrowCnt);

    unsigned int sceneFBO,sceneTex,sceneRBO;
    glGenFramebuffers(1,&sceneFBO);glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO);
    glGenTextures(1,&sceneTex);glBindTexture(GL_TEXTURE_2D,sceneTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,3840,2160,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,sceneTex,0);
    glGenRenderbuffers(1,&sceneRBO);glBindRenderbuffer(GL_RENDERBUFFER,sceneRBO);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,sceneRBO);
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    unsigned int gameFBO,gameTex,gameRBO;
    glGenFramebuffers(1,&gameFBO);glBindFramebuffer(GL_FRAMEBUFFER,gameFBO);
    glGenTextures(1,&gameTex);glBindTexture(GL_TEXTURE_2D,gameTex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,3840,2160,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,gameTex,0);
    glGenRenderbuffers(1,&gameRBO);glBindRenderbuffer(GL_RENDERBUFFER,gameRBO);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,gameRBO);
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    std::vector<SceneObject> objects;
    g_LuaObjectsPtr = &objects; // для Animation.* API из Lua
    std::vector<LightObject> lights;
    std::vector<CameraObject> sceneCameras;
    int sel=-1,selLight=-1,selCamera=-1;
    SelectionType selType=SelectionType::None;

    {SceneObject o;o.name="Cube";o.pos=glm::vec3(0,.5f,0);o.type=PrimitiveType::Cube;o.color=glm::vec3(1.f,1.f,1.f);
     o.ecsID=scene.CreateEntity(o.name);scene.GetTransform(o.ecsID).Position=o.pos;
     scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);objects.push_back(o);}
    {LightObject l;l.name="PointLight_1";l.pos=glm::vec3(5,8,4);l.color=glm::vec3(1,1,1);l.intensity=1.f;l.range=30.f;
     l.ecsID=scene.CreateEntity(l.name);scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);lights.push_back(l);}
    {CameraObject cam;cam.name="GameCamera_1";cam.pos=glm::vec3(0,2,5);
     cam.ecsID=scene.CreateEntity(cam.name);scene.registry.AddComponent<VE::CameraComponent>(cam.ecsID,true);sceneCameras.push_back(cam);}
    sel=0;selType=SelectionType::Object;
    logInfo("ECS initialized — "+std::to_string(scene.EntityCount())+" entities");

    VE::ScriptEditor scriptEditor;
    std::string projectRoot = g_PlayerMode
        ? fs::current_path().string()
        : (!g_OverrideProjectRoot.empty() ? g_OverrideProjectRoot : (fs::current_path()/"project").string());
    std::string currentScenePath="";
    bool showSkybox=true,showGrid=true,showGizmos=true;
    int g_SideTab=0; // 0=Hierarchy, 1=Project, 2=Scripts
    static char hierSearch[128]={};
    std::string assetCurrentPath=projectRoot+"\\Assets",assetSelected="";
    static char assetSearch[128]={};

    // ── Стартовая структура проекта (как Unity: сразу готовые папки) ──
    {
        const char* starterFolders[] = {
            "Assets\\Scenes",
            "Assets\\Scripts",
            "Assets\\Materials",
            "Assets\\Models",
            "Assets\\Textures",
            "Assets\\Audio",
            "Saves"
        };
        for (auto* f : starterFolders) {
            try { fs::create_directories(projectRoot+"\\"+f); } catch(...) {}
        }
    }

    logInfo("Engine initialized — VisualEngine v0.1");
    logInfo("Scene loaded: Untitled");

    VE::AudioEngine::Get().Init();
    VE::SaveSystem::Get().SetSaveDir(projectRoot + "\\Saves");
    VE::BuildSystem::Get().SetEngineRoot((fs::current_path().parent_path()).string());

    // ── SceneManager: регистрируем коллбэк загрузки сцены ──
    VE::SceneManager::Get().SetLoadCallback([&](const std::string& path){
        // Если идёт Play — стопаем
        if(isPlaying){
            isPlaying=false; isPaused=false;
            if(g_MouseCaptured){ g_MouseCaptured=false; glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
            VE::Physics::Get().ClearAllBodies();
            for(auto& obj:objects){ obj.luaInstances.clear(); }
        }
        objects.clear(); lights.clear(); sceneCameras.clear();
        sel=-1; selLight=-1; selCamera=-1; selType=SelectionType::None;
        LoadScene(path,objects,lights,sceneCameras,sel,selType);
        currentScenePath=path;
        // Пересоздаём ECS сущности
        for(auto& o:objects){
            o.ecsID=scene.CreateEntity(o.name);
            scene.GetTransform(o.ecsID).Position=o.pos;
            scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
        }
        logInfo("Scene loaded: "+path);
        // Player mode: как только сцена загрузилась — ставим флаг на автозапуск Play
        // (саму StartPlay() вызвать здесь нельзя — она объявляется ниже по коду)
        if (g_PlayerMode) g_PlayerAutoPlayPending = true;
    });

    glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);

    // ── Запуск Play: создаёт Lua-инстансы объектов и вызывает onStart() ──
    // Вынесено в отдельную функцию, чтобы её мог вызвать и Play-кнопка
    // редактора, и автозапуск в player mode (когда движок собран как игра).
    auto StartPlay = [&](){
        isPlaying=true; isPaused=false;
        savedTransforms.clear();
        for(auto& obj:objects) savedTransforms.push_back({obj.pos,obj.rot,obj.scale});
        for(auto& obj:objects){
            for(auto& scriptPath : obj.scriptPaths){
            if(scriptPath.empty()) continue;
            auto luaInst=std::make_shared<VE::LuaEngine>();
            luaInst->scriptPath = scriptPath;
            luaInst->setWindow(native);
            VE::AudioEngine::Get().RegisterLua(luaInst->L);
            VE::SceneManager::Get().RegisterLua(luaInst->L);
            VE::HUD::Get().RegisterLua(luaInst->L);
            VE::SaveSystem::Get().RegisterLua(luaInst->L);

            // ── Physics API: управление физикой СВОЕГО объекта ──
            // Physics.AddForce(x,y,z) / AddImpulse(x,y,z) / SetVelocity(x,y,z)
            // Physics.GetVelocity() -> x,y,z / Physics.Stop()
            {
                lua_State* LL = luaInst->L;
                VE::EntityID selfID = obj.ecsID;
                lua_newtable(LL);

                auto pushSelfFn = [&](const char* name, lua_CFunction fn){
                    lua_pushstring(LL, name);
                    lua_pushinteger(LL, (lua_Integer)selfID);
                    lua_pushcclosure(LL, fn, 1);
                    lua_settable(LL, -3);
                };

                pushSelfFn("AddForce", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    float x=(float)luaL_optnumber(L,1,0), y=(float)luaL_optnumber(L,2,0), z=(float)luaL_optnumber(L,3,0);
                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id))
                        scene.registry.GetComponent<VE::RigidbodyComponent>(id).AddForce(glm::vec3(x,y,z));
                    return 0;
                });
                pushSelfFn("AddImpulse", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    float x=(float)luaL_optnumber(L,1,0), y=(float)luaL_optnumber(L,2,0), z=(float)luaL_optnumber(L,3,0);
                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id))
                        scene.registry.GetComponent<VE::RigidbodyComponent>(id).AddImpulse(glm::vec3(x,y,z));
                    return 0;
                });
                pushSelfFn("SetVelocity", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    float x=(float)luaL_optnumber(L,1,0), y=(float)luaL_optnumber(L,2,0), z=(float)luaL_optnumber(L,3,0);
                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id))
                        scene.registry.GetComponent<VE::RigidbodyComponent>(id).Velocity = glm::vec3(x,y,z);
                    return 0;
                });
                pushSelfFn("GetVelocity", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    glm::vec3 v(0.f);
                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id))
                        v = scene.registry.GetComponent<VE::RigidbodyComponent>(id).Velocity;
                    lua_pushnumber(L,v.x); lua_pushnumber(L,v.y); lua_pushnumber(L,v.z);
                    return 3;
                });
                pushSelfFn("Stop", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id))
                        scene.registry.GetComponent<VE::RigidbodyComponent>(id).Stop();
                    return 0;
                });

                lua_setglobal(LL, "Physics");
            }

            // ── Scene API: найти другой объект по имени и прочитать/задать позицию ──
            // Scene.GetPosition("Name") -> x,y,z (ничего, если не найден)
            // Scene.SetPosition("Name", x,y,z)
            // Scene.Exists("Name") -> bool
            {
                lua_State* LL = luaInst->L;
                lua_newtable(LL);

                lua_pushstring(LL, "GetPosition");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* name=luaL_checkstring(L,1);
                    for(auto& o:*objs){
                        if(o.name==name){
                            lua_pushnumber(L,o.pos.x); lua_pushnumber(L,o.pos.y); lua_pushnumber(L,o.pos.z);
                            return 3;
                        }
                    }
                    return 0;
                }, 1);
                lua_settable(LL, -3);

                lua_pushstring(LL, "SetPosition");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* name=luaL_checkstring(L,1);
                    float x=(float)luaL_optnumber(L,2,0), y=(float)luaL_optnumber(L,3,0), z=(float)luaL_optnumber(L,4,0);
                    for(auto& o:*objs){
                        if(o.name==name){ o.pos=glm::vec3(x,y,z); return 0; }
                    }
                    return 0;
                }, 1);
                lua_settable(LL, -3);

                lua_pushstring(LL, "Exists");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* name=luaL_checkstring(L,1);
                    for(auto& o:*objs) if(o.name==name){ lua_pushboolean(L,1); return 1; }
                    lua_pushboolean(L,0);
                    return 1;
                }, 1);
                lua_settable(LL, -3);

                lua_setglobal(LL, "Scene");
            }

            // ── Environment API: время суток и туман из Lua ──
            // Environment.SetTimeOfDay(hours) / GetTimeOfDay()
            // Environment.SetFog(density, r, g, b)
            {
                lua_State* LL = luaInst->L;
                lua_newtable(LL);

                lua_pushstring(LL, "SetTimeOfDay");
                lua_pushcfunction(LL, [](lua_State* L)->int{
                    extern float g_TimeOfDay;
                    float h=(float)luaL_optnumber(L,1,12.0);
                    while(h<0.f)h+=24.f; g_TimeOfDay=fmodf(h,24.f);
                    return 0;
                });
                lua_settable(LL, -3);

                lua_pushstring(LL, "GetTimeOfDay");
                lua_pushcfunction(LL, [](lua_State* L)->int{
                    extern float g_TimeOfDay;
                    lua_pushnumber(L,g_TimeOfDay);
                    return 1;
                });
                lua_settable(LL, -3);

                lua_pushstring(LL, "SetFog");
                lua_pushcfunction(LL, [](lua_State* L)->int{
                    extern float g_FogDensity;
                    extern glm::vec3 g_FogColor;
                    g_FogDensity = (float)luaL_optnumber(L,1,g_FogDensity);
                    g_FogColor.x = (float)luaL_optnumber(L,2,g_FogColor.x);
                    g_FogColor.y = (float)luaL_optnumber(L,3,g_FogColor.y);
                    g_FogColor.z = (float)luaL_optnumber(L,4,g_FogColor.z);
                    return 0;
                });
                lua_settable(LL, -3);

                lua_setglobal(LL, "Environment");
            }

            // ── Animation API: управление проигрыванием анимации СВОЕГО объекта ──
            // Animation.Play("ИмяКлипа") или Animation.Play(0) по индексу
            // Animation.Stop() / Animation.SetLoop(true/false) / Animation.IsPlaying()
            {
                lua_State* LL = luaInst->L;
                VE::EntityID selfID = obj.ecsID;
                lua_newtable(LL);

                auto pushSelfFn = [&](const char* name, lua_CFunction fn){
                    lua_pushstring(LL, name);
                    lua_pushinteger(LL, (lua_Integer)selfID);
                    lua_pushcclosure(LL, fn, 1);
                    lua_settable(LL, -3);
                };

                pushSelfFn("Play", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    if(!g_LuaObjectsPtr) return 0;
                    for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID!=id || !o.model) continue;
                        if(lua_isnumber(L,1)){
                            int idx=(int)lua_tointeger(L,1);
                            if(idx>=0 && idx<(int)o.model->animations.size()){ o.animIndex=idx; o.animTime=0.f; o.animPlaying=true; }
                        } else {
                            const char* name = luaL_checkstring(L,1);
                            for(int a=0;a<(int)o.model->animations.size();a++){
                                if(o.model->animations[a].name==name){ o.animIndex=a; o.animTime=0.f; o.animPlaying=true; break; }
                            }
                        }
                        break;
                    }
                    return 0;
                });
                pushSelfFn("Stop", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    if(!g_LuaObjectsPtr) return 0;
                    for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID==id){ o.animPlaying=false; o.animTime=0.f; break; }
                    }
                    return 0;
                });
                pushSelfFn("SetLoop", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    bool loop = lua_toboolean(L,1)!=0;
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    if(!g_LuaObjectsPtr) return 0;
                    for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID==id){ o.animLoop=loop; break; }
                    }
                    return 0;
                });
                pushSelfFn("IsPlaying", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    bool playing=false;
                    if(g_LuaObjectsPtr) for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID==id){ playing=o.animPlaying; break; }
                    }
                    lua_pushboolean(L, playing);
                    return 1;
                });

                lua_setglobal(LL, "Animation");
            }

            std::ifstream sf(scriptPath);
            if(sf){
                std::stringstream ss; ss<<sf.rdbuf();
                if(luaInst->loadScript(ss.str())){
                    luaInst->objX=obj.pos.x;luaInst->objY=obj.pos.y;luaInst->objZ=obj.pos.z;
                    luaInst->objR=obj.color.r;luaInst->objG=obj.color.g;luaInst->objB=obj.color.b;
                    luaInst->pushObjectData();
                    luaInst->callOnStart();
                    luaInst->pullObjectData();
                    obj.pos.x=luaInst->objX;obj.pos.y=luaInst->objY;obj.pos.z=luaInst->objZ;
                    luaInst->started=true;
                    obj.luaInstances.push_back(luaInst);
                } else {
                    logError("Lua load failed: "+obj.name+" ("+fs::path(scriptPath).filename().string()+")");
                }
            }
            }
        }
        logInfo("Play");
    };

    // ── Player mode: сразу грузим сцену, Play включится автоматически после загрузки ──
    if (g_PlayerMode) {
        VE::SceneManager::Get().RequestLoad(g_PlayerScenePath);
        logInfo("Player mode: launching "+g_PlayerScenePath);
    }

    const char* typeNames[]={"Cube","Sphere","Cylinder","Pyramid","Capsule","Plane","Model"};
    static float hierW=240.f, inspW=280.f, bottomH=190.f;
    const float sideW=48.f, toolH=38.f;

    while(!window->ShouldClose())
    {
        float now=glfwGetTime();deltaTime=now-lastFrame;lastFrame=now;
        // ── Продвигаем время скелетной анимации (играет и в редакторе, для превью) ──
        for(auto& obj:objects){
            if(obj.animPlaying && obj.model && obj.model->hasSkeleton && obj.animIndex>=0)
                obj.animTime += deltaTime;
        }
        // ── Кастомная покадровая анимация — двигает/крутит/масштабирует ЛЮБОЙ объект ──
        for(auto& obj:objects){
            if(!obj.customAnimPlaying || obj.customClipIndex<0 || obj.customClipIndex>=(int)obj.customClips.size()) continue;
            auto& clip = obj.customClips[obj.customClipIndex];
            if(clip.keys.empty()) continue;
            obj.customAnimTime += deltaTime;
            float dur = clip.keys.back().time;
            if(obj.customAnimTime > dur){
                if(clip.loop) obj.customAnimTime = dur>0.f ? fmodf(obj.customAnimTime, dur) : 0.f;
                else { obj.customAnimTime = dur; obj.customAnimPlaying = false; }
            }
            SampleObjectClip(clip, obj.customAnimTime, obj.pos, obj.rot, obj.scale);
        }
        // ── Автосохранение сцены (настраивается в Preferences) ──
        {
            static float autosaveAccum=0.f;
            autosaveAccum += deltaTime;
            if (g_Prefs.autosaveEnabled && !currentScenePath.empty() &&
                autosaveAccum >= g_Prefs.autosaveMinutes*60.f) {
                autosaveAccum = 0.f;
                SaveScene(currentScenePath,objects,lights,sceneCameras);
                logInfo("Autosaved: "+currentScenePath);
            }
        }
        // ── Scene Manager: проверяем отложенную загрузку сцены ──
        if(VE::SceneManager::Get().Tick()) continue;
        if (g_PlayerAutoPlayPending) { g_PlayerAutoPlayPending=false; StartPlay(); }
        VE::HUD::Get().BeginFrame();
        g_DragHoverObj = -1; // сброс каждый кадр, обновляется в drop target
        float menuH=20.f;
        float viewH=io.DisplaySize.y-menuH-bottomH-toolH;
        float vpW=io.DisplaySize.x-sideW-hierW-inspW;
        float vpAspect=g_VpSize.x>1?g_VpSize.x/g_VpSize.y:vpW/viewH;

        if(!sceneCameras.empty()){
            for(auto& sc:sceneCameras){if(sc.isPrimary){
                if(sc.followTargetIndex>=0&&sc.followTargetIndex<(int)objects.size()&&isPlaying){
                    // Камера "follow" игрока (как в Unity child-camera) —
                    // позиция = followTarget.pos + offset, поворот в World Y =
                    // followTarget.rot.y (управляется Lua-скриптом игрока),
                    // pitch берётся из followTarget.lookPitch
                    auto& target=objects[sc.followTargetIndex];
                    gameCamera.Position=target.pos+sc.followOffset;
                    gameCamera.Yaw=target.rot.y-90.f;   // -90 коррекция под Yaw=0 смотрящий по -Z
                    gameCamera.Pitch=target.lookPitch;
                    gameCamera.UpdateVectors();
                } else {
                    gameCamera.Position=sc.pos;
                }
                break;
            }}
        }

        if(!io.WantCaptureKeyboard){
            if(glfwGetKey(native,GLFW_KEY_W)==GLFW_PRESS&&rightMouseDown) camera.ProcessKeyboard(0,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_S)==GLFW_PRESS&&rightMouseDown) camera.ProcessKeyboard(1,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_A)==GLFW_PRESS&&rightMouseDown) camera.ProcessKeyboard(2,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_D)==GLFW_PRESS&&rightMouseDown) camera.ProcessKeyboard(3,deltaTime);
            if(!rightMouseDown){
                if(glfwGetKey(native,GLFW_KEY_Q)==GLFW_PRESS) gizmoMode=GizmoMode::Select;
                if(glfwGetKey(native,GLFW_KEY_W)==GLFW_PRESS) gizmoMode=GizmoMode::Move;
                if(glfwGetKey(native,GLFW_KEY_E)==GLFW_PRESS) gizmoMode=GizmoMode::Rotate;
                if(glfwGetKey(native,GLFW_KEY_R)==GLFW_PRESS) gizmoMode=GizmoMode::Scale;
            }
            if(glfwGetKey(native,GLFW_KEY_DELETE)==GLFW_PRESS&&selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()){
                if(scene.IsAlive(objects[sel].ecsID))scene.DestroyEntity(objects[sel].ecsID);
                objects.erase(objects.begin()+sel);
                if(sel>=(int)objects.size())sel=(int)objects.size()-1;
            }
        }
        if(isPlaying&&!isPaused&&rightMouseDown){
            if(glfwGetKey(native,GLFW_KEY_W)==GLFW_PRESS) gameCamera.ProcessKeyboard(0,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_S)==GLFW_PRESS) gameCamera.ProcessKeyboard(1,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_A)==GLFW_PRESS) gameCamera.ProcessKeyboard(2,deltaTime);
            if(glfwGetKey(native,GLFW_KEY_D)==GLFW_PRESS) gameCamera.ProcessKeyboard(3,deltaTime);
        }

        glm::vec3 selPos(0);
        if(selType==SelectionType::Object&&sel>=0&&sel<(int)objects.size()) selPos=objects[sel].pos;
        else if(selType==SelectionType::Light&&selLight>=0&&selLight<(int)lights.size()) selPos=lights[selLight].pos;
        else if(selType==SelectionType::Camera&&selCamera>=0&&selCamera<(int)sceneCameras.size()){
            auto& scSel=sceneCameras[selCamera];
            selPos = (scSel.followTargetIndex>=0 && scSel.followTargetIndex<(int)objects.size())
                   ? objects[scSel.followTargetIndex].pos + scSel.followOffset
                   : scSel.pos;
        }
        float dist=glm::length(camera.Position-selPos);if(dist<0.1f)dist=3.f;
        float gs=dist*0.16f;

        glm::mat4 view=camera.GetViewMatrix();
        glm::mat4 proj=camera.GetProjectionMatrix(vpAspect);

        if(leftClickThisFrame&&!rightMouseDown){
            double lx=clickX-g_VpPos.x,ly=clickY-g_VpPos.y;
            int vw=(int)g_VpSize.x,vh=(int)g_VpSize.y;
            if(lx>=0&&ly>=0&&lx<vw&&ly<vh){
                Ray ray=screenToRay(lx,ly,vw,vh,view,proj);
                bool hitGizmo=false;
                if(gizmoMode!=GizmoMode::Select){
                    glm::vec3 axes[3]={glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,1)};
                    float bestT=1e9f;GizmoAxis hitAxis=GizmoAxis::None;
                    if(gizmoMode==GizmoMode::Move||gizmoMode==GizmoMode::Scale){
                        for(int i=0;i<3;i++){float t;if(gizmoArrowHit(ray,selPos,axes[i],gs,t)&&t<bestT){bestT=t;hitAxis=(GizmoAxis)(i+1);}}
                    } else {
                        for(int i=0;i<3;i++){glm::vec3 hs=glm::vec3(gs*1.1f);hs[i]=0.12f*gs;float t;if(rayAABB(ray,selPos,hs,t)&&t<bestT){bestT=t;hitAxis=(GizmoAxis)(i+1);}}
                    }
                    if(hitAxis!=GizmoAxis::None){
                        hitGizmo=true;dragAxis=hitAxis;dragStartPos=selPos;
                        if(selType==SelectionType::Object&&sel>=0){dragStartRot=objects[sel].rot;dragStartScale=objects[sel].scale;}
                        glm::vec3 axDir=axes[(int)hitAxis-1];
                        glm::vec3 plN=glm::normalize(glm::cross(axDir,glm::cross(camera.Front,axDir)));
                        float t=rayPlaneT(ray,plN,selPos);dragStartHit=t>0?ray.origin+ray.dir*t:selPos;
                    }
                }
                if(!hitGizmo){
                    float bO=1e9f;int bI=-1;SelectionType bType=SelectionType::None;
                    for(int i=0;i<(int)objects.size();i++){float t;if(rayAABB(ray,objects[i].pos,objects[i].scale*.55f,t)&&t<bO){bO=t;bI=i;bType=SelectionType::Object;}}
                    for(int i=0;i<(int)lights.size();i++){float t;if(rayAABB(ray,lights[i].pos,glm::vec3(.35f),t)&&t<bO){bO=t;bI=i;bType=SelectionType::Light;}}
                    for(int i=0;i<(int)sceneCameras.size();i++){
                        glm::vec3 cwp=sceneCameras[i].pos;
                        if(sceneCameras[i].followTargetIndex>=0 && sceneCameras[i].followTargetIndex<(int)objects.size())
                            cwp = objects[sceneCameras[i].followTargetIndex].pos + sceneCameras[i].followOffset;
                        float t;if(rayAABB(ray,cwp,glm::vec3(.35f),t)&&t<bO){bO=t;bI=i;bType=SelectionType::Camera;}
                    }
                    if(bType!=SelectionType::None){
                        selType=bType;
                        if(bType==SelectionType::Object){sel=bI;logInfo("Selected: "+objects[bI].name);}
                        else if(bType==SelectionType::Light){selLight=bI;logInfo("Selected: "+lights[bI].name);}
                        else if(bType==SelectionType::Camera){selCamera=bI;logInfo("Selected: "+sceneCameras[bI].name);}
                    }
                }
            }
        }
        if(leftDown&&dragAxis!=GizmoAxis::None){
            double lx=mouseX-g_VpPos.x,ly=mouseY-g_VpPos.y;
            Ray ray=screenToRay(lx,ly,(int)g_VpSize.x,(int)g_VpSize.y,view,proj);
            glm::vec3 axes[3]={glm::vec3(1,0,0),glm::vec3(0,1,0),glm::vec3(0,0,1)};
            int ai=(int)dragAxis-1;
            glm::vec3 plN=glm::normalize(glm::cross(axes[ai],glm::cross(camera.Front,axes[ai])));
            float t=rayPlaneT(ray,plN,dragStartPos);
            if(t>0){
                glm::vec3 hit=ray.origin+ray.dir*t;float p=glm::dot(hit-dragStartHit,axes[ai]);
                if(gizmoMode==GizmoMode::Move){
                    glm::vec3 np=dragStartPos+axes[ai]*p;
                    if(selType==SelectionType::Object&&sel>=0){
                        objects[sel].pos=np;
                        if(objects[sel].parentIndex>=0)
                            objects[sel].localOffset=np-objects[objects[sel].parentIndex].pos;
                    }
                    else if(selType==SelectionType::Light&&selLight>=0) lights[selLight].pos=np;
                    else if(selType==SelectionType::Camera&&selCamera>=0){
                        auto& scCam=sceneCameras[selCamera];
                        if(scCam.followTargetIndex>=0 && scCam.followTargetIndex<(int)objects.size())
                            scCam.followOffset = np - objects[scCam.followTargetIndex].pos;
                        else
                            scCam.pos = np;
                    }
                }
                else if(gizmoMode==GizmoMode::Scale&&selType==SelectionType::Object&&sel>=0){
                    glm::vec3 sc=dragStartScale;sc[ai]*=std::max(1.f+p*.5f,.05f);objects[sel].scale=sc;
                }
                else if(gizmoMode==GizmoMode::Rotate&&selType==SelectionType::Object&&sel>=0){
                    float signs[3]={1.f,-1.f,1.f};glm::vec3 ro=dragStartRot;ro[ai]+=p*90.f*signs[ai];objects[sel].rot=ro;
                }
            }
        }
        leftClickThisFrame=false;

        if(isPlaying&&!isPaused){
            VE::Physics::Get().Step(scene.registry,deltaTime);
            for(auto& obj:objects){
                if(obj.hasRigidBody&&scene.registry.HasComponent<VE::RigidbodyComponent>(obj.ecsID)){
                    auto& tr=scene.GetTransform(obj.ecsID);
                    obj.pos=tr.Position;
                    obj.rot=tr.Rotation;
                }
            }
            for(auto& obj:objects){
                if(obj.parentIndex>=0&&obj.parentIndex<(int)objects.size()){
                    obj.pos=objects[obj.parentIndex].pos+obj.localOffset;
                }
            }
            for(auto& obj:objects){
                for(auto& li : obj.luaInstances){
                    if(!li) continue;
                    li->objX=obj.pos.x;li->objY=obj.pos.y;li->objZ=obj.pos.z;
                    li->objRotX=obj.rot.x;li->objRotY=obj.rot.y;li->objRotZ=obj.rot.z;
                    li->objR=obj.color.r;li->objG=obj.color.g;li->objB=obj.color.b;
                    li->objLookPitch=obj.lookPitch;
                    li->pushObjectData();li->callOnUpdate(deltaTime);li->pullObjectData();
                    obj.pos.x=li->objX;obj.pos.y=li->objY;obj.pos.z=li->objZ;
                    obj.rot.x=li->objRotX;obj.rot.y=li->objRotY;obj.rot.z=li->objRotZ;
                    obj.color.r=li->objR;obj.color.g=li->objG;obj.color.b=li->objB;
                    obj.lookPitch=li->objLookPitch;
                }
                // позиция и поворот могли измениться из Lua — применить их и к Bullet rigidbody
                if(obj.hasRigidBody&&scene.registry.HasComponent<VE::RigidbodyComponent>(obj.ecsID)){
                    auto& tr=scene.GetTransform(obj.ecsID);
                    tr.Position=obj.pos;
                    tr.Rotation=obj.rot;
                }
            }

            // ── Collision / Trigger callbacks ──
            // EntityID -> SceneObject* для быстрого поиска при диспетчеризации
            auto findObjByEntity=[&](VE::EntityID id)->SceneObject*{
                for(auto& o:objects) if(o.ecsID==id) return &o;
                return nullptr;
            };
            for(auto& pair:VE::Physics::Get().GetCollisionEnters()){
                SceneObject* a=findObjByEntity(pair.A); SceneObject* b=findObjByEntity(pair.B);
                if(a) for(auto& li:a->luaInstances){ if(!li) continue;
                    if(pair.IsTrigger) li->callOnTriggerEnter(b?b->name:"",  (int)pair.B);
                    else               li->callOnCollisionEnter(b?b->name:"",(int)pair.B);
                }
                if(b) for(auto& li:b->luaInstances){ if(!li) continue;
                    if(pair.IsTrigger) li->callOnTriggerEnter(a?a->name:"",  (int)pair.A);
                    else               li->callOnCollisionEnter(a?a->name:"",(int)pair.A);
                }
            }
            for(auto& pair:VE::Physics::Get().GetCollisionExits()){
                SceneObject* a=findObjByEntity(pair.A); SceneObject* b=findObjByEntity(pair.B);
                if(a) for(auto& li:a->luaInstances){ if(!li) continue;
                    if(pair.IsTrigger) li->callOnTriggerExit(b?b->name:"",  (int)pair.B);
                    else               li->callOnCollisionExit(b?b->name:"",(int)pair.B);
                }
                if(b) for(auto& li:b->luaInstances){ if(!li) continue;
                    if(pair.IsTrigger) li->callOnTriggerExit(a?a->name:"",  (int)pair.A);
                    else               li->callOnCollisionExit(a?a->name:"",(int)pair.A);
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER,sceneFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,sel,false,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,camera,vpAspect,gizmoMode,dragAxis,showSkybox,showGrid,showGizmos,gs,lights,sceneCameras,selLight,selCamera,selType);
        // ── Своё же тело не должно быть видно от первого лица (как в Unity/Godot) ──
        int fpExcludeIdx=-1;
        for(auto& sc:sceneCameras){
            if(sc.isPrimary && sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size()){
                fpExcludeIdx=sc.followTargetIndex; break;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER,gameFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,-1,true,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,gameCamera,vpAspect,gizmoMode,dragAxis,showSkybox,false,false,gs,lights,sceneCameras,-1,-1,SelectionType::None,fpExcludeIdx);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,(int)io.DisplaySize.x,(int)io.DisplaySize.y); // restore full
        glClearColor(0.08f,0.08f,0.09f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        menuH=ImGui::GetFrameHeight();

// ═══════════════════════════════════════════════════════
//   ВИЗУАЛЬНАЯ ТЕМА — чёрный + фиолетовый (Unity6/UE5)
// ═══════════════════════════════════════════════════════
static bool themeApplied = false;
if (!themeApplied) {
    themeApplied = true;
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding    = 4.f;
    st.ChildRounding     = 4.f;
    st.FrameRounding     = 3.f;
    st.PopupRounding     = 4.f;
    st.ScrollbarRounding = 3.f;
    st.GrabRounding      = 3.f;
    st.TabRounding       = 4.f;
    st.WindowBorderSize  = 1.f;
    st.FrameBorderSize   = 0.f;
    st.WindowPadding     = ImVec2(8,8);
    st.FramePadding      = ImVec2(6,3);
    st.ItemSpacing       = ImVec2(6,4);
    st.ItemInnerSpacing  = ImVec2(4,4);
    st.IndentSpacing     = 14.f;
    st.ScrollbarSize     = 8.f;
    st.GrabMinSize       = 6.f;

    ImVec4* c = st.Colors;
    // ── CONCEPT COLORS: #16181c base, #1e2024 panels, #2563eb accent ──
    // Backgrounds
    c[ImGuiCol_WindowBg]          = ImVec4(0.086f,0.094f,0.106f,1.f); // #161820
    c[ImGuiCol_ChildBg]           = ImVec4(0.086f,0.094f,0.106f,1.f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.118f,0.125f,0.141f,0.98f); // #1e2024
    // Title
    c[ImGuiCol_TitleBg]           = ImVec4(0.086f,0.094f,0.106f,1.f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.086f,0.094f,0.106f,1.f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.086f,0.094f,0.106f,1.f);
    // MenuBar
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.082f,0.090f,0.102f,1.f); // #15171a - concept titlebar
    // Border
    c[ImGuiCol_Border]            = ImVec4(0.176f,0.188f,0.212f,1.f); // #2d3036
    c[ImGuiCol_BorderShadow]      = ImVec4(0.f,0.f,0.f,0.f);
    // Frames
    c[ImGuiCol_FrameBg]           = ImVec4(0.118f,0.125f,0.141f,1.f); // #1e2024
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.149f,0.161f,0.180f,1.f); // #262930
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.220f,0.240f,0.270f,1.f); // #2563eb
    // Scrollbar
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.086f,0.094f,0.106f,1.f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.196f,0.212f,0.235f,1.f);
    c[ImGuiCol_ScrollbarGrabHovered]=ImVec4(0.247f,0.267f,0.298f,1.f);
    c[ImGuiCol_ScrollbarGrabActive] =ImVec4(0.300f,0.320f,0.360f,1.f);
    // Checkbox / slider
    c[ImGuiCol_CheckMark]         = ImVec4(0.700f,0.720f,0.760f,1.f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.300f,0.320f,0.360f,1.f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.500f,0.520f,0.560f,1.f);
    // Buttons — subtle, like concept
    c[ImGuiCol_Button]            = ImVec4(0.137f,0.149f,0.169f,1.f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.180f,0.196f,0.220f,1.f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.200f,0.220f,0.250f,1.f);
    // Headers (TreeNode selected etc)
    c[ImGuiCol_Header]            = ImVec4(0.180f,0.196f,0.220f,0.6f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.200f,0.216f,0.240f,1.f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.220f,0.240f,0.270f,1.f);
    // Separators
    c[ImGuiCol_Separator]         = ImVec4(0.176f,0.188f,0.212f,1.f);
    c[ImGuiCol_SeparatorHovered]  = ImVec4(0.300f,0.320f,0.360f,1.f);
    c[ImGuiCol_SeparatorActive]   = ImVec4(0.400f,0.420f,0.460f,1.f);
    // Resize
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.300f,0.320f,0.360f,0.2f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.400f,0.420f,0.460f,0.6f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.500f,0.520f,0.560f,1.f);
    // Tabs
    c[ImGuiCol_Tab]               = ImVec4(0.094f,0.102f,0.114f,1.f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.149f,0.161f,0.180f,1.f);
    c[ImGuiCol_TabActive]         = ImVec4(0.137f,0.149f,0.169f,1.f);
    c[ImGuiCol_TabUnfocused]      = ImVec4(0.086f,0.094f,0.106f,1.f);
    c[ImGuiCol_TabUnfocusedActive]= ImVec4(0.098f,0.106f,0.118f,1.f);
    // Text
    c[ImGuiCol_Text]              = ImVec4(0.878f,0.894f,0.918f,1.f); // #e0e4ea
    c[ImGuiCol_TextDisabled]      = ImVec4(0.376f,0.400f,0.435f,1.f); // #606670
    c[ImGuiCol_TextSelectedBg]    = ImVec4(0.300f,0.320f,0.360f,0.4f);
    // Misc
    c[ImGuiCol_PlotLines]         = ImVec4(0.600f,0.620f,0.660f,1.f);
    c[ImGuiCol_PlotLinesHovered]  = ImVec4(0.800f,0.820f,0.860f,1.f);
    c[ImGuiCol_PlotHistogram]     = ImVec4(0.400f,0.420f,0.460f,1.f);
    c[ImGuiCol_PlotHistogramHovered]=ImVec4(0.600f,0.620f,0.660f,1.f);
    c[ImGuiCol_ModalWindowDimBg]  = ImVec4(0.f,0.f,0.f,0.5f);
    c[ImGuiCol_NavHighlight]      = ImVec4(0.500f,0.520f,0.560f,1.f);
    c[ImGuiCol_DragDropTarget]    = ImVec4(0.700f,0.720f,0.760f,1.f);
}
const ImVec4 COL_ACCENT      = ImVec4(0.220f,0.240f,0.270f,1.f); // subtle hover
const ImVec4 COL_ACCENT_HOV  = ImVec4(0.700f,0.720f,0.760f,1.f); // light text
const ImVec4 COL_PLAY        = ImVec4(0.20f, 0.75f, 0.30f, 1.f);
const ImVec4 COL_STOP        = ImVec4(0.85f, 0.25f, 0.25f, 1.f);
const ImVec4 COL_PAUSE       = ImVec4(0.85f, 0.65f, 0.10f, 1.f);
const ImVec4 COL_LIGHT_OBJ   = ImVec4(1.00f, 0.90f, 0.35f, 1.f);
const ImVec4 COL_CAM_OBJ     = ImVec4(0.40f, 0.80f, 1.00f, 1.f);
const ImVec4 COL_DIM         = ImVec4(0.40f, 0.42f, 0.46f, 1.f);
const ImVec4 COL_GREEN       = ImVec4(0.35f, 0.90f, 0.45f, 1.f);
const ImVec4 COL_RED_X       = ImVec4(0.95f, 0.35f, 0.35f, 1.f);
const ImVec4 COL_GREEN_Y     = ImVec4(0.35f, 0.90f, 0.40f, 1.f);
const ImVec4 COL_BLUE_Z      = ImVec4(0.35f, 0.60f, 1.00f, 1.f);

// ── Хелпер — горизонтальная линия с отступами ──
auto HRule = [&](){
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.176f,0.188f,0.212f,1.f));
    ImGui::Separator();
    ImGui::PopStyleColor();
};

// ── Хелпер — кнопка-переключатель с подсветкой акцентом ──
auto ToggleBtn = [&](const char* lbl, bool active, ImVec2 sz) -> bool {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        g_Prefs.accentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(g_Prefs.accentColor.x+0.10f,g_Prefs.accentColor.y+0.08f,g_Prefs.accentColor.z+0.15f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL_ACCENT_HOV);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f,0.11f,0.16f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f,0.14f,0.30f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,          COL_DIM);
    }
    bool clicked = ImGui::Button(lbl, sz);
    ImGui::PopStyleColor(3);
    return clicked;
};

if (!g_PlayerMode) {
if (ImGui::BeginMainMenuBar()) {
    // ── Logo ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f,0.89f,0.92f,1.f));
    ImGui::Text("  VE  VisualEngine");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(ImVec4(0.25f,0.25f,0.28f,1.f), "|");
    ImGui::SameLine(0, 8);

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene",  "Ctrl+N")) logInfo("New scene");
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
            std::string sp=projectRoot+"\\Assets\\Scenes\\scene.vescene";
            if(fs::exists(sp)){
                LoadScene(sp,objects,lights,sceneCameras,sel,selType);
                currentScenePath=sp;
                logInfo("Scene loaded: "+sp);
            } else logWarn("No scene file found: "+sp);
        }
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
            if(currentScenePath.empty()) currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
            SaveScene(currentScenePath,objects,lights,sceneCameras);
            logInfo("Scene saved: "+currentScenePath);
        }
        if (ImGui::MenuItem("Save Scene As...")) {
            currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
            SaveScene(currentScenePath,objects,lights,sceneCameras);
            logInfo("Scene saved: "+currentScenePath);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Build & Export...")) {
            if(currentScenePath.empty()){
                currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
                SaveScene(currentScenePath,objects,lights,sceneCameras);
            }
            bool ok = VE::BuildSystem::Get().Build(projectRoot, currentScenePath, "Game");
            for(auto& line : VE::BuildSystem::Get().GetLog()) logInfo(line);
            if(ok) logInfo(">>> Build complete! Check Build/ folder");
            else   logError(">>> Build failed!");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4"))       glfwSetWindowShouldClose(native, true);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Duplicate","Ctrl+D") && selType==SelectionType::Object && sel>=0) {
            SceneObject copy=objects[sel]; copy.name+="_copy"; copy.pos+=glm::vec3(1,0,0);
            copy.ecsID=scene.CreateEntity(copy.name); scene.GetTransform(copy.ecsID).Position=copy.pos;
            scene.registry.AddComponent<VE::MeshComponent>(copy.ecsID,VE::Mesh{},copy.color);
            objects.push_back(copy); sel=(int)objects.size()-1;
        }
        if (ImGui::MenuItem("Delete","Del") && selType==SelectionType::Object && sel>=0) {
            if(scene.IsAlive(objects[sel].ecsID)) scene.DestroyEntity(objects[sel].ecsID);
            objects.erase(objects.begin()+sel);
            if(sel>=(int)objects.size()) sel=(int)objects.size()-1;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...")) g_ShowPreferences = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Create Empty")) addObject(objects, PrimitiveType::Cube, sel, selType);
        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Cube"))     addObject(objects,PrimitiveType::Cube,    sel,selType);
            if (ImGui::MenuItem("Sphere"))   addObject(objects,PrimitiveType::Sphere,  sel,selType);
            if (ImGui::MenuItem("Cylinder")) addObject(objects,PrimitiveType::Cylinder,sel,selType);
            if (ImGui::MenuItem("Pyramid"))  addObject(objects,PrimitiveType::Pyramid, sel,selType);
            if (ImGui::MenuItem("Capsule"))  addObject(objects,PrimitiveType::Capsule, sel,selType);
            if (ImGui::MenuItem("Plane"))    addObject(objects,PrimitiveType::Plane,   sel,selType);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Point Light")) {
            LightObject l; l.name="PointLight_"+std::to_string(lights.size()+1); l.pos=glm::vec3(0,3,0);
            l.ecsID=scene.CreateEntity(l.name); scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);
            lights.push_back(l); selLight=(int)lights.size()-1; selType=SelectionType::Light; logInfo("Created "+l.name);
        }
        if (ImGui::MenuItem("Camera")) {
            CameraObject cam; cam.name="Camera_"+std::to_string(sceneCameras.size()+1); cam.pos=glm::vec3(0,2,5);
            cam.ecsID=scene.CreateEntity(cam.name); scene.registry.AddComponent<VE::CameraComponent>(cam.ecsID,false);
            sceneCameras.push_back(cam); selCamera=(int)sceneCameras.size()-1; selType=SelectionType::Camera; logInfo("Created "+cam.name);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Skybox",  nullptr, &showSkybox);
        ImGui::MenuItem("Grid",    nullptr, &showGrid);
        ImGui::MenuItem("Gizmos",  nullptr, &showGizmos);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Script Editor","Ctrl+E")) logInfo("Use VS Code to edit scripts");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About VisualEngine")) logInfo("VisualEngine v0.1 Beta");
        ImGui::EndMenu();
    }

    // ── Play / Pause / Stop по центру ──
    float mw = ImGui::GetWindowWidth();
    ImGui::SetCursorPosX(mw * 0.5f - 68.f);

    if (isPlaying) {
        // Stop
        ImGui::PushStyleColor(ImGuiCol_Button,        COL_STOP);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,.35f,.35f,1.f));
        if (ImGui::Button(" \xe2\x96\xa0 Stop ", ImVec2(64,22))) {
            isPlaying=false; isPaused=false;
            if(g_MouseCaptured){ g_MouseCaptured=false; glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
            VE::Physics::Get().ClearAllBodies();
            for(int i=0;i<(int)objects.size()&&i<(int)savedTransforms.size();i++){
                objects[i].pos=savedTransforms[i].pos; objects[i].rot=savedTransforms[i].rot; objects[i].scale=savedTransforms[i].scale;
                if(objects[i].hasRigidBody&&scene.registry.HasComponent<VE::RigidbodyComponent>(objects[i].ecsID)){
                    auto& rb=scene.registry.GetComponent<VE::RigidbodyComponent>(objects[i].ecsID); rb.Stop();
                }
                auto& tr=scene.GetTransform(objects[i].ecsID);
                tr.Position=objects[i].pos; tr.Rotation=objects[i].rot; tr.Scale=objects[i].scale;
            }
            for(auto& obj:objects){ obj.luaInstances.clear(); }
            logInfo("Stop — scene restored");
        }
        ImGui::PopStyleColor(2);
    } else {
        // Play
        ImGui::PushStyleColor(ImGuiCol_Button,        COL_PLAY);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.30f,.90f,.40f,1.f));
        if (ImGui::Button(" \xe2\x96\xb6 Play ", ImVec2(64,22))) {
            StartPlay();
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine(0,3);

    // Pause
    ImVec4 pcol = isPaused ? COL_PAUSE : ImVec4(0.15f,0.13f,0.20f,1.f);
    ImGui::PushStyleColor(ImGuiCol_Button, pcol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.90f,.70f,.15f,1.f));
    if (ImGui::Button(" \xe2\x8f\xb8 ", ImVec2(28,22))) isPaused=!isPaused;
    ImGui::PopStyleColor(2);

    // Статистика справа
    ImGui::SetCursorPosX(mw - 200.f);
    if (isPlaying) { ImGui::TextColored(COL_PLAY,"  \xe2\x97\x8f "); ImGui::SameLine(0,0); }
    ImGui::TextColored(COL_DIM, "FPS:");
    ImGui::SameLine(0,3);
    ImGui::TextColored(COL_GREEN, "%.0f", io.Framerate);
    ImGui::SameLine(0,10);
    ImGui::TextColored(COL_DIM, "ECS:");
    ImGui::SameLine(0,3);
    ImGui::TextColored(COL_ACCENT_HOV, "%u", scene.EntityCount());

    ImGui::EndMainMenuBar();
}

// ═══════════════════════════════════════════════════════
//   DOCKSPACE — область докинга (Hierarchy/Viewport/Inspector/Bottom)
// ═══════════════════════════════════════════════════════
static bool dockLayoutBuilt = false;
ImGuiID dockspaceId = 0;
{
    ImGui::SetNextWindowPos(ImVec2(sideW, menuH+toolH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x-sideW, io.DisplaySize.y-menuH-toolH), ImGuiCond_Always);

    ImGuiWindowFlags dockHostFlags =
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("##EditorDockHost", nullptr, dockHostFlags);
    ImGui::PopStyleVar(3);

    dockspaceId = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0,0), ImGuiDockNodeFlags_None);
    ImGui::End();

    // ── Дефолтный layout строится один раз при первом запуске ──
    if (!dockLayoutBuilt) {
        dockLayoutBuilt = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(io.DisplaySize.x-sideW, io.DisplaySize.y-menuH-toolH));

        ImGuiID dmain = dockspaceId;
        ImGuiID dLeft, dRight, dBottom;
        ImGui::DockBuilderSplitNode(dmain, ImGuiDir_Left,  0.20f, &dLeft,  &dmain);
        ImGui::DockBuilderSplitNode(dmain, ImGuiDir_Right, 0.22f, &dRight, &dmain);
        ImGui::DockBuilderSplitNode(dmain, ImGuiDir_Down,  0.28f, &dBottom,&dmain);

        ImGui::DockBuilderDockWindow("Hierarchy##leftpanel", dLeft);
        ImGui::DockBuilderDockWindow("  Inspector ",         dRight);
        ImGui::DockBuilderDockWindow("Viewport##viewport",   dmain);
        ImGui::DockBuilderDockWindow("Bottom##bottom",       dBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }
}

// ═══════════════════════════════════════════════════════
//   PREFERENCES WINDOW — как Editor Settings в Godot
// ═══════════════════════════════════════════════════════
if (g_ShowPreferences) {
    static int prefCat = 0;
    ImGui::SetNextWindowSize(ImVec2(640,440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f,0.5f));
    ImGui::Begin("Editor Settings", &g_ShowPreferences, ImGuiWindowFlags_NoDocking);

    ImGui::BeginChild("##prefcats", ImVec2(150,-32), true);
    const char* cats[] = { "General", "Interface", "Viewport", "Shortcuts" };
    for (int i=0;i<4;i++) {
        if (ImGui::Selectable(cats[i], prefCat==i)) prefCat = i;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##prefbody", ImVec2(0,-32), true);

    if (prefCat==0) { // General
        ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "General");
        ImGui::Separator(); ImGui::Spacing();
        ImGui::Checkbox("Autosave enabled", &g_Prefs.autosaveEnabled);
        ImGui::BeginDisabled(!g_Prefs.autosaveEnabled);
        ImGui::SliderFloat("Autosave interval (min)", &g_Prefs.autosaveMinutes, 1.0f, 30.0f, "%.0f");
        ImGui::EndDisabled();
        ImGui::Spacing();
        char pathBuf[256];
        strncpy(pathBuf, g_Prefs.defaultProjectPath.c_str(), sizeof(pathBuf)-1);
        pathBuf[sizeof(pathBuf)-1]='\0';
        if (ImGui::InputText("Default project path", pathBuf, sizeof(pathBuf)))
            g_Prefs.defaultProjectPath = pathBuf;
    }
    else if (prefCat==1) { // Interface
        ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "Interface");
        ImGui::Separator(); ImGui::Spacing();
        ImGui::ColorEdit3("Accent color", (float*)&g_Prefs.accentColor);
        ImGui::SliderFloat("UI scale", &g_Prefs.uiScale, 0.75f, 1.5f, "%.2f");
        ImGui::TextColored(COL_DIM, "  UI scale applies after restart");
    }
    else if (prefCat==2) { // Viewport
        ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "Viewport");
        ImGui::Separator(); ImGui::Spacing();
        if (ImGui::SliderFloat("Camera move speed", &g_Prefs.camSpeed, 0.5f, 30.0f, "%.1f"))
            camera.Speed = g_Prefs.camSpeed;
        if (ImGui::SliderFloat("Mouse sensitivity", &g_Prefs.camSensitivity, 0.02f, 0.5f, "%.2f"))
            camera.Sensitivity = g_Prefs.camSensitivity;
        ImGui::Checkbox("Invert Y look", &g_Prefs.invertY);
    }
    else if (prefCat==3) { // Shortcuts
        ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "Shortcuts");
        ImGui::Separator(); ImGui::Spacing();
        ImGui::TextColored(COL_DIM, "  Read-only for now — remapping coming later");
        ImGui::Spacing();
        struct SC{const char* action; const char* key;};
        static const SC scs[] = {
            {"Select tool","Q"}, {"Move tool","W"}, {"Rotate tool","E"}, {"Scale tool","R"},
            {"Save Scene","Ctrl+S"}, {"Open Scene","Ctrl+O"}, {"New Scene","Ctrl+N"},
            {"Undo","Ctrl+Z"}, {"Redo","Ctrl+Y"}, {"Play/Stop","Ctrl+P"},
            {"Camera move","W A S D"}, {"Camera look","RMB + Mouse"},
        };
        if (ImGui::BeginTable("##sctbl", 2, ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV)) {
            for (auto& s : scs) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%s", s.action);
                ImGui::TableSetColumnIndex(1); ImGui::TextColored(COL_DIM, "%s", s.key);
            }
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();

    if (ImGui::Button("Save", ImVec2(80,0))) { g_Prefs.Save(); logInfo("Preferences saved"); }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(80,0))) { g_Prefs.Save(); g_ShowPreferences=false; }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════
//   ENVIRONMENT WINDOW — время суток, облака
// ═══════════════════════════════════════════════════════
// ───────────────────────────────────────────────────────
//   TOOLBAR
// ───────────────────────────────────────────────────────
ImGui::SetNextWindowPos(ImVec2(0, menuH), ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, toolH), ImGuiCond_Always);
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,4));
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f,0.094f,0.106f,1.f));
ImGui::Begin("##toolbar", nullptr,
    ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
    ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|
    ImGuiWindowFlags_NoScrollbar|0);

ImGui::SetCursorPosY(5);

// Gizmo кнопки
if (ToggleBtn("  Select", gizmoMode==GizmoMode::Select, ImVec2(64,24))) gizmoMode=GizmoMode::Select;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select (Q)");
ImGui::SameLine(0,2);
if (ToggleBtn("  Move", gizmoMode==GizmoMode::Move, ImVec2(56,24))) gizmoMode=GizmoMode::Move;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move (W)");
ImGui::SameLine(0,2);
if (ToggleBtn("  Rotate", gizmoMode==GizmoMode::Rotate, ImVec2(64,24))) gizmoMode=GizmoMode::Rotate;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (E)");
ImGui::SameLine(0,2);
if (ToggleBtn("  Scale", gizmoMode==GizmoMode::Scale, ImVec2(58,24))) gizmoMode=GizmoMode::Scale;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (R)");

ImGui::SameLine(0,16);
// Разделитель-линия
ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.176f,0.188f,0.212f,1.f));
ImGui::Text("|");
ImGui::PopStyleColor();
ImGui::SameLine(0,16);

// Вьюпорт-тоглы
if (ToggleBtn("Sky",    showSkybox, ImVec2(38,24))) showSkybox=!showSkybox;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Skybox");
ImGui::SameLine(0,2);
if (ToggleBtn(" \xe2\x9a\x99 ", selType==SelectionType::Environment, ImVec2(28,24))) selType=SelectionType::Environment;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lighting: Time of Day, Fog");
ImGui::SameLine(0,2);
if (ToggleBtn("Grid",   showGrid,   ImVec2(40,24))) showGrid=!showGrid;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Grid");
ImGui::SameLine(0,2);
if (ToggleBtn("Gizmos", showGizmos, ImVec2(58,24))) showGizmos=!showGizmos;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle Gizmos");

ImGui::End();
ImGui::PopStyleColor();
ImGui::PopStyleVar();

// ───────────────────────────────────────────────────────
// ───────────────────────────────────────────────────────
//   SIDE ICON PANEL (like concept - left vertical bar)
// ───────────────────────────────────────────────────────
ImGui::SetNextWindowPos(ImVec2(0, menuH+toolH), ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(sideW, viewH+bottomH), ImGuiCond_Always);
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f,0.086f,0.098f,1.f));
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4,6));
ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,2));
ImGui::Begin("##sidepanel", nullptr,
    ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
    ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|
    ImGuiWindowFlags_NoBringToFrontOnFocus);
{
    // Draw active indicator line on left edge
    auto SideIconBtn = [&](const char* label, const char* tooltip, int tabIdx) -> bool {
        bool active = (g_SideTab == tabIdx);
        if (active) {
            // Blue active line on left
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x-4, p.y+2),
                ImVec2(p.x-1, p.y+30),
                IM_COL32(180,185,200,255), 2.f);
        }
        ImGui::PushStyleColor(ImGuiCol_Button,
            active ? ImVec4(0.15f,0.30f,0.55f,0.4f) : ImVec4(0.f,0.f,0.f,0.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.16f,0.18f,0.22f,1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            active ? ImVec4(0.88f,0.89f,0.92f,1.f) : ImVec4(0.40f,0.42f,0.47f,1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
        bool clicked = ImGui::Button(label, ImVec2(40,36));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
        if (clicked) g_SideTab = tabIdx;
        return clicked;
    };

    ImGui::Spacing();
    SideIconBtn("Hier", "Hierarchy (H)",  0);

    // Push settings/help to bottom
    float bottomY = ImGui::GetWindowHeight() - 80.f;
    if (ImGui::GetCursorPosY() < bottomY)
        ImGui::SetCursorPosY(bottomY);

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.176f,0.188f,0.212f,1.f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Settings & Help (no tab switching, just icons)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f,0.f,0.f,0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f,0.18f,0.22f,1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f,0.52f,0.58f,1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    if (ImGui::Button("Sett", ImVec2(40,36))) logInfo("Settings (coming soon)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Settings");
    ImGui::Spacing();
    if (ImGui::Button("Help", ImVec2(40,36))) logInfo("Help (coming soon)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Help");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}
ImGui::End();
ImGui::PopStyleVar(2);
ImGui::PopStyleColor();

//   HIERARCHY
// ───────────────────────────────────────────────────────
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f,0.094f,0.106f,1.f));
const char* leftPanelTitle = "Hierarchy";
ImGui::Begin("Hierarchy##leftpanel", nullptr, ImGuiWindowFlags_NoCollapse);

// Panel title bar
ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f,0.094f,0.106f,1.f));
ImGui::BeginChild("##lefttitle", ImVec2(-1, 28), false);
ImGui::SetCursorPosY(6);
ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "  %s", leftPanelTitle);
ImGui::EndChild();
ImGui::PopStyleColor();
ImGui::Separator();

if (g_SideTab == 0) { // HIERARCHY

// Поиск + кнопка добавить
ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.09f,0.13f,1.f));
ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40);
ImGui::InputTextWithHint("##hs", "\xf0\x9f\x94\x8d  Search...", hierSearch, sizeof(hierSearch));
ImGui::PopStyleColor();
ImGui::SameLine(0,4);
ImGui::PushStyleColor(ImGuiCol_Button,        COL_ACCENT);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
if (ImGui::Button(" + ", ImVec2(34,22))) ImGui::OpenPopup("##addobj");
ImGui::PopStyleColor(2);

if (ImGui::BeginPopup("##addobj")) {
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  3D Objects"); ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::MenuItem("  Cube"))     addObject(objects,PrimitiveType::Cube,    sel,selType);
    if (ImGui::MenuItem("  Sphere"))   addObject(objects,PrimitiveType::Sphere,  sel,selType);
    if (ImGui::MenuItem("  Cylinder")) addObject(objects,PrimitiveType::Cylinder,sel,selType);
    if (ImGui::MenuItem("  Pyramid"))  addObject(objects,PrimitiveType::Pyramid, sel,selType);
    if (ImGui::MenuItem("  Capsule"))  addObject(objects,PrimitiveType::Capsule, sel,selType);
    if (ImGui::MenuItem("  Plane"))    addObject(objects,PrimitiveType::Plane,   sel,selType);
    if (ImGui::MenuItem("  Empty Model")){
        SceneObject o;
        o.name="Model_"+std::to_string(objects.size()+1);
        o.type=PrimitiveType::Model3D;
        o.color=glm::vec3(0.8f,0.8f,0.8f);
        o.ecsID=scene.CreateEntity(o.name);
        scene.GetTransform(o.ecsID).Position=o.pos;
        scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
        objects.push_back(o);sel=(int)objects.size()-1;selType=SelectionType::Object;
        logInfo("Created empty model object: "+o.name);
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  Lights & Cameras"); ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::MenuItem("  Point Light")) {
        LightObject l; l.name="PointLight_"+std::to_string(lights.size()+1); l.pos=glm::vec3(0,3,0);
        l.ecsID=scene.CreateEntity(l.name); scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);
        lights.push_back(l); selLight=(int)lights.size()-1; selType=SelectionType::Light; logInfo("Created "+l.name);
    }
    if (ImGui::MenuItem("  Camera")) {
        CameraObject cam; cam.name="Camera_"+std::to_string(sceneCameras.size()+1); cam.pos=glm::vec3(0,2,5);
        cam.ecsID=scene.CreateEntity(cam.name); scene.registry.AddComponent<VE::CameraComponent>(cam.ecsID,false);
        sceneCameras.push_back(cam); selCamera=(int)sceneCameras.size()-1; selType=SelectionType::Camera; logInfo("Created "+cam.name);
    }
    ImGui::EndPopup();
}

HRule();

// Сцена дерево
ImGui::SetNextItemOpen(true, ImGuiCond_Once);
ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT_HOV);
bool sceneOpen = ImGui::TreeNodeEx("  Untitled Scene", ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_DefaultOpen);
ImGui::PopStyleColor();

if (sceneOpen) {
    // ── Lighting — как сервис в Roblox: постоянный пункт, не объект сцены ──
    {
        bool isEnvSel = (selType == SelectionType::Environment);
        ImGui::PushStyleColor(ImGuiCol_Text, isEnvSel ? ImVec4(1,1,1,1) : ImVec4(1.0f,0.85f,0.4f,1.f));
        if (isEnvSel) ImGui::PushStyleColor(ImGuiCol_Header, COL_ACCENT);
        ImGui::Selectable("  [W] Lighting", isEnvSel);
        if (ImGui::IsItemClicked()) { selType = SelectionType::Environment; }
        if (isEnvSel) ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }
    // Objects
    for (int i = 0; i < (int)objects.size(); i++) {
        auto& obj = objects[i];
        if (obj.parentIndex >= 0) continue;
        std::string filter(hierSearch);
        if (!filter.empty() && obj.name.find(filter)==std::string::npos) continue;

        const char* icons[] = {"[#]","[o]","[|]","[^]","[*]","[-]","[M]"};
        bool hasChildren = false;
        for (int j=0;j<(int)objects.size();j++) if(objects[j].parentIndex==i){hasChildren=true;break;}

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selType==SelectionType::Object && i==sel) {
            flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f,0.14f,0.38f,1.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.28f,0.18f,0.48f,1.f));
        }

        std::string label = std::string("  ")+icons[(int)obj.type]+" "+obj.name;
        bool nodeOpen = hasChildren ? ImGui::TreeNodeEx(label.c_str(), flags) : (ImGui::TreeNodeEx(label.c_str(), flags), false);

        if (selType==SelectionType::Object && i==sel) ImGui::PopStyleColor(2);

        if (ImGui::IsItemClicked()) { sel=i; selType=SelectionType::Object; }

        // ── Drop target: перетащи .mat прямо на объект в Hierarchy ──
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                std::string matPath((const char*)payload->Data, payload->DataSize-1);
                logInfo("[DnD] Received: "+matPath);
                Material loaded = LoadMaterial(matPath);
                auto& tobj = objects[i];
                if (tobj.materials.empty()) tobj.materials.push_back(loaded);
                else tobj.materials[tobj.activeMaterial] = loaded;
                if (tobj.activeMaterial==0) {
                    tobj.color = loaded.color;
                    tobj.texturePath = loaded.texturePath;
                    tobj.textureID = loaded.textureID;
                }
                logInfo("Material '"+loaded.name+"' -> "+tobj.name+" (dropped)");
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
                std::string scriptPath((const char*)payload->Data, payload->DataSize-1);
                auto& tobj = objects[i];
                bool already=false;
                for(auto& sp:tobj.scriptPaths) if(sp==scriptPath){ already=true; break; }
                if(!already){ tobj.scriptPaths.push_back(scriptPath); tobj.hasScript=true; }
                logInfo("Script '"+fs::path(scriptPath).filename().string()+"' -> "+tobj.name+" (dropped)");
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem()) {
            static char s_HierRenameBuf[128] = {};
            static int  s_HierRenameTarget = -1;

            if (ImGui::MenuItem("  Duplicate")) {
                SceneObject o=objects[i]; o.name=o.name+"_copy"; o.pos.x+=1.f;
                o.ecsID=scene.CreateEntity(o.name);
                scene.GetTransform(o.ecsID).Position=o.pos;
                scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                o.luaInstances.clear();
                objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                logInfo("Duplicated: "+o.name);
            }
            if (ImGui::MenuItem("  Rename")) {
                s_HierRenameTarget = i;
                strncpy(s_HierRenameBuf, objects[i].name.c_str(), sizeof(s_HierRenameBuf)-1);
                ImGui::OpenPopup("##hier_rename");
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, COL_RED_X);
            if (ImGui::MenuItem("  Delete")) {
                ImGui::PopStyleColor();
                if(scene.IsAlive(obj.ecsID)) scene.DestroyEntity(obj.ecsID);
                logInfo("Deleted: "+objects[i].name);
                objects.erase(objects.begin()+i);
                sel=(int)objects.size()-1;
                if(objects.empty()){sel=-1;selType=SelectionType::None;}
                ImGui::EndPopup(); if(hasChildren) ImGui::TreePop(); break;
            } else {
                ImGui::PopStyleColor();
            }

            // Rename inline popup
            if (ImGui::BeginPopup("##hier_rename")) {
                ImGui::Text("Rename object:");
                ImGui::SetNextItemWidth(200);
                bool enter = ImGui::InputText("##hrn", s_HierRenameBuf, sizeof(s_HierRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();
                if ((ImGui::Button("OK") || enter) && s_HierRenameTarget>=0 && s_HierRenameTarget<(int)objects.size()) {
                    objects[s_HierRenameTarget].name = s_HierRenameBuf;
                    logInfo("Renamed to: "+std::string(s_HierRenameBuf));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::EndPopup();
        }

        if (hasChildren && nodeOpen) {
            for (int j=0;j<(int)objects.size();j++) {
                if(objects[j].parentIndex!=i) continue;
                ImGuiTreeNodeFlags cf=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if(selType==SelectionType::Object&&j==sel) cf|=ImGuiTreeNodeFlags_Selected;
                ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
                ImGui::TreeNodeEx(("    > "+objects[j].name).c_str(), cf);
                ImGui::PopStyleColor();
                if(ImGui::IsItemClicked()){sel=j;selType=SelectionType::Object;}
            }
            ImGui::TreePop();
        }
    }
    // Lights
    for (int i=0;i<(int)lights.size();i++) {
        ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(selType==SelectionType::Light&&i==selLight) flags|=ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Text, COL_LIGHT_OBJ);
        ImGui::TreeNodeEx(("  [L] "+lights[i].name).c_str(), flags);
        ImGui::PopStyleColor();
        if(ImGui::IsItemClicked()){selLight=i;selType=SelectionType::Light;}
    }
    // Cameras
    for (int i=0;i<(int)sceneCameras.size();i++) {
        ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(selType==SelectionType::Camera&&i==selCamera) flags|=ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Text, COL_CAM_OBJ);
        ImGui::TreeNodeEx(("  [C] "+sceneCameras[i].name).c_str(), flags);
        ImGui::PopStyleColor();
        if(ImGui::IsItemClicked()){selCamera=i;selType=SelectionType::Camera;}
    }
    ImGui::TreePop();
}

} // end g_SideTab == 0 (Hierarchy)

ImGui::End();
ImGui::PopStyleColor();

// ───────────────────────────────────────────────────────
//   VIEWPORT
// ───────────────────────────────────────────────────────
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.071f,0.078f,0.090f,1.f));
ImGui::PushStyleColor(ImGuiCol_Tab,         ImVec4(0.08f,0.07f,0.11f,1.f));
ImGui::PushStyleColor(ImGuiCol_TabActive,   ImVec4(0.16f,0.11f,0.26f,1.f));
ImGui::PushStyleColor(ImGuiCol_TabHovered,  ImVec4(0.22f,0.15f,0.35f,1.f));
ImGui::Begin("Viewport##viewport", nullptr,
    ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);

if (ImGui::BeginTabBar("##vptabs")) {
    if (ImGui::BeginTabItem("  Scene")) {
        float tw=ImGui::GetContentRegionAvail().x, th=ImGui::GetContentRegionAvail().y;
        g_VpPos=ImGui::GetCursorScreenPos(); g_VpSize=ImVec2(tw,th);
        // UV matches viewport/FBO ratio
        float u2 = g_VpSize.x > 0 ? g_VpSize.x/3840.f : 1.f;
        float v1 = g_VpSize.y > 0 ? g_VpSize.y/2160.f : 1.f;
        ImGui::Image((ImTextureID)(intptr_t)sceneTex, ImVec2(tw,th), ImVec2(0,v1), ImVec2(u2,0));

        // ── Drop target: raycast-based material drop (like Godot/Unity) ──
        if (ImGui::BeginDragDropTarget()) {
            // Во время hover — подсвечиваем объект под курсором
            ImVec2 mp = ImGui::GetIO().MousePos;
            double lx = mp.x - g_VpPos.x;
            double ly = mp.y - g_VpPos.y;
            if (lx>=0 && ly>=0 && lx<g_VpSize.x && ly<g_VpSize.y) {
                Ray hray = screenToRay(lx, ly, (int)g_VpSize.x, (int)g_VpSize.y, view, proj);
                float bestT = 1e9f;
                g_DragHoverObj = -1;
                for (int oi=0; oi<(int)objects.size(); oi++) {
                    glm::vec3 hs = objects[oi].scale * 0.5f;
                    float t;
                    if (rayAABB(hray, objects[oi].pos, hs, t) && t < bestT) {
                        bestT = t; g_DragHoverObj = oi;
                    }
                }
            }

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                std::string matPath((const char*)payload->Data, payload->DataSize-1);
                // Применяем к объекту под курсором, или к выбранному если нет под курсором
                int targetObj = (g_DragHoverObj >= 0) ? g_DragHoverObj
                              : (selType==SelectionType::Object && sel>=0) ? sel : -1;
                if (targetObj >= 0 && targetObj < (int)objects.size()) {
                    Material loaded = LoadMaterial(matPath);
                    auto& tobj = objects[targetObj];
                    if (tobj.materials.empty()) tobj.materials.push_back(loaded);
                    else tobj.materials[tobj.activeMaterial] = loaded;
                    if (tobj.activeMaterial==0) {
                        tobj.color = loaded.color;
                        tobj.texturePath = loaded.texturePath;
                        tobj.textureID = loaded.textureID;
                    }
                    sel = targetObj; selType = SelectionType::Object;
                    logInfo("Material '"+loaded.name+"' -> "+tobj.name);
                } else {
                    logWarn("No object under cursor to apply material");
                }
                g_DragHoverObj = -1;
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
                std::string scriptPath((const char*)payload->Data, payload->DataSize-1);
                int targetObj = (g_DragHoverObj >= 0) ? g_DragHoverObj
                              : (selType==SelectionType::Object && sel>=0) ? sel : -1;
                if (targetObj >= 0 && targetObj < (int)objects.size()) {
                    auto& tobj = objects[targetObj];
                    bool already=false;
                    for(auto& sp:tobj.scriptPaths) if(sp==scriptPath){ already=true; break; }
                    if(!already){ tobj.scriptPaths.push_back(scriptPath); tobj.hasScript=true; }
                    sel = targetObj; selType = SelectionType::Object;
                    logInfo("Script '"+fs::path(scriptPath).filename().string()+"' -> "+tobj.name);
                } else {
                    logWarn("No object under cursor to attach script");
                }
                g_DragHoverObj = -1;
            }
            ImGui::EndDragDropTarget();
        }

        // Подсветка объекта под курсором во время drag&drop
        if (g_DragHoverObj >= 0 && g_DragHoverObj < (int)objects.size()) {
            auto* dndDl = ImGui::GetWindowDrawList();
            // Рисуем пульсирующий контур вокруг названия объекта
            ImVec2 hintPos = ImVec2(g_VpPos.x + 8, g_VpPos.y + 8);
            dndDl->AddText(hintPos, IM_COL32(255,200,80,220),
                ("Drop material on: "+objects[g_DragHoverObj].name).c_str());
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            // GetMouseDragDelta корректно работает и на кадре отпускания кнопки
            // (в отличие от IsMouseDragging, которая требует, чтобы кнопка ещё была зажата)
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 5.0f);
            if (dragDelta.x == 0.0f && dragDelta.y == 0.0f)
                ImGui::OpenPopup("##scene_ctx");
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
        }

        if (ImGui::BeginPopup("##scene_ctx")) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.57f,0.62f,1.f));
            ImGui::Text("  Create Object"); ImGui::PopStyleColor();
            ImGui::Separator();

            // 3D Objects submenu
            if (ImGui::BeginMenu("  3D Object")) {
                auto spawnObj = [&](const char* n, PrimitiveType t){
                    SceneObject o; o.name=n; o.type=t;
                    o.pos=glm::vec3(0,.5f,0); o.color=glm::vec3(0.8f,0.8f,0.8f);
                    o.ecsID=scene.CreateEntity(o.name);
                    scene.GetTransform(o.ecsID).Position=o.pos;
                    scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                    objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                    logInfo("Created: "+std::string(n));
                };
                if (ImGui::MenuItem("  Cube"))     spawnObj("Cube",    PrimitiveType::Cube);
                if (ImGui::MenuItem("  Sphere"))   spawnObj("Sphere",  PrimitiveType::Sphere);
                if (ImGui::MenuItem("  Cylinder")) spawnObj("Cylinder",PrimitiveType::Cylinder);
                if (ImGui::MenuItem("  Plane"))    spawnObj("Plane",   PrimitiveType::Plane);
                if (ImGui::MenuItem("  Capsule"))  spawnObj("Capsule", PrimitiveType::Capsule);
                if (ImGui::MenuItem("  Pyramid"))  spawnObj("Pyramid", PrimitiveType::Pyramid);
                ImGui::EndMenu();
            }

            // Light submenu
            if (ImGui::BeginMenu("  Light")) {
                auto spawnLight = [&](const char* n){
                    LightObject l; l.name=n+std::to_string(lights.size()+1);
                    l.pos=glm::vec3(0,3,0); l.color=glm::vec3(1,1,1); l.intensity=1.f; l.range=20.f;
                    l.ecsID=scene.CreateEntity(l.name);
                    scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);
                    lights.push_back(l); selLight=(int)lights.size()-1; selType=SelectionType::Light;
                    logInfo("Created light: "+l.name);
                };
                if (ImGui::MenuItem("  Point Light"))       spawnLight("PointLight_");
                if (ImGui::MenuItem("  Directional Light")) spawnLight("DirLight_");
                if (ImGui::MenuItem("  Spot Light"))        spawnLight("SpotLight_");
                ImGui::EndMenu();
            }

            // Camera
            if (ImGui::MenuItem("  Camera")) {
                CameraObject cam;
                cam.name="GameCamera_"+std::to_string(sceneCameras.size()+1);
                cam.pos=glm::vec3(0,1,5); cam.fov=45.f; cam.isPrimary=sceneCameras.empty();
                cam.ecsID=scene.CreateEntity(cam.name);
                scene.registry.AddComponent<VE::CameraComponent>(cam.ecsID,cam.isPrimary);
                sceneCameras.push_back(cam); selCamera=(int)sceneCameras.size()-1; selType=SelectionType::Camera;
                logInfo("Created: "+cam.name);
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.57f,0.62f,1.f));
            ImGui::Text("  Edit"); ImGui::PopStyleColor();
            ImGui::Separator();

            // Copy/Paste/Duplicate selected
            bool hasSel = (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size());
            if (ImGui::MenuItem("  Duplicate", "Ctrl+D", false, hasSel)) {
                if (hasSel) {
                    SceneObject o = objects[sel];
                    o.name = o.name+"_copy"; o.pos.x+=1.f;
                    o.ecsID=scene.CreateEntity(o.name);
                    scene.GetTransform(o.ecsID).Position=o.pos;
                    scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                    o.luaInstances.clear();
                    objects.push_back(o); sel=(int)objects.size()-1;
                    logInfo("Duplicated: "+o.name);
                }
            }
            if (ImGui::MenuItem("  Delete", "Del", false, hasSel)) {
                if (hasSel) {
                    if(scene.IsAlive(objects[sel].ecsID)) scene.DestroyEntity(objects[sel].ecsID);
                    logInfo("Deleted: "+objects[sel].name);
                    objects.erase(objects.begin()+sel);
                    sel=(int)objects.size()-1;
                    if(objects.empty()){sel=-1;selType=SelectionType::None;}
                }
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f,0.57f,0.62f,1.f));
            ImGui::Text("  Scene"); ImGui::PopStyleColor();
            ImGui::Separator();
            if (ImGui::MenuItem("  Focus on Selected", "F", false, hasSel)) {
                if (hasSel) { camera.Position = objects[sel].pos + glm::vec3(0,1,4); }
            }
            if (ImGui::MenuItem("  Reset Camera")) {
                camera.Position=glm::vec3(0,2,8); camera.Yaw=-90; camera.Pitch=-15;
            }
            if (ImGui::MenuItem("  Save Scene", "Ctrl+S")) {
                if(currentScenePath.empty()) currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
                SaveScene(currentScenePath,objects,lights,sceneCameras);
                logInfo("Scene saved: "+currentScenePath);
            }
            ImGui::EndPopup();
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("  Game")) {
        float tw=ImGui::GetContentRegionAvail().x, th=ImGui::GetContentRegionAvail().y;
        if (isPlaying) {
            ImVec2 gamePos = ImGui::GetCursorScreenPos();
            float u2g = g_VpSize.x > 0 ? g_VpSize.x/3840.f : 1.f;
            float v1g = g_VpSize.y > 0 ? g_VpSize.y/2160.f : 1.f;
            ImGui::Image((ImTextureID)(intptr_t)gameTex, ImVec2(tw,th), ImVec2(0,v1g), ImVec2(u2g,0));
            // ── Захват курсора: клик по Game — прячем и зацикливаем мышь для FPS-камеры ──
            if (ImGui::IsItemClicked() && !g_MouseCaptured) {
                g_MouseCaptured = true;
                glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                g_RawMouseFirst = true; // не дать скачок дельты в первом кадре захвата
            }
            if (g_MouseCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                g_MouseCaptured = false;
                glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            if (g_MouseCaptured) {
                ImGui::SetCursorPos(ImVec2(8,8));
                ImGui::TextColored(COL_DIM, "Esc to release mouse");
            }
            // ── HUD поверх игры ──
            VE::HUD::Get().Draw(gamePos, ImVec2(tw, th));
        } else {
            ImGui::SetCursorPos(ImVec2(tw*0.5f-110.f, th*0.5f-12.f));
            ImGui::TextColored(COL_DIM, "  Press  Play  to  enter  Game  mode");
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
ImGui::End();
ImGui::PopStyleColor(4);
ImGui::PopStyleVar();

// ───────────────────────────────────────────────────────
//   INSPECTOR
// ───────────────────────────────────────────────────────
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f,0.086f,0.098f,1.f));
ImGui::Begin("  Inspector ", nullptr, ImGuiWindowFlags_NoCollapse);

if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
    auto& obj = objects[sel];

    // Заголовок объекта
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.09f,0.14f,1.f));
    ImGui::Checkbox("##act", &obj.active); ImGui::SameLine();
    static char nb[64]; strncpy_s(nb, obj.name.c_str(), sizeof(nb)-1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##nm", nb, sizeof(nb))) obj.name=nb;
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  Tag: Untagged     Layer: Default");
    ImGui::PopStyleColor();
    HRule();

    // Transform
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);
        float lw = 70.f;
        // Position
        ImGui::Text("Position"); ImGui::SameLine(lw);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED_X);
        ImGui::Text("X"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##px",&obj.pos.x,0.05f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN_Y);
        ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##py",&obj.pos.y,0.05f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLUE_Z);
        ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);  ImGui::DragFloat("##pz",&obj.pos.z,0.05f);
        // Rotation
        ImGui::Text("Rotation"); ImGui::SameLine(lw);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED_X);
        ImGui::Text("X"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##rx",&obj.rot.x,0.5f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN_Y);
        ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##ry",&obj.rot.y,0.5f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLUE_Z);
        ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);  ImGui::DragFloat("##rz",&obj.rot.z,0.5f);
        // Scale
        ImGui::Text("Scale"); ImGui::SameLine(lw);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED_X);
        ImGui::Text("X"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##sx",&obj.scale.x,0.05f,0.001f,100.f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN_Y);
        ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##sy",&obj.scale.y,0.05f,0.001f,100.f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLUE_Z);
        ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);  ImGui::DragFloat("##sz",&obj.scale.z,0.05f,0.001f,100.f);
    } else { ImGui::PopStyleColor(2); }

    // Mesh Renderer
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);
        ImGui::Text("Mesh:"); ImGui::SameLine(80);
        ImGui::TextColored(COL_ACCENT_HOV, "%s", typeNames[(int)obj.type]);
        if(obj.type==PrimitiveType::Model3D){
            ImGui::Text("File:"); ImGui::SameLine(80);
            if(obj.modelPath.empty()) ImGui::TextColored(COL_DIM,"None");
            else ImGui::TextColored(COL_GREEN,"%s",fs::path(obj.modelPath).filename().string().c_str());
            if(ImGui::Button("Load Model...",ImVec2(-1,0))){
                // Открыть папку Assets в проводнике для выбора файла
                std::string cmd="explorer "+projectRoot+"\\Assets";
                system(cmd.c_str());
                logInfo("Put your .obj/.fbx/.gltf in Assets folder, then double-click it in Project panel");
            }
        }
        ImGui::Text("Color:"); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1); ImGui::ColorEdit3("##col", glm::value_ptr(obj.color));
    } else { ImGui::PopStyleColor(2); }

    // ── Animation (только для моделей со скелетом и клипами) ──
    if (obj.type==PrimitiveType::Model3D && obj.model && obj.model->hasSkeleton && !obj.model->animations.empty()) {
        HRule();
        ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
        if (ImGui::CollapsingHeader("  Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PopStyleColor(2);

            const char* curName = (obj.animIndex>=0 && obj.animIndex<(int)obj.model->animations.size())
                ? obj.model->animations[obj.animIndex].name.c_str() : "None";
            if (ImGui::BeginCombo("Clip", curName)) {
                for (int a=0; a<(int)obj.model->animations.size(); a++) {
                    bool sel = (obj.animIndex==a);
                    if (ImGui::Selectable(obj.model->animations[a].name.c_str(), sel)) {
                        obj.animIndex = a; obj.animTime = 0.f;
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(obj.animIndex<0);
            if (ImGui::Button(obj.animPlaying ? "  Pause  " : "  Play  ", ImVec2(80,0))) {
                obj.animPlaying = !obj.animPlaying;
            }
            ImGui::SameLine();
            if (ImGui::Button("  Stop  ", ImVec2(80,0))) {
                obj.animPlaying = false; obj.animTime = 0.f;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &obj.animLoop);

            if (obj.animIndex>=0) {
                float dur = obj.model->animations[obj.animIndex].duration / std::max(1.f, obj.model->animations[obj.animIndex].ticksPerSecond);
                ImGui::SliderFloat("Time", &obj.animTime, 0.f, std::max(0.01f,dur), "%.2f s");
            }
            ImGui::EndDisabled();
        } else { ImGui::PopStyleColor(2); }
    }

    // ── Materials ──
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);

        // Гарантируем хотя бы один материал
        if (obj.materials.empty()) {
            Material m; m.name = "Default";
            // Подхватываем старый texturePath если был (обратная совместимость)
            if (!obj.texturePath.empty()) { m.texturePath = obj.texturePath; m.textureID = obj.textureID; }
            m.color = obj.color;
            obj.materials.push_back(m);
        }
        if (obj.activeMaterial < 0 || obj.activeMaterial >= (int)obj.materials.size())
            obj.activeMaterial = 0;

        // Список материалов — горизонтальные вкладки-плашки
        for (int m = 0; m < (int)obj.materials.size(); m++) {
            bool isActive = (m == obj.activeMaterial);
            ImGui::PushID(m);
            ImGui::PushStyleColor(ImGuiCol_Button,
                isActive ? ImVec4(0.20f,0.22f,0.27f,1.f) : ImVec4(0.13f,0.13f,0.15f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f,0.20f,0.24f,1.f));
            if (ImGui::Button(obj.materials[m].name.c_str(), ImVec2(0,24))) obj.activeMaterial = m;
            ImGui::PopStyleColor(2);
            ImGui::PopID();
            if (m < (int)obj.materials.size()-1) ImGui::SameLine(0,4);
        }

        // Кнопки + / -
        ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f,0.16f,0.19f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f,0.22f,0.26f,1.f));
        if (ImGui::Button(" + ", ImVec2(0,24))) {
            Material m; m.name = "Material_"+std::to_string(obj.materials.size()+1);
            obj.materials.push_back(m);
            obj.activeMaterial = (int)obj.materials.size()-1;
            logInfo("Added material slot: "+m.name);
        }
        if (obj.materials.size() > 1) {
            ImGui::SameLine(0,2);
            if (ImGui::Button(" - ", ImVec2(0,24))) {
                obj.materials.erase(obj.materials.begin()+obj.activeMaterial);
                obj.activeMaterial = std::max(0, obj.activeMaterial-1);
            }
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Настройки выбранного материала ──
        Material& mat = obj.materials[obj.activeMaterial];

        ImGui::Text("Name:"); ImGui::SameLine(80);
        static char s_MatNameBuf[64];
        strncpy(s_MatNameBuf, mat.name.c_str(), sizeof(s_MatNameBuf)-1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##matname", s_MatNameBuf, sizeof(s_MatNameBuf)))
            mat.name = s_MatNameBuf;

        ImGui::Text("Color:"); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit3("##matcol", glm::value_ptr(mat.color))) {
            if (obj.activeMaterial == 0) obj.color = mat.color; // главный материал красит весь объект
        }

        ImGui::Text("Texture:"); ImGui::SameLine(80);
        if (mat.texturePath.empty())
            ImGui::TextColored(COL_DIM, "None (double-click an image in Project panel)");
        else
            ImGui::TextColored(COL_GREEN, "%s", fs::path(mat.texturePath).filename().string().c_str());

        if (!mat.texturePath.empty() && ImGui::Button("Clear Texture", ImVec2(-1,0))) {
            mat.texturePath.clear(); mat.textureID = 0;
        }

        ImGui::Spacing();
        ImGui::Text("Roughness:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##rough", &mat.roughness, 0.01f, 0.f, 1.f, "%.2f");

        ImGui::Text("Metallic:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##metal", &mat.metallic, 0.01f, 0.f, 1.f, "%.2f");

        ImGui::Spacing();
        ImGui::Text("Tiling X:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##tilex", &mat.tilingX, 0.05f, 0.1f, 20.f, "%.2f");
        ImGui::Text("Tiling Y:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##tiley", &mat.tilingY, 0.05f, 0.1f, 20.f, "%.2f");

        ImGui::Spacing();
        if (!mat.assetPath.empty()) {
            ImGui::TextColored(COL_DIM, "Asset: %s", fs::path(mat.assetPath).filename().string().c_str());
            if (ImGui::Button("Save Changes to Asset", ImVec2(-1,0))) {
                SaveMaterial(mat.assetPath, mat);
                logInfo("Saved material asset: "+fs::path(mat.assetPath).filename().string());
            }
        } else {
            if (ImGui::Button("Save As Material Asset...", ImVec2(-1,0))) {
                std::string matDir = projectRoot + "\\Assets";
                std::string mp = matDir + "\\" + (mat.name.empty()?"NewMaterial":mat.name) + ".mat";
                SaveMaterial(mp, mat);
                mat.assetPath = mp;
                logInfo("Saved as: "+fs::path(mp).filename().string());
            }
        }

        // Сохраняем активную текстуру в obj.textureID/texturePath для рендера
        // (рендер пока поддерживает один материал — основной/0)
        if (obj.activeMaterial == 0) {
            obj.texturePath = mat.texturePath;
            obj.textureID   = mat.textureID;
        }
    } else { ImGui::PopStyleColor(2); }

    // Rigidbody
    if (obj.hasRigidBody && scene.registry.HasComponent<VE::RigidbodyComponent>(obj.ecsID)) {
        auto& rb = scene.registry.GetComponent<VE::RigidbodyComponent>(obj.ecsID);
        ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
        if (ImGui::CollapsingHeader("  Rigidbody", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PopStyleColor(2);
            ImGui::Text("Mass:");          ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); if(ImGui::DragFloat("##mass",&rb.Mass,0.1f,0.f,1000.f))obj.mass=rb.Mass;
            ImGui::Text("Gravity Scale:"); ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##gs",&rb.GravityScale,0.05f,-5.f,5.f);
            ImGui::Text("Linear Drag:");   ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##ld",&rb.LinearDrag,0.01f,0.f,10.f);
            ImGui::Text("Angular Drag:");  ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##ad",&rb.AngularDrag,0.01f,0.f,10.f);
            ImGui::Text("Use Gravity:");   ImGui::SameLine(110); ImGui::Checkbox("##ug",&rb.UseGravity);
            ImGui::Text("Is Kinematic:");  ImGui::SameLine(110); ImGui::Checkbox("##ik",&rb.IsKinematic);
            HRule();
            ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
            ImGui::Text("Freeze Position:");
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::Checkbox("X##fpx",&rb.FreezePositionX);
            ImGui::SameLine(); ImGui::Checkbox("Y##fpy",&rb.FreezePositionY);
            ImGui::SameLine(); ImGui::Checkbox("Z##fpz",&rb.FreezePositionZ);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
            ImGui::Text("Freeze Rotation:");
            ImGui::PopStyleColor();
            ImGui::SameLine(); ImGui::Checkbox("X##frx",&rb.FreezeRotationX);
            ImGui::SameLine(); ImGui::Checkbox("Y##fry",&rb.FreezeRotationY);
            ImGui::SameLine(); ImGui::Checkbox("Z##frz",&rb.FreezeRotationZ);
            if (scene.registry.HasComponent<VE::ColliderComponent>(obj.ecsID)) {
                auto& col=scene.registry.GetComponent<VE::ColliderComponent>(obj.ecsID);
                HRule();
                ImGui::TextColored(COL_ACCENT_HOV, "  Collider");
                const char* shapes[]={"Box","Sphere","Capsule"};
                int sh=(int)col.Shape;
                ImGui::Text("Shape:"); ImGui::SameLine(110); ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("##sh",&sh,shapes,3)) col.Shape=(VE::ColliderComponent::ShapeType)sh;
                ImGui::Text("Is Trigger:"); ImGui::SameLine(110); ImGui::Checkbox("##tr",&col.IsTrigger);
                ImGui::Text("Is Solid:");   ImGui::SameLine(110); ImGui::Checkbox("##so",&col.IsSolid);
                ImGui::Text("Friction:");   ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##fr",&col.Material.Friction,0.01f,0.f,1.f);
                ImGui::Text("Bounciness:"); ImGui::SameLine(110); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##bn",&col.Material.Bounciness,0.01f,0.f,1.f);
            }
            HRule();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.08f,0.08f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f,0.12f,0.12f,1.f));
            if(ImGui::Button("Remove Rigidbody",ImVec2(-1,0))){
                obj.hasRigidBody=false;
                scene.registry.RemoveComponent<VE::RigidbodyComponent>(obj.ecsID);
                scene.registry.RemoveComponent<VE::ColliderComponent>(obj.ecsID);
            }
            ImGui::PopStyleColor(2);
        } else { ImGui::PopStyleColor(2); }
    }

    // Lua Scripts (можно несколько на объект) — показываем всегда, чтобы было куда перетащить
    {
        ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
        if (ImGui::CollapsingHeader("  Lua Scripts", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PopStyleColor(2);
            if (obj.scriptPaths.empty()) ImGui::TextColored(COL_DIM, "  No scripts attached");
            int removeIdx = -1;
            for (int si=0; si<(int)obj.scriptPaths.size(); si++) {
                ImGui::PushID(si);
                ImGui::TextColored(COL_GREEN, "%s", fs::path(obj.scriptPaths[si]).filename().string().c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Edit")) openInVSCode(obj.scriptPaths[si]);
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.08f,0.08f,1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f,0.12f,0.12f,1.f));
                if (ImGui::SmallButton("X")) removeIdx = si;
                ImGui::PopStyleColor(2);
                ImGui::PopID();
            }
            if (removeIdx>=0) {
                obj.scriptPaths.erase(obj.scriptPaths.begin()+removeIdx);
                obj.hasScript = !obj.scriptPaths.empty();
            }

            // ── Зона для перетаскивания .lua файла из Project — как в Unity ──
            ImGui::Dummy(ImVec2(-1,4));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f,0.10f,0.13f,1.f));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
            ImGui::Button("  Drop .lua script here  ", ImVec2(-1,28));
            ImGui::PopStyleColor(2);
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
                    std::string dropped((const char*)payload->Data);
                    bool already=false;
                    for(auto& sp:obj.scriptPaths) if(sp==dropped){ already=true; break; }
                    if(!already){ obj.scriptPaths.push_back(dropped); obj.hasScript=true; }
                }
                ImGui::EndDragDropTarget();
            }
        } else { ImGui::PopStyleColor(2); }
    }

    HRule();
    ImGui::Spacing();
    // Add Component
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f,0.10f,0.22f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text,          COL_ACCENT_HOV);
    if (ImGui::Button("  +  Add Component  ", ImVec2(-1,28))) ImGui::OpenPopup("##addcomp");
    ImGui::PopStyleColor(3);

    if (ImGui::BeginPopup("##addcomp")) {
        ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM); ImGui::Text("  Components"); ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::MenuItem("  New Lua Script")) {
            // Уникальное имя файла, чтобы не перезаписать чужой скрипт
            std::string base = projectRoot+"\\Assets\\Scripts\\"+obj.name;
            std::string newPath = base+".lua";
            int suffix=1;
            while (fs::exists(newPath)) { newPath = base+"_"+std::to_string(suffix)+".lua"; suffix++; }
            obj.hasScript=true;
            obj.scriptPaths.push_back(newPath);
            scene.AttachScript(obj.ecsID,newPath);
            openInVSCode(newPath);
            logInfo("Script added: "+fs::path(newPath).filename().string());
        }
        if (!obj.hasRigidBody && ImGui::MenuItem("  Rigidbody")) {
            obj.hasRigidBody=true;
            auto& rb=scene.registry.AddComponent<VE::RigidbodyComponent>(obj.ecsID);
            rb.Mass=obj.mass; rb.UseGravity=obj.useGravity;
            auto& col=scene.registry.AddComponent<VE::ColliderComponent>(obj.ecsID);
            col=VE::ColliderComponent::Box({obj.scale.x*0.5f,obj.scale.y*0.5f,obj.scale.z*0.5f});
            logInfo("Rigidbody added to "+obj.name);
        }
        ImGui::EndPopup();
    }
}
else if (selType==SelectionType::None && !assetSelected.empty() &&
         fs::path(assetSelected).extension()==".mat" && fs::exists(assetSelected)) {
    // ── Просмотр/редактирование материала как отдельного ассета ──
    static std::string s_LoadedMatPath;
    static Material    s_EditMat;
    if (s_LoadedMatPath != assetSelected) {
        s_EditMat = LoadMaterial(assetSelected);
        s_LoadedMatPath = assetSelected;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f,0.85f,0.90f,1.f));
    ImGui::Text("  Material Asset");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  %s", fs::path(assetSelected).filename().string().c_str());
    ImGui::PopStyleColor();
    HRule();

    // Превью сферы
    ImVec2 prevCenter = ImGui::GetCursorScreenPos();
    prevCenter.x += ImGui::GetContentRegionAvail().x*0.5f - 24.f; prevCenter.y += 40.f;
    auto* dlp = ImGui::GetWindowDrawList();
    ImU32 sCol = IM_COL32((int)(s_EditMat.color.r*255),(int)(s_EditMat.color.g*255),(int)(s_EditMat.color.b*255),255);
    dlp->AddCircleFilled(prevCenter, 40.f, sCol, 32);
    dlp->AddCircleFilled(ImVec2(prevCenter.x-14,prevCenter.y-14), 10.f, IM_COL32(255,255,255,140), 16);
    ImGui::Dummy(ImVec2(0, 90));

    ImGui::Text("Name:"); ImGui::SameLine(90);
    static char s_AssetMatName[64];
    strncpy(s_AssetMatName, s_EditMat.name.c_str(), sizeof(s_AssetMatName)-1);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##assetmatname", s_AssetMatName, sizeof(s_AssetMatName)))
        s_EditMat.name = s_AssetMatName;

    ImGui::Text("Color:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit3("##assetmatcol", glm::value_ptr(s_EditMat.color));

    ImGui::Text("Texture:"); ImGui::SameLine(90);
    if (s_EditMat.texturePath.empty()) ImGui::TextColored(COL_DIM, "None");
    else ImGui::TextColored(COL_GREEN, "%s", fs::path(s_EditMat.texturePath).filename().string().c_str());

    ImGui::Spacing();
    ImGui::Text("Roughness:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##assetrough", &s_EditMat.roughness, 0.01f, 0.f, 1.f, "%.2f");
    ImGui::Text("Metallic:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##assetmetal", &s_EditMat.metallic, 0.01f, 0.f, 1.f, "%.2f");

    ImGui::Spacing();
    ImGui::Text("Tiling X:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##assettilex", &s_EditMat.tilingX, 0.05f, 0.1f, 20.f, "%.2f");
    ImGui::Text("Tiling Y:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##assettiley", &s_EditMat.tilingY, 0.05f, 0.1f, 20.f, "%.2f");

    ImGui::Spacing(); ImGui::Spacing();
    if (ImGui::Button("Save Material", ImVec2(-1,0))) {
        SaveMaterial(assetSelected, s_EditMat);
        logInfo("Saved: "+fs::path(assetSelected).filename().string());
    }
    ImGui::TextColored(COL_DIM, "Drag this material onto an object\nin the Scene or Hierarchy to apply it.");
}
else if (selType==SelectionType::Environment) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f,0.85f,0.4f,1.f));
    ImGui::Text("  Lighting"); ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  Scene-wide environment settings");
    ImGui::PopStyleColor();
    HRule();

    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Sky & Time", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);
        int hh=(int)g_TimeOfDay, mm=(int)((g_TimeOfDay-hh)*60.f);
        ImGui::Text("Time of Day: %02d:%02d", hh, mm);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##tod", &g_TimeOfDay, 0.0f, 24.0f, "");
        if (ImGui::Button("Dawn",    ImVec2(58,0))) g_TimeOfDay=6.0f;  ImGui::SameLine();
        if (ImGui::Button("Noon",    ImVec2(58,0))) g_TimeOfDay=12.0f; ImGui::SameLine();
        if (ImGui::Button("Dusk",    ImVec2(58,0))) g_TimeOfDay=18.0f; ImGui::SameLine();
        if (ImGui::Button("Midnight",ImVec2(74,0))) g_TimeOfDay=0.0f;
    } else { ImGui::PopStyleColor(2); }

    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Sun & Ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("Sun Intensity", &g_SunIntensity, 0.0f, 2.0f, "%.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("Ambient", &g_AmbientStrength, 0.0f, 0.5f, "%.2f");
        ImGui::TextColored(COL_DIM, "Sun follows Time of Day above —\nfades out automatically at night.");
    } else { ImGui::PopStyleColor(2); }

    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if (ImGui::CollapsingHeader("  Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PopStyleColor(2);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("Density", &g_FogDensity, 0.0f, 1.0f, "%.2f");
        ImGui::ColorEdit3("Color", (float*)&g_FogColor);
    } else { ImGui::PopStyleColor(2); }

    ImGui::Spacing();
    ImGui::TextColored(COL_DIM, "Tip: control this from Lua with\nEnvironment.SetTimeOfDay(hours)\nto animate a day/night cycle.");
}
else if (selType==SelectionType::Light && selLight>=0 && selLight<(int)lights.size()) {
    auto& l=lights[selLight];
    ImGui::Checkbox("##la",&l.active); ImGui::SameLine();
    static char lb[64]; strncpy_s(lb,l.name.c_str(),sizeof(lb)-1);
    ImGui::SetNextItemWidth(-1); if(ImGui::InputText("##ln",lb,sizeof(lb)))l.name=lb;
    ImGui::TextColored(COL_LIGHT_OBJ,"  Point Light"); HRule();
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if(ImGui::CollapsingHeader("  Transform",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PopStyleColor(2);
        ImGui::Text("Position"); ImGui::SameLine(70);
        ImGui::PushStyleColor(ImGuiCol_Text,COL_RED_X);   ImGui::Text("X"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##lpx",&l.pos.x,0.05f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text,COL_GREEN_Y); ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::DragFloat("##lpy",&l.pos.y,0.05f); ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Text,COL_BLUE_Z);  ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
        ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##lpz",&l.pos.z,0.05f);
    } else { ImGui::PopStyleColor(2); }
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if(ImGui::CollapsingHeader("  Light",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PopStyleColor(2);
        ImGui::Text("Color:");     ImGui::SameLine(80); ImGui::SetNextItemWidth(-1); ImGui::ColorEdit3("##lc",glm::value_ptr(l.color));
        ImGui::Text("Intensity:"); ImGui::SameLine(80); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##li",&l.intensity,0.05f,0.f,10.f);
        ImGui::Text("Range:");     ImGui::SameLine(80); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##lr",&l.range,0.1f,0.1f,100.f);
    } else { ImGui::PopStyleColor(2); }
}
else if (selType==SelectionType::Camera && selCamera>=0 && selCamera<(int)sceneCameras.size()) {
    auto& cam=sceneCameras[selCamera];
    ImGui::Checkbox("##ca",&cam.active); ImGui::SameLine();
    static char cb[64]; strncpy_s(cb,cam.name.c_str(),sizeof(cb)-1);
    ImGui::SetNextItemWidth(-1); if(ImGui::InputText("##cn",cb,sizeof(cb)))cam.name=cb;
    ImGui::TextColored(COL_CAM_OBJ,"  Camera"); HRule();
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if(ImGui::CollapsingHeader("  Transform",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PopStyleColor(2);
        if(cam.followTargetIndex>=0 && cam.followTargetIndex<(int)objects.size()){
            glm::vec3 livePos = objects[cam.followTargetIndex].pos + cam.followOffset;
            ImGui::TextColored(COL_DIM,"Position (live, follows target):");
            ImGui::Text("  %.2f, %.2f, %.2f", livePos.x, livePos.y, livePos.z);
            ImGui::TextColored(COL_DIM,"Edit \"Offset\" below to reposition the camera");
        } else {
            ImGui::Text("Position"); ImGui::SameLine(70);
            ImGui::PushStyleColor(ImGuiCol_Text,COL_RED_X);   ImGui::Text("X"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(60); if(ImGui::DragFloat("##cpx",&cam.pos.x,0.05f))gameCamera.Position=cam.pos; ImGui::SameLine(0,4);
            ImGui::PushStyleColor(ImGuiCol_Text,COL_GREEN_Y); ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(60); if(ImGui::DragFloat("##cpy",&cam.pos.y,0.05f))gameCamera.Position=cam.pos; ImGui::SameLine(0,4);
            ImGui::PushStyleColor(ImGuiCol_Text,COL_BLUE_Z);  ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(-1); if(ImGui::DragFloat("##cpz",&cam.pos.z,0.05f))gameCamera.Position=cam.pos;
        }
    } else { ImGui::PopStyleColor(2); }
    ImGui::PushStyleColor(ImGuiCol_Header,       ImVec4(0.12f,0.09f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f,0.13f,0.30f,1.f));
    if(ImGui::CollapsingHeader("  Camera",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PopStyleColor(2);
        ImGui::Text("FOV:");     ImGui::SameLine(80); ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##fov",&cam.fov,0.5f,10.f,120.f);
        ImGui::Text("Primary:"); ImGui::SameLine(80); ImGui::Checkbox("##pri",&cam.isPrimary);
        ImGui::Separator();
        ImGui::Text("Follow Target:");
        std::string curName = (cam.followTargetIndex>=0 && cam.followTargetIndex<(int)objects.size())
                               ? objects[cam.followTargetIndex].name : "None (free fly)";
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##followTarget", curName.c_str())){
            if(ImGui::Selectable("None (free fly)", cam.followTargetIndex==-1)) cam.followTargetIndex=-1;
            for(int oi=0; oi<(int)objects.size(); oi++){
                bool isSelObj = (cam.followTargetIndex==oi);
                if(ImGui::Selectable(objects[oi].name.c_str(), isSelObj)) cam.followTargetIndex=oi;
            }
            ImGui::EndCombo();
        }
        if(cam.followTargetIndex>=0){
            ImGui::Text("Offset:"); ImGui::SameLine(70);
            ImGui::SetNextItemWidth(60); ImGui::DragFloat("##fox",&cam.followOffset.x,0.05f); ImGui::SameLine(0,4);
            ImGui::SetNextItemWidth(60); ImGui::DragFloat("##foy",&cam.followOffset.y,0.05f); ImGui::SameLine(0,4);
            ImGui::SetNextItemWidth(-1); ImGui::DragFloat("##foz",&cam.followOffset.z,0.05f);
            ImGui::TextColored(COL_DIM,"Camera follows this object's position & rotation in Play mode");
        }
    } else { ImGui::PopStyleColor(2); }
}
else {
    ImGui::Spacing(); ImGui::Spacing();
    float tw = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((tw - ImGui::CalcTextSize("Nothing selected").x)*0.5f);
    ImGui::TextColored(COL_DIM, "Nothing selected");
    ImGui::Spacing();
    ImGui::SetCursorPosX((tw - ImGui::CalcTextSize("Click an object in the Outliner").x)*0.5f);
    ImGui::TextColored(ImVec4(0.28f,0.24f,0.36f,1.f),"Click an object in the Outliner");
}
ImGui::End();
ImGui::PopStyleColor();

// ───────────────────────────────────────────────────────
//   BOTTOM PANEL (Console / Project / Animation)
// ───────────────────────────────────────────────────────
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f,0.086f,0.098f,1.f));
ImGui::Begin("Bottom##bottom", nullptr, ImGuiWindowFlags_NoCollapse);

if (ImGui::BeginTabBar("##btabs")) {
    // ── Console ──
    if (ImGui::BeginTabItem("  Console")) {
        ImGui::SameLine(0,8);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f,0.10f,0.18f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f,0.15f,0.38f,1.f));
        if(ImGui::SmallButton(" Clear ")) consoleLog.clear();
        ImGui::PopStyleColor(2);
        HRule();

        // Лог — оставляем место для строки ввода внизу (24px)
        float logH = ImGui::GetContentRegionAvail().y - 28.f;
        if (ImGui::BeginChild("##clog", ImVec2(-1, logH))) {
            for (auto& e : consoleLog) {
                ImVec4 col = e.level==2 ? ImVec4(1.f,.35f,.35f,1.f)
                           : e.level==1 ? ImVec4(1.f,.80f,.25f,1.f)
                           : e.level==3 ? ImVec4(0.55f,0.90f,1.f,1.f)   // CMD echo
                           :              ImVec4(.78f,.75f,.88f,1.f);
                const char* pfx = e.level==2?"  [ERR] ":e.level==1?"  [WRN] ":e.level==3?"  >  ":"  [INF] ";
                ImGui::TextColored(col, "%s%s", pfx, e.msg.c_str());
            }
            if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();

        // ── Строка ввода команд ──
        HRule();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f,0.07f,0.12f,1.f));
        ImGui::PushItemWidth(-60.f);
        bool enterPressed = false;

        // Колбэк для истории команд (стрелки вверх/вниз)
        struct CmdCallback {
            static int cb(ImGuiInputTextCallbackData* d) {
                if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                    if (g_CmdHistory.empty()) return 0;
                    if (d->EventKey == ImGuiKey_UpArrow) {
                        if (g_CmdHistoryIdx < (int)g_CmdHistory.size()-1) g_CmdHistoryIdx++;
                    } else if (d->EventKey == ImGuiKey_DownArrow) {
                        if (g_CmdHistoryIdx > -1) g_CmdHistoryIdx--;
                    }
                    std::string val = g_CmdHistoryIdx >= 0 ? g_CmdHistory[g_CmdHistoryIdx] : "";
                    d->DeleteChars(0, d->BufTextLen);
                    d->InsertChars(0, val.c_str());
                }
                return 0;
            }
        };

        if (g_ConsoleFocusInput) { ImGui::SetKeyboardFocusHere(); g_ConsoleFocusInput=false; }
        if (ImGui::InputText("##cmd", g_CmdBuf, sizeof(g_CmdBuf),
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackHistory,
            CmdCallback::cb)) {
            enterPressed = true;
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
        ImGui::SameLine(0,4);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f,0.14f,0.38f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.38f,0.24f,0.60f,1.f));
        if (ImGui::SmallButton(" Run ")) enterPressed = true;
        ImGui::PopStyleColor(2);

        if (enterPressed && g_CmdBuf[0] != '\0') {
            std::string cmd = g_CmdBuf;
            consoleLog.push_back({cmd, 3}); // echo cyan
            g_CmdHistory.insert(g_CmdHistory.begin(), cmd);
            if (g_CmdHistory.size() > 50) g_CmdHistory.pop_back();
            g_CmdHistoryIdx = -1;
            memset(g_CmdBuf, 0, sizeof(g_CmdBuf));
            g_ConsoleFocusInput = true;

            // ── Парсинг команды ──
            std::istringstream ss(cmd);
            std::string token; std::vector<std::string> args;
            while (ss >> token) args.push_back(token);
            std::string c = args.empty() ? "" : args[0];

            // help
            if (c=="help") {
                logInfo("Commands:");
                logInfo("  mkdir <path>           — create folder");
                logInfo("  ls / dir [path]        — list files");
                logInfo("  echo <text>            — print text");
                logInfo("  clear                  — clear console");
                logInfo("  list objects           — all scene objects");
                logInfo("  list lights            — all lights");
                logInfo("  select <name>          — select object");
                logInfo("  move <x> <y> <z>       — move selected");
                logInfo("  color <r> <g> <b>      — set color (0..1)");
                logInfo("  delete                 — delete selected");
                logInfo("  spawn cube/sphere/plane <name> — create object");
                logInfo("  volume <0..1>          — audio volume");
                logInfo("  play <path>            — play sound");
                logInfo("  fps                    — show FPS");
                logInfo("  scene save             — save scene");
                logInfo("  scene load <path>      — load scene file");
                logInfo("  scene reload           — reload current scene");
                logInfo("  scene current          — show current scene path");
            }
            // clear
            else if (c=="clear") { consoleLog.clear(); }
            // echo
            else if (c=="echo") {
                std::string out; for(int i=1;i<(int)args.size();i++) out+=args[i]+" ";
                logInfo(out);
            }
            // fps
            else if (c=="fps") {
                logInfo("FPS: "+std::to_string((int)io.Framerate));
            }
            // mkdir
            else if (c=="mkdir" && args.size()>=2) {
                try {
                    fs::path p = args[1];
                    if (p.is_relative()) p = fs::path(projectRoot) / p;
                    fs::create_directories(p);
                    logInfo("Created: "+p.string());
                } catch(const std::exception& ex){ logError(ex.what()); }
            }
            // ls / dir
            else if ((c=="ls"||c=="dir")) {
                fs::path p = args.size()>=2 ? fs::path(args[1]) : fs::path(std::string(assetCurrentPath));
                if (p.is_relative()) p = fs::path(projectRoot) / p;
                try {
                    for (auto& e : fs::directory_iterator(p)) {
                        std::string _pfx = e.is_directory() ? "[dir] " : "      ";
                        logInfo("  " + _pfx + e.path().filename().string());
                    }
                } catch(...){ logError("Folder not found: "+p.string()); }
            }
            // list objects
            else if (c=="list" && args.size()>=2 && args[1]=="objects") {
                if (objects.empty()) logInfo("No objects");
                for(int i=0;i<(int)objects.size();i++)
                    logInfo("  ["+std::to_string(i)+"] "+objects[i].name+
                        " pos("+std::to_string((int)objects[i].pos.x)+","+
                        std::to_string((int)objects[i].pos.y)+","+
                        std::to_string((int)objects[i].pos.z)+")");
            }
            // list lights
            else if (c=="list" && args.size()>=2 && args[1]=="lights") {
                if (lights.empty()) logInfo("No lights");
                for(int i=0;i<(int)lights.size();i++)
                    logInfo("  ["+std::to_string(i)+"] "+lights[i].name);
            }
            // select <name>
            else if (c=="select" && args.size()>=2) {
                bool found=false;
                for(int i=0;i<(int)objects.size();i++){
                    if(objects[i].name==args[1]){ sel=i; selType=SelectionType::Object; found=true;
                        logInfo("Selected: "+objects[i].name); break; }
                }
                if(!found) logWarn("Object not found: "+args[1]);
            }
            // move <x> <y> <z>
            else if (c=="move" && args.size()>=4 && selType==SelectionType::Object && sel>=0) {
                objects[sel].pos={std::stof(args[1]),std::stof(args[2]),std::stof(args[3])};
                logInfo("Moved: "+objects[sel].name);
            }
            // color <r> <g> <b>
            else if (c=="color" && args.size()>=4 && selType==SelectionType::Object && sel>=0) {
                objects[sel].color={std::stof(args[1]),std::stof(args[2]),std::stof(args[3])};
                logInfo("Color changed: "+objects[sel].name);
            }
            // delete
            else if (c=="delete" && selType==SelectionType::Object && sel>=0) {
                logInfo("Deleted: "+objects[sel].name);
                if(scene.IsAlive(objects[sel].ecsID)) scene.DestroyEntity(objects[sel].ecsID);
                objects.erase(objects.begin()+sel);
                if(sel>=(int)objects.size()) sel=(int)objects.size()-1;
            }
            // spawn <type> [name]
            else if (c=="spawn" && args.size()>=2) {
                PrimitiveType pt=PrimitiveType::Cube;
                if(args[1]=="sphere")   pt=PrimitiveType::Sphere;
                else if(args[1]=="plane")  pt=PrimitiveType::Plane;
                else if(args[1]=="cylinder") pt=PrimitiveType::Cylinder;
                else if(args[1]=="capsule")  pt=PrimitiveType::Capsule;
                else if(args[1]=="pyramid")  pt=PrimitiveType::Pyramid;
                SceneObject o; o.name=args.size()>=3?args[2]:(args[1]+"_"+std::to_string(objects.size()+1));
                o.type=pt; o.pos={0,0.5f,0}; o.color={0.8f,0.6f,0.3f};
                o.ecsID=scene.CreateEntity(o.name);
                scene.GetTransform(o.ecsID).Position=o.pos;
                scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                logInfo("Spawned: "+o.name);
            }
            // volume <v>
            else if (c=="volume" && args.size()>=2) {
                float v=std::stof(args[1]);
                VE::AudioEngine::Get().SetMasterVolume(v);
                logInfo("Volume: "+std::to_string(v));
            }
            // play <path>
            else if (c=="play" && args.size()>=2) {
                VE::AudioEngine::Get().PlaySound(args[1]);
                logInfo("Playing: "+args[1]);
            }
            // scene save / load / reload
            else if (c=="scene" && args.size()>=2 && args[1]=="save") {
                if(currentScenePath.empty()) currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
                SaveScene(currentScenePath,objects,lights,sceneCameras);
                logInfo("Scene saved: "+currentScenePath);
            }
            else if (c=="scene" && args.size()>=3 && args[1]=="load") {
                VE::SceneManager::Get().RequestLoad(args[2]);
                logInfo("Loading scene: "+args[2]);
            }
            else if (c=="scene" && args.size()>=2 && args[1]=="reload") {
                VE::SceneManager::Get().RequestReload();
                logInfo("Reloading scene...");
            }
            else if (c=="scene" && args.size()>=2 && args[1]=="current") {
                logInfo("Current: "+VE::SceneManager::Get().GetCurrent());
            }
            else {
                logWarn("Unknown command: "+c+" (type 'help')");
            }
        }

        ImGui::EndTabItem();
    }
    // ── Project ──
    if (ImGui::BeginTabItem("  Project")) {
        // Drag&drop
        if (!g_DroppedFiles.empty()) {
            for (auto& srcPath : g_DroppedFiles) {
                try {
                    fs::path src(srcPath);
                    fs::path dst = fs::path(assetCurrentPath) / src.filename();
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                    logInfo("Imported: " + src.filename().string());
                } catch (...) {}
            }
            g_DroppedFiles.clear();
        }

        float totalH = ImGui::GetContentRegionAvail().y;

        // ── LEFT: folder tree ──
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.078f,0.086f,0.098f,1.f));
        ImGui::BeginChild("##ptree", ImVec2(180, totalH), false);
        ImGui::Spacing();
        bool assetsOpen = ImGui::TreeNodeEx("Assets",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        if (assetsOpen) {
            try {
                std::string assetsDir = projectRoot + "\\Assets";
                if (fs::exists(assetsDir)) {
                    std::function<void(const std::string&)> drawTree = [&](const std::string& dir) {
                        for (auto& e : fs::directory_iterator(dir)) {
                            if (!e.is_directory()) continue;
                            std::string fn = e.path().filename().string();
                            bool isCur = (assetCurrentPath == e.path().string());
                            bool hasSub = false;
                            try { for (auto& s : fs::directory_iterator(e.path())) if (s.is_directory()) { hasSub=true; break; } } catch (...) {}
                            ImGuiTreeNodeFlags fl = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
                            if (isCur)  fl |= ImGuiTreeNodeFlags_Selected;
                            if (!hasSub) fl |= ImGuiTreeNodeFlags_Leaf;
                            ImGui::PushStyleColor(ImGuiCol_Text, isCur
                                ? ImVec4(0.88f,0.89f,0.92f,1.f) : ImVec4(0.60f,0.62f,0.66f,1.f));
                            bool open = ImGui::TreeNodeEx(fn.c_str(), fl, "  %s", fn.c_str());
                            ImGui::PopStyleColor();
                            if (ImGui::IsItemClicked()) assetCurrentPath = e.path().string();
                            if (open) { drawTree(e.path().string()); ImGui::TreePop(); }
                        }
                    };
                    drawTree(assetsDir);
                }
            } catch (...) {}
            ImGui::TreePop();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // Separator
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.176f,0.188f,0.212f,1.f));
        ImGui::BeginChild("##pvsep", ImVec2(1, totalH), false);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);

        // ── RIGHT: file grid ──
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.086f,0.094f,0.106f,1.f));
        ImGui::BeginChild("##pfiles", ImVec2(-1, totalH), false);

        // Breadcrumb
        ImGui::Spacing();
        std::string relPath = assetCurrentPath.size() > projectRoot.size()
            ? "Assets" + assetCurrentPath.substr(projectRoot.size() + 7) : "Assets";
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.45f,0.50f,1.f));
        ImGui::Text("  / %s", relPath.c_str());
        ImGui::PopStyleColor();
        if (assetCurrentPath != projectRoot + "\\Assets") {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.118f,0.125f,0.141f,1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f,0.20f,0.23f,1.f));
            if (ImGui::SmallButton(" < ")) assetCurrentPath = fs::path(assetCurrentPath).parent_path().string();
            ImGui::PopStyleColor(2);
        }
        ImGui::SameLine();
        static char s_ProjectSearch[128] = {};
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.09f,0.13f,1.f));
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##psearch", "\xf0\x9f\x94\x8d  Search...", s_ProjectSearch, sizeof(s_ProjectSearch));
        ImGui::PopStyleColor();
        ImGui::Separator();

        // File grid using columns
        try {
            if (fs::exists(assetCurrentPath) && fs::is_directory(assetCurrentPath)) {
                // Collect entries: dirs first, then files
                std::vector<fs::directory_entry> entries;
                std::vector<fs::directory_entry> dirs, files;
                for (auto& e : fs::directory_iterator(assetCurrentPath)) {
                    if (e.is_directory()) dirs.push_back(e);
                    else files.push_back(e);
                }
                std::sort(dirs.begin(), dirs.end(), [](auto& a, auto& b){ return a.path().filename() < b.path().filename(); });
                std::sort(files.begin(), files.end(), [](auto& a, auto& b){ return a.path().filename() < b.path().filename(); });
                std::string searchLower = s_ProjectSearch;
                std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
                auto matchesSearch = [&](const fs::directory_entry& e) -> bool {
                    if (searchLower.empty()) return true;
                    std::string n = e.path().filename().string();
                    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
                    return n.find(searchLower) != std::string::npos;
                };
                for (auto& d : dirs)  if (matchesSearch(d)) entries.push_back(d);
                for (auto& f : files) if (matchesSearch(f)) entries.push_back(f);

                const float ICON_SIZE = 64.f;
                const float LABEL_H   = 20.f;
                const float CELL_W    = ICON_SIZE + 16.f;
                const float CELL_H    = ICON_SIZE + LABEL_H + 8.f;
                float panelW = ImGui::GetContentRegionAvail().x;
                int numCols  = std::max(1, (int)(panelW / CELL_W));

                ImGui::Columns(numCols, "##grid", false);

                for (auto& e : entries) {
                    std::string name = e.path().filename().string();
                    std::string ext  = e.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    bool isDir = e.is_directory();
                    bool isSel = (assetSelected == e.path().string());

                    // Determine icon type
                    enum class IconType { Folder, Image, Script, Mesh3D, Scene, Audio, Save, MaterialIcon, Generic };
                    IconType iconType;
                    if      (isDir)                                                             iconType = IconType::Folder;
                    else if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".bmp"||ext==".tga") iconType = IconType::Image;
                    else if (ext==".lua")                                                       iconType = IconType::Script;
                    else if (ext==".obj"||ext==".fbx"||ext==".gltf"||ext==".glb")             iconType = IconType::Mesh3D;
                    else if (ext==".vescene")                                                   iconType = IconType::Scene;
                    else if (ext==".wav"||ext==".mp3"||ext==".ogg"||ext==".flac")             iconType = IconType::Audio;
                    else if (ext==".vesave")                                                    iconType = IconType::Save;
                    else if (ext==".mat")                                                       iconType = IconType::MaterialIcon;
                    else                                                                        iconType = IconType::Generic;

                    ImGui::PushID(name.c_str());

                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    auto*  dl = ImGui::GetWindowDrawList();
                    float  S  = ICON_SIZE;
                    float  x  = cp.x, y = cp.y;

                    // Background
                    ImU32 bd = isSel ? IM_COL32(180,185,200,255) : IM_COL32(45,45,52,255);
                    dl->AddRectFilled(cp, ImVec2(x+S,y+S), IM_COL32(28,28,34,255), 7.f);
                    dl->AddRect      (cp, ImVec2(x+S,y+S), bd, 7.f, 0, isSel?2.f:1.f);

                    // Draw icon shape
                    float cx = x + S*.5f, cy = y + S*.5f;
                    float p  = S * 0.18f; // padding

                    if (iconType == IconType::Folder) {
                        // Folder body
                        dl->AddRectFilled(ImVec2(x+p, y+p+8), ImVec2(x+S-p, y+S-p), IM_COL32(200,160,40,255), 4.f);
                        // Folder tab
                        dl->AddRectFilled(ImVec2(x+p, y+p+2), ImVec2(x+p+S*.35f, y+p+10), IM_COL32(200,160,40,255), 3.f);
                        // Highlight
                        dl->AddRectFilled(ImVec2(x+p, y+p+8), ImVec2(x+S-p, y+p+16), IM_COL32(230,190,70,80), 4.f);
                    }
                    else if (iconType == IconType::Image) {
                        // Image frame
                        dl->AddRectFilled(ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(30,60,100,255), 3.f);
                        dl->AddRect      (ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(80,140,220,255), 3.f, 0, 1.5f);
                        // Sun
                        dl->AddCircleFilled(ImVec2(cx-6,y+p+10), 7.f, IM_COL32(255,210,60,255));
                        // Mountain
                        ImVec2 mt1[] = {ImVec2(x+p+2,y+S-p-2), ImVec2(cx-2,y+p+18), ImVec2(cx+10,y+S-p-2)};
                        ImVec2 mt2[] = {ImVec2(cx+4,y+S-p-2), ImVec2(cx+14,y+p+22), ImVec2(x+S-p-2,y+S-p-2)};
                        dl->AddConvexPolyFilled(mt1, 3, IM_COL32(50,120,60,255));
                        dl->AddConvexPolyFilled(mt2, 3, IM_COL32(70,150,80,255));
                    }
                    else if (iconType == IconType::Script) {
                        // Page
                        dl->AddRectFilled(ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(30,80,45,255), 3.f);
                        dl->AddRect      (ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(60,180,90,200), 3.f, 0, 1.f);
                        // Folded corner
                        float fc = S*.22f;
                        dl->AddTriangleFilled(ImVec2(x+S-p-fc,y+p), ImVec2(x+S-p,y+p+fc), ImVec2(x+S-p-fc,y+p+fc), IM_COL32(20,50,30,255));
                        dl->AddTriangle      (ImVec2(x+S-p-fc,y+p), ImVec2(x+S-p,y+p+fc), ImVec2(x+S-p-fc,y+p+fc), IM_COL32(60,180,90,180));
                        // Code lines
                        float lx1=x+p+4, lx2=x+S-p-8, ly=y+p+fc+6;
                        for (int i=0;i<4;i++) {
                            float lw = (i%2==0)?lx2:lx2-10;
                            dl->AddLine(ImVec2(lx1,ly+i*6), ImVec2(lw,ly+i*6), IM_COL32(100,220,130,180), 1.5f);
                        }
                    }
                    else if (iconType == IconType::Mesh3D) {
                        // Cube wireframe look
                        dl->AddRectFilled(ImVec2(cx-12,cy-10), ImVec2(cx+10,cy+12), IM_COL32(80,40,140,255), 2.f);
                        dl->AddRect      (ImVec2(cx-12,cy-10), ImVec2(cx+10,cy+12), IM_COL32(160,100,255,220), 2.f, 0, 1.5f);
                        // Top face
                        ImVec2 top[]={ImVec2(cx-12,cy-10),ImVec2(cx-4,cy-18),ImVec2(cx+18,cy-18),ImVec2(cx+10,cy-10)};
                        dl->AddConvexPolyFilled(top,4,IM_COL32(100,55,180,200));
                        dl->AddPolyline(top,4,IM_COL32(160,100,255,220),ImDrawFlags_Closed,1.5f);
                        // Right face
                        ImVec2 rgt[]={ImVec2(cx+10,cy-10),ImVec2(cx+18,cy-18),ImVec2(cx+18,cy+4),ImVec2(cx+10,cy+12)};
                        dl->AddConvexPolyFilled(rgt,4,IM_COL32(60,30,110,200));
                        dl->AddPolyline(rgt,4,IM_COL32(160,100,255,220),ImDrawFlags_Closed,1.5f);
                    }
                    else if (iconType == IconType::Scene) {
                        // Globe
                        dl->AddCircleFilled(ImVec2(cx,cy), S*.32f, IM_COL32(20,60,120,255));
                        dl->AddCircle      (ImVec2(cx,cy), S*.32f, IM_COL32(60,140,240,255), 32, 1.5f);
                        // Latitude lines
                        dl->AddLine(ImVec2(cx-S*.3f,cy), ImVec2(cx+S*.3f,cy), IM_COL32(60,140,240,150), 1.f);
                        dl->AddLine(ImVec2(cx,cy-S*.3f), ImVec2(cx,cy+S*.3f), IM_COL32(60,140,240,150), 1.f);
                        // Ellipse (meridian) approximate
                        dl->AddEllipse(ImVec2(cx,cy), ImVec2(S*.32f, S*.16f), IM_COL32(60,140,240,120), 0.f, 32, 1.f);
                    }
                    else if (iconType == IconType::Audio) {
                        // Speaker body
                        ImVec2 sp[]={ImVec2(cx-14,cy-8),ImVec2(cx-6,cy-8),ImVec2(cx+2,cy-16),ImVec2(cx+2,cy+16),ImVec2(cx-6,cy+8),ImVec2(cx-14,cy+8)};
                        dl->AddConvexPolyFilled(sp,6,IM_COL32(200,130,40,255));
                        // Sound waves
                        for (int i=1;i<=3;i++) {
                            float r=i*6.f;
                            dl->AddCircle(ImVec2(cx+4,cy), r, IM_COL32(220,160,60,180-i*40), 12, 1.5f);
                        }
                    }
                    else if (iconType == IconType::Save) {
                        // Floppy disk
                        dl->AddRectFilled(ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(30,90,50,255), 3.f);
                        dl->AddRect      (ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(60,180,90,200), 3.f, 0, 1.5f);
                        // Label area
                        dl->AddRectFilled(ImVec2(x+p+4,y+p+4), ImVec2(x+S-p-4,y+p+18), IM_COL32(20,60,35,255), 2.f);
                        // Metal shutter
                        dl->AddRectFilled(ImVec2(cx-8,y+S-p-14), ImVec2(cx+8,y+S-p-2), IM_COL32(150,150,160,255), 2.f);
                    }
                    else if (iconType == IconType::MaterialIcon) {
                        // Sphere preview using material's actual color
                        Material previewMat = LoadMaterial(e.path().string());
                        ImU32 sphereCol = IM_COL32(
                            (int)(previewMat.color.r*255),
                            (int)(previewMat.color.g*255),
                            (int)(previewMat.color.b*255), 255);
                        float r = S*.30f;
                        dl->AddCircleFilled(ImVec2(cx,cy), r, sphereCol, 24);
                        // Highlight (specular dot) for shiny look
                        dl->AddCircleFilled(ImVec2(cx-r*.35f,cy-r*.35f), r*.25f, IM_COL32(255,255,255,140), 12);
                        dl->AddCircle(ImVec2(cx,cy), r, IM_COL32(0,0,0,80), 24, 1.f);
                    }
                    else {
                        // Generic file page
                        dl->AddRectFilled(ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(35,35,42,255), 3.f);
                        dl->AddRect      (ImVec2(x+p,y+p), ImVec2(x+S-p,y+S-p), IM_COL32(90,90,100,200), 3.f, 0, 1.f);
                        float fc2=S*.20f;
                        dl->AddTriangleFilled(ImVec2(x+S-p-fc2,y+p), ImVec2(x+S-p,y+p+fc2), ImVec2(x+S-p-fc2,y+p+fc2), IM_COL32(22,22,28,255));
                        dl->AddTriangle      (ImVec2(x+S-p-fc2,y+p), ImVec2(x+S-p,y+p+fc2), ImVec2(x+S-p-fc2,y+p+fc2), IM_COL32(90,90,100,150));
                        // Extension text
                        if (!ext.empty()) {
                            std::string extU = ext.substr(1); std::transform(extU.begin(),extU.end(),extU.begin(),::toupper);
                            ImVec2 ets=ImGui::CalcTextSize(extU.c_str());
                            dl->AddText(ImVec2(cx-ets.x*.5f, cy-ets.y*.5f+4), IM_COL32(150,150,160,255), extU.c_str());
                        }
                    }

                    // Invisible button over the icon
                    ImGui::InvisibleButton(("##icon"+name).c_str(), ImVec2(ICON_SIZE, ICON_SIZE));
                    if (ImGui::IsItemClicked()) {
                        assetSelected = e.path().string();
                        if (isDir) assetCurrentPath = e.path().string();
                        else if (ext==".mat") selType = SelectionType::None; // показать материал в Inspector
                    }

                    // ── Drag source: материалы можно перетащить на объект ──
                    if (ext==".mat") {
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            std::string dragPath = e.path().string();
                            ImGui::SetDragDropPayload("MATERIAL_PATH", dragPath.c_str(), dragPath.size()+1, ImGuiCond_Once);
                            ImGui::TextColored(ImVec4(0.7f,0.85f,1.f,1.f), "Material: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }

                    // ── Drag source: Lua-скрипты можно перетащить на объект (как в Unity) ──
                    if (ext==".lua") {
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            std::string dragPath = e.path().string();
                            ImGui::SetDragDropPayload("SCRIPT_PATH", dragPath.c_str(), dragPath.size()+1, ImGuiCond_Once);
                            ImGui::TextColored(ImVec4(0.6f,1.f,0.6f,1.f), "Script: %s", name.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !isDir) {
                        if (ext==".lua") openInVSCode(e.path().string());
                        else if (ext==".mat") {
                            // Применить материал к выбранному объекту (быстрый способ без drag&drop)
                            if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
                                Material loaded = LoadMaterial(e.path().string());
                                auto& tobj = objects[sel];
                                if (tobj.materials.empty()) tobj.materials.push_back(loaded);
                                else tobj.materials[tobj.activeMaterial] = loaded;
                                if (tobj.activeMaterial==0) {
                                    tobj.color = loaded.color;
                                    tobj.texturePath = loaded.texturePath;
                                    tobj.textureID = loaded.textureID;
                                }
                                logInfo("Material '"+loaded.name+"' -> "+tobj.name);
                            } else logInfo("Select an object first to apply material");
                        }
                        else if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".bmp"||ext==".tga") {
                            if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
                                GLuint tid = VE::LoadTextureRaw(e.path().string());
                                if (tid) {
                                    auto& tobj = objects[sel];
                                    if (tobj.materials.empty()) { Material m; m.name="Default"; m.color=tobj.color; tobj.materials.push_back(m); tobj.activeMaterial=0; }
                                    auto& tmat = tobj.materials[tobj.activeMaterial];
                                    tmat.texturePath = e.path().string(); tmat.textureID = tid;
                                    if (tobj.activeMaterial==0) { tobj.texturePath=tmat.texturePath; tobj.textureID=tid; }
                                    logInfo("Texture -> "+tobj.name+" ["+tmat.name+"]");
                                }
                            } else logInfo("Select object first");
                        } else if (ext==".obj"||ext==".fbx"||ext==".gltf"||ext==".glb") {
                            SceneObject o; o.name=e.path().stem().string(); o.type=PrimitiveType::Model3D;
                            o.modelPath=e.path().string(); o.model=std::make_shared<VE::Model>(); o.model->Load(o.modelPath);
                            o.color=glm::vec3(0.8f,0.8f,0.8f); o.ecsID=scene.CreateEntity(o.name);
                            scene.GetTransform(o.ecsID).Position=o.pos;
                            scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                            objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                            logInfo("Model: "+o.name);
                        }
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name.c_str());

                    // File name below icon
                    std::string sn = name.size() > 10 ? name.substr(0,9)+"~" : name;
                    ImVec2 ns = ImGui::CalcTextSize(sn.c_str());
                    ImVec2 namePos = ImGui::GetCursorScreenPos();
                    dl->AddText(ImVec2(cp.x+(ICON_SIZE-ns.x)*.5f, namePos.y+2.f),
                        isSel ? IM_COL32(160,205,255,255) : IM_COL32(170,170,180,220), sn.c_str());

                    // Dummy to reserve space for name label
                    ImGui::Dummy(ImVec2(ICON_SIZE, LABEL_H));
                    ImGui::Spacing();

                    ImGui::PopID();
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
            } else {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.f,.4f,.4f,1.f), "  Folder not found: %s", assetCurrentPath.c_str());
            }
        } catch (...) {
            ImGui::TextColored(ImVec4(1.f,.4f,.4f,1.f), "  Error reading folder");
        }

        // Right-click context menu
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered())
            ImGui::OpenPopup("##projctx");
        if (ImGui::BeginPopup("##projctx")) {
            ImGui::TextColored(ImVec4(0.45f,0.45f,0.50f,1.f), "  Create"); ImGui::Separator();
            if (ImGui::MenuItem("  Lua Script")) {
                static int snum = 1;
                std::string sp = assetCurrentPath + "\\NewScript_" + std::to_string(snum++) + ".lua";
                std::ofstream f(sp); f << "-- New Script\nfunction onStart()\nend\nfunction onUpdate(dt)\nend\n"; f.close();
                logInfo("Created: " + fs::path(sp).filename().string()); openInVSCode(sp);
            }
            if (ImGui::MenuItem("  Material")) {
                static int mnum = 1;
                std::string mp = assetCurrentPath + "\\NewMaterial_" + std::to_string(mnum++) + ".mat";
                Material newMat; newMat.name = fs::path(mp).stem().string();
                SaveMaterial(mp, newMat);
                logInfo("Created: " + fs::path(mp).filename().string());
                assetSelected = mp;
            }
            if (ImGui::MenuItem("  Folder")) {
                static int fnum = 1;
                std::string fp = assetCurrentPath + "\\NewFolder_" + std::to_string(fnum++);
                try { fs::create_directory(fp); logInfo("Created folder"); } catch (...) {}
            }
            ImGui::Separator();
            if (ImGui::MenuItem("  Show in Explorer")) {
                std::string cmd = "explorer \"" + assetCurrentPath + "\"";
                system(cmd.c_str());
            }
            if (!assetSelected.empty() && fs::exists(assetSelected)) {
                ImGui::Separator();
                if (ImGui::MenuItem("  Delete Selected")) {
                    try {
                        std::string fname = fs::path(assetSelected).filename().string();
                        if (fs::is_directory(assetSelected))
                            fs::remove_all(assetSelected);
                        else
                            fs::remove(assetSelected);
                        logInfo("Deleted: "+fname);
                        assetSelected.clear();
                    } catch(const std::exception& ex){ logError(ex.what()); }
                }
                if (ImGui::MenuItem("  Rename...")) {
                    // Open rename popup
                    ImGui::OpenPopup("##rename_file");
                }
            }
            ImGui::EndPopup();
        }

        // Rename file popup
        static char s_RenameBuffer[256] = {};
        if (ImGui::BeginPopupModal("##rename_file", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("New name:");
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##rename_input", s_RenameBuffer, sizeof(s_RenameBuffer));
            ImGui::Spacing();
            if (ImGui::Button("Rename", ImVec2(120,0))) {
                if (s_RenameBuffer[0] && !assetSelected.empty()) {
                    try {
                        fs::path oldP(assetSelected);
                        fs::path newP = oldP.parent_path() / s_RenameBuffer;
                        fs::rename(oldP, newP);
                        logInfo("Renamed to: "+std::string(s_RenameBuffer));
                        assetSelected = newP.string();
                    } catch(const std::exception& ex){ logError(ex.what()); }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120,0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::EndTabItem();
    }

        // ── Animation ──
    if (ImGui::BeginTabItem("  Animation")) {
        ImGui::Spacing();
        if (selType != SelectionType::Object || sel < 0 || sel >= (int)objects.size()) {
            ImGui::TextColored(COL_DIM, "  Select an object in the scene to animate it.");
        } else {
            auto& aobj = objects[sel];
            ImGui::TextColored(ImVec4(0.85f,0.85f,0.90f,1.f), "  Animating: %s", aobj.name.c_str());
            ImGui::Spacing();

            // ── Выбор клипа ──
            const char* curClipName = (aobj.customClipIndex>=0 && aobj.customClipIndex<(int)aobj.customClips.size())
                ? aobj.customClips[aobj.customClipIndex].name.c_str() : "None";
            ImGui::SetNextItemWidth(180);
            if (ImGui::BeginCombo("##customclip", curClipName)) {
                for (int c=0;c<(int)aobj.customClips.size();c++){
                    bool s=(aobj.customClipIndex==c);
                    if (ImGui::Selectable(aobj.customClips[c].name.c_str(), s)) { aobj.customClipIndex=c; aobj.customAnimTime=0.f; }
                    if (s) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("+ New Clip")) {
                ObjectAnimClip c; c.name = "Clip "+std::to_string(aobj.customClips.size()+1);
                aobj.customClips.push_back(c);
                aobj.customClipIndex = (int)aobj.customClips.size()-1;
                aobj.customAnimTime = 0.f;
            }
            if (aobj.customClipIndex>=0) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.08f,0.08f,1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f,0.12f,0.12f,1.f));
                if (ImGui::Button("Delete Clip")) {
                    aobj.customClips.erase(aobj.customClips.begin()+aobj.customClipIndex);
                    aobj.customClipIndex=-1; aobj.customAnimPlaying=false;
                }
                ImGui::PopStyleColor(2);
            }

            if (aobj.customClipIndex>=0 && aobj.customClipIndex<(int)aobj.customClips.size()) {
                auto& clip = aobj.customClips[aobj.customClipIndex];
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                if (ImGui::Button(aobj.customAnimPlaying ? "  Pause  " : "  Play  ", ImVec2(80,0)))
                    aobj.customAnimPlaying = !aobj.customAnimPlaying;
                ImGui::SameLine();
                if (ImGui::Button("  Stop  ", ImVec2(80,0))) { aobj.customAnimPlaying=false; aobj.customAnimTime=0.f; }
                ImGui::SameLine();
                ImGui::Checkbox("Loop", &clip.loop);

                float dur = clip.keys.empty() ? 1.f : std::max(1.f, clip.keys.back().time);

                // ── Визуальный таймлайн (как в Blender/Blockbench) ──
                ImGui::Spacing();
                {
                    float timelineWidth  = ImGui::GetContentRegionAvail().x - 16.f;
                    float timelineHeight = 50.f;
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    ImVec2 p1 = ImVec2(p0.x+timelineWidth, p0.y+timelineHeight);
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    float pxPerSec = timelineWidth / dur;

                    dl->AddRectFilled(p0, p1, IM_COL32(18,18,24,255), 4.f);

                    // Сетка по секундам + подписи
                    for (int s=0; s<=(int)ceilf(dur); s++) {
                        float x = p0.x + s*pxPerSec;
                        dl->AddLine(ImVec2(x,p0.y), ImVec2(x,p1.y), IM_COL32(48,48,56,255));
                        dl->AddText(ImVec2(x+3,p0.y+2), IM_COL32(140,140,150,255), (std::to_string(s)+"s").c_str());
                    }

                    // Ромбики — по одному на ключевой кадр
                    float trackY = p0.y + timelineHeight*0.68f;
                    for (auto& k : clip.keys) {
                        float x = p0.x + k.time*pxPerSec;
                        bool isCur = fabsf(k.time - aobj.customAnimTime) < (0.25f/pxPerSec);
                        ImU32 col = isCur ? IM_COL32(255,205,80,255) : IM_COL32(120,170,255,255);
                        dl->AddQuadFilled(ImVec2(x,trackY-6), ImVec2(x+6,trackY), ImVec2(x,trackY+6), ImVec2(x-6,trackY), col);
                        dl->AddQuad(ImVec2(x,trackY-6), ImVec2(x+6,trackY), ImVec2(x,trackY+6), ImVec2(x-6,trackY), IM_COL32(10,10,12,255),1.5f);
                    }

                    // Плейхед (текущее время)
                    float phX = p0.x + glm::clamp(aobj.customAnimTime,0.f,dur)*pxPerSec;
                    dl->AddLine(ImVec2(phX,p0.y), ImVec2(phX,p1.y), IM_COL32(255,90,90,255), 2.f);
                    dl->AddTriangleFilled(ImVec2(phX-5,p0.y), ImVec2(phX+5,p0.y), ImVec2(phX,p0.y+8), IM_COL32(255,90,90,255));

                    ImGui::InvisibleButton("##timeline", ImVec2(timelineWidth, timelineHeight));
                    // Клик/протаскивание по полосе — скраб времени (как таскать плейхед в Blender)
                    if (ImGui::IsItemActive()) {
                        float mx = ImGui::GetIO().MousePos.x;
                        float t = (mx - p0.x) / pxPerSec;
                        aobj.customAnimTime = glm::clamp(t, 0.f, dur);
                        aobj.customAnimPlaying = false;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click or drag to scrub time — diamonds are keyframes");
                }
                ImGui::Spacing();
                ImGui::Text("Time: %.2f s", aobj.customAnimTime);
                if (!aobj.customAnimPlaying && !clip.keys.empty())
                    SampleObjectClip(clip, aobj.customAnimTime, aobj.pos, aobj.rot, aobj.scale); // превью позы при скрабе

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f,0.10f,0.22f,1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT);
                if (ImGui::Button("  + Add Keyframe at current time (captures current Transform)  ")) {
                    ObjectKeyframe k; k.time=aobj.customAnimTime; k.pos=aobj.pos; k.rot=aobj.rot; k.scale=aobj.scale;
                    // если кадр в это же время уже есть — заменяем, иначе добавляем и сортируем
                    bool replaced=false;
                    for (auto& ek : clip.keys) if (fabsf(ek.time-k.time)<0.001f) { ek=k; replaced=true; break; }
                    if (!replaced) {
                        clip.keys.push_back(k);
                        std::sort(clip.keys.begin(), clip.keys.end(), [](auto& a, auto& b){ return a.time<b.time; });
                    }
                }
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::TextColored(COL_DIM, "Keyframes:");
                if (ImGui::BeginTable("##keys", 6, ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY, ImVec2(0,180))) {
                    ImGui::TableSetupColumn("Time");
                    ImGui::TableSetupColumn("Position");
                    ImGui::TableSetupColumn("Rotation");
                    ImGui::TableSetupColumn("Scale");
                    ImGui::TableSetupColumn("Go");
                    ImGui::TableSetupColumn("Del");
                    ImGui::TableHeadersRow();
                    int delIdx=-1;
                    for (int k=0;k<(int)clip.keys.size();k++) {
                        auto& key = clip.keys[k];
                        ImGui::PushID(k);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::Text("%.2f s", key.time);
                        ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f, %.1f, %.1f", key.pos.x,key.pos.y,key.pos.z);
                        ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f, %.0f, %.0f", key.rot.x,key.rot.y,key.rot.z);
                        ImGui::TableSetColumnIndex(3); ImGui::Text("%.1f, %.1f, %.1f", key.scale.x,key.scale.y,key.scale.z);
                        ImGui::TableSetColumnIndex(4); if (ImGui::SmallButton("Go")) { aobj.customAnimTime=key.time; aobj.customAnimPlaying=false; }
                        ImGui::TableSetColumnIndex(5);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f,0.08f,0.08f,1.f));
                        if (ImGui::SmallButton("X")) delIdx=k;
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                    if (delIdx>=0) clip.keys.erase(clip.keys.begin()+delIdx);
                    ImGui::EndTable();
                }
                ImGui::TextColored(COL_DIM, "Tip: move/rotate/scale the object in the viewport,\nthen click \"Add Keyframe\" to capture that pose at the current time.");
            }
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
ImGui::End();
ImGui::PopStyleColor();

} // end if (!g_PlayerMode)
else {
    // ═══════════════════════════════════════════════════════
    //   PLAYER MODE — полноэкранный вид игры, без редактора
    // ═══════════════════════════════════════════════════════
    g_VpSize = io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##GameFullscreen", nullptr,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|
        ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus|
        ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoDocking);
    float u2g=g_VpSize.x>0.f?g_VpSize.x/3840.f:1.f;
    float v1g=g_VpSize.y>0.f?g_VpSize.y/2160.f:1.f;
    ImGui::Image((ImTextureID)(intptr_t)gameTex, g_VpSize, ImVec2(0,v1g), ImVec2(u2g,0));
    // ── Player mode: курсор захватывается сразу (нет UI, некуда кликать) ──
    if (!g_MouseCaptured) {
        g_MouseCaptured = true;
        glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_RawMouseFirst = true;
    }
    if (g_MouseCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        g_MouseCaptured = false;
        glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    if (!g_MouseCaptured && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_MouseCaptured = true;
        glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_RawMouseFirst = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

ImGui::Render();
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
g_RawMouseDX=0; g_RawMouseDY=0; // сброс дельты ПЕРЕД poll — свежие данные переживут до следующего кадра
window->OnUpdate();

    } // end while

    g_Prefs.Save();

    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();
    VE::AudioEngine::Get().Shutdown();
    shader.Delete();outlineShader.Delete();gridShader.Delete();gizmoShader.Delete();skyboxShader.Delete();
    glDeleteFramebuffers(1,&sceneFBO);glDeleteFramebuffers(1,&gameFBO);
    delete window;return 0;
}