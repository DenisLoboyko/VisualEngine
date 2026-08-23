#pragma once
#include <vector>
#include <string>
inline std::vector<std::string> g_CcNames;
inline std::vector<std::string> g_ColNames;
inline bool g_SkipPropagate=false;
#include "SceneObject.h"
#include "Core/UndoSystem.h"
// в”Ђв”Ђ Р“Р»РѕР±Р°Р»СЊРЅРѕРµ СЃРѕСЃС‚РѕСЏРЅРёРµ СЂРµРґР°РєС‚РѕСЂР°: РєР°РјРµСЂР°, РІРІРѕРґ, EditorMode, EditorPrefs (РІС‹РЅРµСЃРµРЅРѕ РёР· main.cpp) в”Ђв”Ђ

VE::Camera camera(glm::vec3(0,3,8));
VE::Camera gameCamera(glm::vec3(0,2,5));
float lastX=640,lastY=360,deltaTime=0,lastFrame=0;
bool firstMouse=true,rightMouseDown=false;
bool leftDown=false,leftClickThisFrame=false;
double clickX=0,clickY=0,mouseX=0,mouseY=0;
static int g_DragHoverObj = -1; // РѕР±СЉРµРєС‚ РїРѕРґ РєСѓСЂСЃРѕСЂРѕРј РІРѕ РІСЂРµРјСЏ drag&drop
double g_RawMouseDX=0,g_RawMouseDY=0; // РґРµР»СЊС‚Р° РјС‹С€Рё Р·Р° РєР°РґСЂ, РґР»СЏ FPS-РєР°РјРµСЂС‹ РёР· Lua
double g_LastRawMouseX=0,g_LastRawMouseY=0; bool g_RawMouseFirst=true;
bool g_MouseCaptured=false; // РєСѓСЂСЃРѕСЂ СЃРїСЂСЏС‚Р°РЅ Рё Р·Р°С†РёРєР»РµРЅ (FPS look) вЂ” РєР»РёРє РїРѕ Game / Esc РїРµСЂРµРєР»СЋС‡Р°СЋС‚
GizmoMode gizmoMode=GizmoMode::Move;
GizmoAxis dragAxis=GizmoAxis::None;
int g_UndoDragObjIndex=-1; SelectionType g_UndoDragSelType=SelectionType::None; GizmoAxis g_PrevDragAxis=GizmoAxis::None; // Undo/Redo: СЃРѕСЃС‚РѕСЏРЅРёРµ РґСЂР°РіР° РіРёР·РјРѕ
glm::vec3 dragStartPos,dragStartRot,dragStartScale,dragStartHit;
bool isPlaying=false,isPaused=false;
int g_MatPickTarget = 0; // 0=РЅРµС‚, 1=Texture, 2=Layer2, 3=Mask вЂ” СЂРµР¶РёРј "Р¶РґСѓ РєР»РёРєР° РїРѕ РєР°СЂС‚РёРЅРєРµ РІ Project"
bool g_BrushPaintMode = false; // false = Erase (СЃС‚РµСЂРµС‚СЊ Layer2), true = Paint (РІРµСЂРЅСѓС‚СЊ Layer2). Shift вЂ” РІСЂРµРјРµРЅРЅРѕ РёРЅРІРµСЂС‚РёСЂСѓРµС‚.
// в”Ђв”Ђ Р РµР¶РёРјС‹ СЂРµРґР°РєС‚РѕСЂР° (Р·Р°РґРµР» РЅР° Р±СѓРґСѓС‰РµРµ, РєР°Рє РІ Blender: Object/Paint/...) в”Ђв”Ђ
enum class EditorMode { Object, PaintMask };
EditorMode g_EditorMode = EditorMode::Object;
float g_BrushRadius   = 0.10f; // СЂР°РґРёСѓСЃ РєРёСЃС‚Рё РІ UV (0..1)

// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
//   ENVIRONMENT вЂ” РїСЂРѕС†РµРґСѓСЂРЅРѕРµ РЅРµР±Рѕ (РІСЂРµРјСЏ СЃСѓС‚РѕРє, РѕР±Р»Р°РєР°)
// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
float g_TimeOfDay   = 12.0f;  // С‡Р°СЃС‹, 0..24 (6=СЂР°СЃСЃРІРµС‚, 12=РїРѕР»РґРµРЅСЊ, 18=Р·Р°РєР°С‚, 0/24=РїРѕР»РЅРѕС‡СЊ)
float g_FogDensity = 0.0f;                          // 0..1 вЂ” РїР»РѕС‚РЅРѕСЃС‚СЊ С‚СѓРјР°РЅР° (0 = РІС‹РєР»)
glm::vec3 g_FogColor = glm::vec3(0.6f,0.65f,0.7f);  // С†РІРµС‚ С‚СѓРјР°РЅР°
float g_SunIntensity   = 1.0f;   // РјРЅРѕР¶РёС‚РµР»СЊ СЏСЂРєРѕСЃС‚Рё СЃРѕР»РЅС†Р° (0 = РІС‹РєР»СЋС‡РёС‚СЊ)
float g_AmbientStrength = 0.12f; // С„РѕРЅРѕРІР°СЏ РїРѕРґСЃРІРµС‚РєР° (С‡С‚РѕР±С‹ С‚РµРЅРё РЅРµ Р±С‹Р»Рё С‡С‘СЂРЅС‹РјРё)
std::vector<SceneObject>* g_LuaObjectsPtr = nullptr; // СѓРєР°Р·С‹РІР°РµС‚ РЅР° objects[] РёР· main(), РґР»СЏ Animation API
std::vector<LightObject>* g_LuaLightsPtr  = nullptr; // СѓРєР°Р·С‹РІР°РµС‚ РЅР° lights[] РёР· main(), РґР»СЏ SaveScene/Scene.SetCamera РёР· Lua
std::vector<CameraObject>* g_LuaCamerasPtr = nullptr; // СѓРєР°Р·С‹РІР°РµС‚ РЅР° sceneCameras[] РёР· main(), РґР»СЏ Scene.SetCamera РёР· Lua

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
std::vector<std::string> g_DroppedFiles; // РїСѓС‚Рё С„Р°Р№Р»РѕРІ, РїРµСЂРµС‚Р°С‰РµРЅРЅС‹С… РёР· Windows РІ РѕРєРЅРѕ РґРІРёР¶РєР°
std::vector<SavedTransform> savedTransforms;
ImVec2 g_VpPos(0,0),g_VpSize(940,600);

// ========================================
// Viewport FBO РґРёРЅР°РјРёС‡РµСЃРєРёР№ СЂРµСЃР°Р№Р·
// ========================================
int g_VpLastWidth = -1, g_VpLastHeight = -1;  // РџРѕСЃР»РµРґРЅРёР№ СЂР°Р·РјРµСЂ FBO (РґР»СЏ РѕС‚СЃР»РµР¶РёРІР°РЅРёСЏ РёР·РјРµРЅРµРЅРёР№)

// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
// Р“Р»РѕР±Р°Р»СЊРЅС‹Рµ СѓРєР°Р·Р°С‚РµР»Рё РґР»СЏ Lua-Р±РёРЅРґРёРЅРіРѕРІ (РѕРїСЂРµРґРµР»РµРЅС‹ РІ main.cpp)
// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
namespace VE {
    class AudioEngine;
    class Physics;
    class Scene;
}

class VE::AudioEngine* g_AudioEngine = nullptr;
std::vector<SceneObject>* g_ObjectsPtr = nullptr;
class VE::Physics* g_PhysicsEngine = nullptr;
class VE::Scene* g_Scene = nullptr;

// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
//   EDITOR PREFERENCES вЂ” РєР°Рє EditorSettings РІ Godot
// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
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

// в”Ђв”Ђ Player mode: РґРІРёР¶РѕРє Р·Р°РїСѓС‰РµРЅ РєР°Рє СЃР°РјРѕСЃС‚РѕСЏС‚РµР»СЊРЅР°СЏ РёРіСЂР° (Р±РµР· СЂРµРґР°РєС‚РѕСЂР°) в”Ђв”Ђ
// РђРєС‚РёРІРёСЂСѓРµС‚СЃСЏ Р°СЂРіСѓРјРµРЅС‚РѕРј РєРѕРјР°РЅРґРЅРѕР№ СЃС‚СЂРѕРєРё: VisualEngine.exe --play "Assets/scene.vescene"
bool g_PlayerMode = false;
std::string g_PlayerScenePath = "";
std::string g_OverrideProjectRoot = ""; // РїРµСЂРµРґР°С‘С‚СЃСЏ РёР· VisualHub С‡РµСЂРµР· --project
bool g_PlayerAutoPlayPending = false;

GLFWcursorposfun g_PrevCursorPosCallback = nullptr; // РєРѕР»Р»Р±СЌРє ImGui, РєРѕС‚РѕСЂС‹Р№ РЅСѓР¶РЅРѕ РїСЂРѕР±СЂРѕСЃРёС‚СЊ РґР°Р»СЊС€Рµ

