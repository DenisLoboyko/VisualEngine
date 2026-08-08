#pragma once
#include "SceneObject.h"
#include "Core/UndoSystem.h"
// ── Глобальное состояние редактора: камера, ввод, EditorMode, EditorPrefs (вынесено из main.cpp) ──

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
int g_UndoDragObjIndex=-1; SelectionType g_UndoDragSelType=SelectionType::None; GizmoAxis g_PrevDragAxis=GizmoAxis::None; // Undo/Redo: состояние драга гизмо
glm::vec3 dragStartPos,dragStartRot,dragStartScale,dragStartHit;
bool isPlaying=false,isPaused=false;
int g_MatPickTarget = 0; // 0=нет, 1=Texture, 2=Layer2, 3=Mask — режим "жду клика по картинке в Project"
bool g_BrushPaintMode = false; // false = Erase (стереть Layer2), true = Paint (вернуть Layer2). Shift — временно инвертирует.
// ── Режимы редактора (задел на будущее, как в Blender: Object/Paint/...) ──
enum class EditorMode { Object, PaintMask };
EditorMode g_EditorMode = EditorMode::Object;
float g_BrushRadius   = 0.10f; // радиус кисти в UV (0..1)

// ═══════════════════════════════════════════════════════
//   ENVIRONMENT — процедурное небо (время суток, облака)
// ═══════════════════════════════════════════════════════
float g_TimeOfDay   = 12.0f;  // часы, 0..24 (6=рассвет, 12=полдень, 18=закат, 0/24=полночь)
float g_FogDensity = 0.0f;                          // 0..1 — плотность тумана (0 = выкл)
glm::vec3 g_FogColor = glm::vec3(0.6f,0.65f,0.7f);  // цвет тумана
float g_SunIntensity   = 1.0f;   // множитель яркости солнца (0 = выключить)
float g_AmbientStrength = 0.12f; // фоновая подсветка (чтобы тени не были чёрными)
std::vector<SceneObject>* g_LuaObjectsPtr = nullptr; // указывает на objects[] из main(), для Animation API
std::vector<LightObject>* g_LuaLightsPtr  = nullptr; // указывает на lights[] из main(), для SaveScene/Scene.SetCamera из Lua
std::vector<CameraObject>* g_LuaCamerasPtr = nullptr; // указывает на sceneCameras[] из main(), для Scene.SetCamera из Lua

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

// ========================================
// Viewport FBO динамический ресайз
// ========================================
int g_VpLastWidth = -1, g_VpLastHeight = -1;  // Последний размер FBO (для отслеживания изменений)

// ════════════════════════════════════════════════════════════════════════════════════
// Глобальные указатели для Lua-биндингов (определены в main.cpp)
// ════════════════════════════════════════════════════════════════════════════════════
namespace VE {
    class AudioEngine;
    class Physics;
    class Scene;
}

class VE::AudioEngine* g_AudioEngine = nullptr;
std::vector<SceneObject>* g_ObjectsPtr = nullptr;
class VE::Physics* g_PhysicsEngine = nullptr;
class VE::Scene* g_Scene = nullptr;

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