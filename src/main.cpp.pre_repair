#define IMGUI_DEFINE_MATH_OPERATORS
#define MINIAUDIO_IMPLEMENTATION
#include "imnodes.h"
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
#include "ECS/CharacterControllerComponent.h"
static bool g_FpsLock=false;
static bool g_WantGameTab=false;
static bool g_HasCtrl=false;
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
#include <unordered_map>
#include <chrono>
#include <string>
#include <algorithm>
#include <memory>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;


// ── Модули движка (вынесены из main.cpp для читаемости) ──
#include "Shaders.h"
#include "SceneTypes.h"
#include "Material.h"
#include "Animation.h"
#include "SceneObject.h"
#include "EditorGlobals.h"
#include "InputCallbacks.h"
#include "SceneRenderer.h"
#include "Sprites2D.h"
#include "UI2D.h"
#include "UILua.h"
#include "GameLua.h"
std::vector<UIElement> uiElements;
int selUI = -1;
std::vector<Sprite2D> sprites2D;
int selSprite2D = -1;
static void UI_TexPick(int idx, const std::string& p){
    if (idx<0 || idx>=(int)uiElements.size()) return;
    GLuint t = VE::LoadTextureRaw(p);
    if (t) { uiElements[idx].tex=t;
        uiElements[idx].texPath=p;
        logInfo("Texture -> "+uiElements[idx].name); }
}
#include "SceneIO.h"
#include "PrefabIO.h"
#include "EditorIcons.h"
#include "ShadowMap.h"
bool showGraphicsSettings = false;
int g_DrawCallCount = 0;
static std::chrono::high_resolution_clock::time_point g_pt0, g_pt1, g_pt2, g_pt3;
static float g_msScene=0, g_msGame=0, g_msBloom=0;
float g_camPosX=0, g_camPosY=0, g_camPosZ=0;
float g_camYaw=0, g_camPitch=0;
namespace GameLua { void* g_window = nullptr; }
VE::ShadowMap* g_ShadowMap = nullptr;
VE::Shader* g_DepthShader = nullptr;
VE::Shader* g_DepthSkinnedShader = nullptr;
glm::mat4 g_ShadowLightSpace(1.0f);

// ---------------------------------------------------------═══════════════════════════════════════════════════════════════
// Функция для динамического ресайза Viewport FBO
// Вызывается когда размер ImGui Viewport изменился
// ---------------------------------------------------------═══════════════════════════════════════════════════════════════
void ResizeViewportFBO(int newWidth, int newHeight, 
                       unsigned int& sceneMSFBO, unsigned int& sceneMSColorRBO, unsigned int& sceneMSDepthRBO,
                       unsigned int& gameMSFBO, unsigned int& gameMSColorRBO, unsigned int& gameMSDepthRBO,
                       unsigned int& sceneHDRFBO, unsigned int& sceneHDRTex,
                       unsigned int& gameHDRFBO, unsigned int& gameHDRTex,
                       unsigned int& sceneFBO, unsigned int& sceneTex, unsigned int& sceneRBO,
                       unsigned int& gameFBO, unsigned int& gameTex, unsigned int& gameRBO)
{
    // Минимальный размер - 100x100
    if (newWidth < 100) newWidth = 100;
    if (newHeight < 100) newHeight = 100;


    const int MSAA_SAMPLES = 4;

    // ---------------------------------------------------------═ Удаляем старые MSAA FBO ══
    glDeleteFramebuffers(1, &sceneMSFBO);
    glDeleteRenderbuffers(1, &sceneMSColorRBO);
    glDeleteRenderbuffers(1, &sceneMSDepthRBO);
    glDeleteFramebuffers(1, &gameMSFBO);
    glDeleteRenderbuffers(1, &gameMSColorRBO);
    glDeleteRenderbuffers(1, &gameMSDepthRBO);

    // ---------------------------------------------------------═ Удаляем старые Resolve FBO ══
    glDeleteFramebuffers(1, &sceneFBO);
    glDeleteTextures(1, &sceneTex);
    glDeleteRenderbuffers(1, &sceneRBO);
    glDeleteFramebuffers(1, &gameFBO);
    glDeleteTextures(1, &gameTex);
    glDeleteRenderbuffers(1, &gameRBO);

    // ---------------------------------------------------------═ Удаляем старые HDR FBO (если есть) ══
    glDeleteFramebuffers(1, &sceneHDRFBO);
    glDeleteTextures(1, &sceneHDRTex);
    glDeleteFramebuffers(1, &gameHDRFBO);
    glDeleteTextures(1, &gameHDRTex);

    // ---------------------------------------------------------═══════════════════════════════════════════════════════════════
    // Пересоздаём Scene MSAA FBO
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &sceneMSFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneMSFBO);

    glGenRenderbuffers(1, &sceneMSColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneMSColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_RGBA16F, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneMSColorRBO);

    glGenRenderbuffers(1, &sceneMSDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneMSDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_DEPTH24_STENCIL8, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneMSDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    // Пересоздаём Game MSAA FBO
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &gameMSFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gameMSFBO);

    glGenRenderbuffers(1, &gameMSColorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gameMSColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_RGBA16F, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gameMSColorRBO);

    glGenRenderbuffers(1, &gameMSDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gameMSDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_DEPTH24_STENCIL8, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gameMSDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    // Пересоздаём Scene Resolve FBO (MSAA → single-sampled)
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &sceneFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

    glGenTextures(1, &sceneTex);
    glBindTexture(GL_TEXTURE_2D, sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newWidth, newHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);

    glGenRenderbuffers(1, &sceneRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    // Пересоздаём Game Resolve FBO (MSAA → single-sampled)
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &gameFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gameFBO);

    glGenTextures(1, &gameTex);
    glBindTexture(GL_TEXTURE_2D, gameTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, newWidth, newHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameTex, 0);

    glGenRenderbuffers(1, &gameRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gameRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, newWidth, newHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gameRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    // Пересоздаём HDR FBO для Scene
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &sceneHDRFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneHDRFBO);
    glGenTextures(1, &sceneHDRTex);
    glBindTexture(GL_TEXTURE_2D, sceneHDRTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, newWidth, newHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneHDRTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    // Пересоздаём HDR FBO для Game
    // ---------------------------------------------------------═════════════════════════════════════════════════════════════════
    glGenFramebuffers(1, &gameHDRFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gameHDRFBO);
    glGenTextures(1, &gameHDRTex);
    glBindTexture(GL_TEXTURE_2D, gameHDRTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, newWidth, newHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gameHDRTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

// ── LevelGen helpers: name-based lookup for the Lua procedural API ──
static bool g_LuaCamActive = false;
static std::vector<SceneObject>* LG_Objs(lua_State* L){ lua_getglobal(L,"__objs"); auto* p=(std::vector<SceneObject>*)lua_touserdata(L,-1); lua_pop(L,1); return p; }
static std::vector<LightObject>* LG_Lights(lua_State* L){ lua_getglobal(L,"__lights"); auto* p=(std::vector<LightObject>*)lua_touserdata(L,-1); lua_pop(L,1); return p; }
static SceneObject* LG_Find(lua_State* L, const char* n){ auto* o=LG_Objs(L); if(!o) return nullptr; for(auto& s:*o) if(s.name==n) return &s; return nullptr; }
int main(int argc, char** argv)
{

            // ── Environment API: время суток и туман из Lua ──
            // Environment.SetTimeOfDay(hours) / GetTimeOfDay()
            // Environment.SetFog(density, r, g, b)
            {
                lua_State* LL = luaInst->L;
                lua_newtable(LL);

                lua_pushstring(LL, "SetTimeOfDay");
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    extern float g_TimeOfDay;
                    float h=(float)luaL_optnumber(L,1,12.0);
                    while(h<0.f)h+=24.f; g_TimeOfDay=fmodf(h,24.f);
                    return 0;
}
                }, 0);
                lua_settable(LL, -3);

                lua_pushstring(LL, "GetTimeOfDay");
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    extern float g_TimeOfDay;
                    lua_pushnumber(L,g_TimeOfDay);
                    return 1;
                }, 0);
                lua_settable(LL, -3);

                lua_pushstring(LL, "SetFog");
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    extern float g_FogDensity;
                    extern glm::vec3 g_FogColor;
                    g_FogDensity = (float)luaL_optnumber(L,1,g_FogDensity);
                    g_FogColor.x = (float)luaL_optnumber(L,2,g_FogColor.x);
                    g_FogColor.y = (float)luaL_optnumber(L,3,g_FogColor.y);
                    g_FogColor.z = (float)luaL_optnumber(L,4,g_FogColor.z);
                    return 0;
                }, 0);
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

                // PlayAnimation / StopAnimation — то же самое, что Play/Stop выше (алиасы под
                // именами, которые обычно ожидают в остальных движках/уроках).
                pushSelfFn("PlayAnimation", [](lua_State* L)->int{
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
                pushSelfFn("StopAnimation", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    if(!g_LuaObjectsPtr) return 0;
                    for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID==id){ o.animPlaying=false; o.animTime=0.f; break; }
                    }
                    return 0;
                });

                // Animation.SetAnimationSpeed(speed) — множитель скорости проигрывания
                // (1.0 = обычная скорость, 2.0 = вдвое быстрее, 0.5 = замедленно, отрицательное — назад).
                pushSelfFn("SetAnimationSpeed", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    float speed=(float)luaL_optnumber(L,1,1.0);
                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    if(!g_LuaObjectsPtr) return 0;
                    for(auto& o : *g_LuaObjectsPtr){
                        if(o.ecsID==id){ o.animSpeed=speed; break; }
                    }
                    return 0;
                });

                lua_setglobal(LL, "Animation");
            }

            std::ifstream sf(scriptPath);
            if(sf){
                std::stringstream ss; ss<<sf.rdbuf();
                if(luaInst->loadScript(ss.str())){
                    luaInst->objX=obj.pos.x;luaInst->objY=obj.pos.y;luaInst->objZ=obj.pos.z;
                    luaInst->objR=obj.color.r;luaInst->objG=obj.color.g;luaInst->objB=obj.color.b;
                    luaInst->objName=obj.name;
                    luaInst->pushObjectData();
                    luaInst->callOnStart();
                    luaInst->pullObjectData();
                    obj.pos.x=luaInst->objX;obj.pos.y=luaInst->objY;obj.pos.z=luaInst->objZ;
                    luaInst->started=true;
                    obj.luaInstances.push_back(luaInst);
        UILua::Register(obj.luaInstances.back()->L);
                } else {
                    logError("Lua load failed: "+obj.name+" ("+fs::path(scriptPath).filename().string()+")");
                }
            }
        logInfo("Play");

    // ── Player mode: сразу грузим сцену, Play включится автоматически после загрузки ──
    if (g_PlayerMode) {
        VE::SceneManager::Get().RequestLoad(g_PlayerScenePath);
        logInfo("Player mode: launching "+g_PlayerScenePath);
    }

    const char* typeNames[]={"Cube","Sphere","Cylinder","Pyramid","Capsule","Plane","Model","Empty"};
    static float hierW=240.f, inspW=280.f, bottomH=190.f;
    const float sideW=54.f, toolH=44.f;

    while(!window->ShouldClose())
    {
        float now=glfwGetTime();deltaTime=now-lastFrame;lastFrame=now;
        // ── Продвигаем время скелетной анимации (играет и в редакторе, для превью) ──
        for(auto& obj:objects){
            if(obj.animPlaying && obj.model && obj.model->hasSkeleton && obj.animIndex>=0)
                obj.animTime += deltaTime * obj.animSpeed;
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
    UILua::TickPopup();
        g_DragHoverObj = -1; // сброс каждый кадр, обновляется в drop target
        float menuH=20.f;
        float viewH=io.DisplaySize.y-menuH-bottomH-toolH;
        float vpW=io.DisplaySize.x-sideW-hierW-inspW;

if (!isPlaying) {
        if(!sceneCameras.empty()){
            for(auto& sc:sceneCameras){if(sc.isPrimary){
            if (!GameLua::g_instances.empty()) { gameCamera.Position = sc.pos; } else
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
                    gameCamera.Position=sc.pos; gameCamera.Front = glm::normalize(((objects.empty()?glm::vec3(0,1,0):objects[0].pos+glm::vec3(0,1,0)))-sc.pos); gameCamera.Up = glm::vec3(0,1,0);
                }
                break;
            }}
        }
} // end !isPlaying camera sync
if (isPlaying && !gameCameraInitialized && !sceneCameras.empty()) {
    if (!GameLua::g_instances.empty()) gameCameraInitialized = true; else
    for (auto& sc:sceneCameras) if (sc.isPrimary) {
        if (sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size()) {
            auto& t=objects[sc.followTargetIndex];
            gameCamera.Position=t.pos+sc.followOffset;
            gameCamera.Yaw=t.rot.y-90.f;
            gameCamera.Pitch=t.lookPitch;
            gameCamera.UpdateVectors();
        } else { gameCamera.Position=sc.pos; gameCamera.Front = glm::normalize(((objects.empty()?glm::vec3(0,1,0):objects[0].pos+glm::vec3(0,1,0)))-sc.pos); gameCamera.Up = glm::vec3(0,1,0); }
        break;
    }
    gameCameraInitialized = true;
}
    // � Play gameCamera ����������� CharacterController/Lua � �� �������������� �

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
                static bool ctrlZWasDown=false, ctrlYWasDown=false;
                bool ctrlDown=glfwGetKey(native,GLFW_KEY_LEFT_CONTROL)==GLFW_PRESS;
                bool zDown=glfwGetKey(native,GLFW_KEY_Z)==GLFW_PRESS;
                bool yDown=glfwGetKey(native,GLFW_KEY_Y)==GLFW_PRESS;
                if(ctrlDown&&zDown&&!ctrlZWasDown) VE::UndoSystem::Get().Undo();
                if(ctrlDown&&yDown&&!ctrlYWasDown) VE::UndoSystem::Get().Redo();
                ctrlZWasDown=ctrlDown&&zDown; ctrlYWasDown=ctrlDown&&yDown;
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
        // Пересчитываем aspect ratio из текущего размера viewport
        float vpAspect = (g_VpSize.x > 1 && g_VpSize.y > 1) ? (g_VpSize.x / g_VpSize.y) : (vpW/viewH);
        glm::mat4 proj=camera.GetProjectionMatrix(vpAspect);

        // ── Рисование маски кистью — приоритет над обычным выделением/гизмо, пока активен режим ──
        if (g_EditorMode==EditorMode::PaintMask && leftDown && selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
            double lx=mouseX-g_VpPos.x, ly=mouseY-g_VpPos.y;
            int vw=(int)g_VpSize.x, vh=(int)g_VpSize.y;
            if (lx>=0&&ly>=0&&lx<vw&&ly<vh) {
                Ray ray=screenToRay(lx,ly,vw,vh,view,proj);
                glm::vec2 uv;
                if (RaycastObjectUV(ray, objects[sel], uv)) {
                    auto& obj = objects[sel];
                    if (!obj.materials.empty() && obj.materials[0].maskPixelSize>0) {
                        bool restoreLayer2 = g_BrushPaintMode != ImGui::GetIO().KeyShift; // тулбар задаёт режим, Shift временно инвертирует
                        StampBrush(obj.materials[0], uv, g_BrushRadius, restoreLayer2);
                    }
                }
            }
        }

        if (g_EditorMode==EditorMode::Object) {
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
                        if(selType==SelectionType::Object&&sel>=0){dragStartRot=objects[sel].rot;dragStartScale=objects[sel].scale;} else if(selType==SelectionType::Camera&&selCamera>=0){dragStartRot=sceneCameras[selCamera].rot;}
                        g_UndoDragObjIndex=sel; g_UndoDragSelType=selType; // Undo/Redo: запоминаем что тащим
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
                else if(gizmoMode==GizmoMode::Rotate&&selType==SelectionType::Camera&&selCamera>=0){
                    float signs[3]={1.f,-1.f,1.f};glm::vec3 ro=dragStartRot;ro[ai]+=p*90.f*signs[ai];sceneCameras[selCamera].rot=ro;
                }
            }
        }
        // ── Undo/Redo: драг гизмо только что закончился — записываем "было/стало" ──
        if (g_PrevDragAxis != GizmoAxis::None && dragAxis == GizmoAxis::None &&
            g_UndoDragSelType == SelectionType::Object &&
            g_UndoDragObjIndex >= 0 && g_UndoDragObjIndex < (int)objects.size())
        {
            int idx = g_UndoDragObjIndex;
            glm::vec3 before, after;
            std::function<void(const glm::vec3&)> setter;
            if (gizmoMode == GizmoMode::Move) {
                before = dragStartPos; after = objects[idx].pos;
                setter = [idx](const glm::vec3& v){ if(g_LuaObjectsPtr && idx < (int)g_LuaObjectsPtr->size()) (*g_LuaObjectsPtr)[idx].pos = v; };
            } else if (gizmoMode == GizmoMode::Rotate) {
                before = dragStartRot; after = objects[idx].rot;
                setter = [idx](const glm::vec3& v){ if(g_LuaObjectsPtr && idx < (int)g_LuaObjectsPtr->size()) (*g_LuaObjectsPtr)[idx].rot = v; };
            } else {
                before = dragStartScale; after = objects[idx].scale;
                setter = [idx](const glm::vec3& v){ if(g_LuaObjectsPtr && idx < (int)g_LuaObjectsPtr->size()) (*g_LuaObjectsPtr)[idx].scale = v; };
            }
            if (before != after)
                VE::UndoSystem::Get().Push(std::make_unique<VE::PropertyChangeCommand<glm::vec3>>(setter, before, after, objects[idx].name));
        }
        g_PrevDragAxis = dragAxis;

        // -- Parenting: ���� ������� �� ��������� --
        static std::vector<glm::vec3> prevPos;
        if ((int)prevPos.size()!=(int)objects.size()) { prevPos.assign(objects.size(), glm::vec3(0)); }
        if (!g_SkipPropagate) {
            for (int pi=0; pi<(int)objects.size(); pi++) {
                glm::vec3 d = objects[pi].pos - prevPos[pi];
                if (glm::length(d) > 1e-6f && glm::length(d) < 5.f) {
                    for (int ci2=0; ci2<(int)objects.size(); ci2++) if (objects[ci2].parentIndex==pi) objects[ci2].pos += d;
                }
            }
        }
        g_SkipPropagate=false;
        for (int pi=0; pi<(int)objects.size(); pi++) prevPos[pi]=objects[pi].pos;
        
        } // if (g_EditorMode==EditorMode::Object)
        leftClickThisFrame=false;

        if(isPlaying&&!isPaused){
    if (isPlaying && !isPaused) {
        for (int oi=0; oi<(int)objects.size(); oi++) {
            if (!scene.registry.HasComponent<VE::CharacterControllerComponent>(objects[oi].ecsID)) {
                for (auto& n : g_CcNames) if (n==objects[oi].name) { scene.registry.AddComponent<VE::CharacterControllerComponent>(objects[oi].ecsID); break; }
            }
            if (scene.registry.HasComponent<VE::CharacterControllerComponent>(objects[oi].ecsID)) {
                auto& cc = scene.registry.GetComponent<VE::CharacterControllerComponent>(objects[oi].ecsID);
                cc.targetObjectIndex = oi;
                
                // ���������� WASD + Space + Shift
                glm::vec3 moveDir(0);
                float speed = cc.controller.Speed;
                if (glfwGetKey(native, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed = cc.controller.RunSpeed;
                
                glm::vec3 forward = glm::normalize(glm::vec3(gameCamera.Front.x, 0, gameCamera.Front.z));
                glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
                
                if (glfwGetKey(native, GLFW_KEY_W) == GLFW_PRESS) moveDir += forward;
                if (glfwGetKey(native, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
                if (glfwGetKey(native, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
                if (glfwGetKey(native, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
                
                if (glm::length(moveDir) > 0.001f) {
                    moveDir = glm::normalize(moveDir) * speed;
                }
                
                if (glfwGetKey(native, GLFW_KEY_SPACE) == GLFW_PRESS) cc.controller.Jump();
                
                // ������ + ��������
                std::vector<VE::CCCollider> ccols;
                for (auto& oo : objects) { if (oo.ecsID!=objects[oi].ecsID && scene.registry.HasComponent<VE::ColliderComponent>(oo.ecsID)) { VE::CCCollider c2; c2.pos=oo.pos; c2.col=scene.registry.GetComponent<VE::ColliderComponent>(oo.ecsID); ccols.push_back(c2); } }
                g_HasCtrl=true;
                cc.controller.Move(moveDir, deltaTime, ccols);
                
                // ��������� ������� �������
                objects[oi].pos = cc.controller.Position;
                scene.GetTransform(objects[oi].ecsID).Position = objects[oi].pos;
                
                // ������ ������� �� ������������
                gameCamera.Position = cc.controller.Position + glm::vec3(0, cc.controller.Height * 0.9f, 0);
                
                // ������ ���� � FPS-������
                break;
            }
        }
        // ���� ��� ����������� � ������� ������
        bool hasAnyCtrl = false;
        for (auto& o : objects) if (scene.registry.HasComponent<VE::CharacterControllerComponent>(o.ecsID)) { hasAnyCtrl=true; break; }
        if (!hasAnyCtrl) glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
            VE::Physics::Get().Step(scene.registry,deltaTime);
            VE::ParticleSystem::Get().Update(deltaTime); // частицы двигаются только пока игра играет
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

    if (g_LuaCamActive || !GameLua::g_instances.empty()) {
            gameCamera.Position = glm::vec3(g_camPosX, g_camPosY, g_camPosZ);
            gameCamera.Front = GameLua::g_camFront;
            gameCamera.Up = glm::vec3(0,1,0);
            gameCamera.UpdateVectors();
        } else {
            // �� Play: gameCamera = primary scene camera (��� � Unity)
            for (auto& sc : sceneCameras) {
                if (sc.isPrimary && sc.active) {
                    gameCamera.Position = sc.pos;
                    gameCamera.Yaw = 90.f - sc.rot.y;
                    gameCamera.Pitch = -sc.rot.x;
                    gameCamera.Fov = sc.fov;
                    gameCamera.UpdateVectors();
                    break;
                }
            }
        }
        gameCamera.UpdateVectors();
        glDisable(GL_SCISSOR_TEST); // ImGui мог оставить scissor включённым с маленьким прямоугольником прошлого кадра

        // ---------------------------------------------------------═══════════════════════════════════════════════════════════════
        // Проверяем, изменился ли размер Viewport, и пересоздаём FBO если нужно
        // ---------------------------------------------------------═══════════════════════════════════════════════════════════════
        int vpWidth = (int)g_VpSize.x;
        int vpHeight = (int)g_VpSize.y;
        if (vpWidth > 0 && vpHeight > 0 && (vpWidth != g_VpLastWidth || vpHeight != g_VpLastHeight)) {
            ResizeViewportFBO(vpWidth, vpHeight,
                            sceneMSFBO, sceneMSColorRBO, sceneMSDepthRBO,
                            gameMSFBO, gameMSColorRBO, gameMSDepthRBO,
                            sceneHDRFBO, sceneHDRTex,
                            gameHDRFBO, gameHDRTex,
                            sceneFBO, sceneTex, sceneRBO,
                            gameFBO, gameTex, gameRBO);
            g_VpLastWidth = vpWidth;
            g_VpLastHeight = vpHeight;
        }

        g_pt0 = std::chrono::high_resolution_clock::now();
        glBindFramebuffer(GL_FRAMEBUFFER,sceneMSFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,sel,false,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,camera,vpAspect,gizmoMode,dragAxis,showSkybox,showGrid,showGizmos,gs,lights,sceneCameras,selLight,selCamera,selType);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,sceneMSFBO);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,sceneHDRFBO);
        glBlitFramebuffer(0,0,(int)g_VpSize.x,(int)g_VpSize.y,0,0,(int)g_VpSize.x,(int)g_VpSize.y,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        g_pt1 = std::chrono::high_resolution_clock::now();
        ApplyBloomAndTonemap(sceneHDRTex, sceneFBO, (int)g_VpSize.x, (int)g_VpSize.y);
        g_pt2 = std::chrono::high_resolution_clock::now();
        // ── Своё же тело не должно быть видно от первого лица (как в Unity/Godot) ──
        g_DrawCallCount = 0;
        int fpExcludeIdx=-1;
        for(auto& sc:sceneCameras){
            if(sc.isPrimary && sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size()){
        g_msScene = std::chrono::duration<float, std::milli>(g_pt1-g_pt0).count();
        g_msBloom = std::chrono::duration<float, std::milli>(g_pt2-g_pt1).count();
                fpExcludeIdx=sc.followTargetIndex; break;
            }
        }
        g_pt3 = std::chrono::high_resolution_clock::now();
        glBindFramebuffer(GL_FRAMEBUFFER,gameMSFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,-1,true,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,gameCamera,vpAspect,gizmoMode,dragAxis,showSkybox,false,false,gs,lights,sceneCameras,-1,-1,SelectionType::None,fpExcludeIdx);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,gameMSFBO);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,gameHDRFBO);
        glBlitFramebuffer(0,0,(int)g_VpSize.x,(int)g_VpSize.y,0,0,(int)g_VpSize.x,(int)g_VpSize.y,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        ApplyBloomAndTonemap(gameHDRTex, gameFBO, (int)g_VpSize.x, (int)g_VpSize.y);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,(int)io.DisplaySize.x,(int)io.DisplaySize.y); // restore full
        auto t4 = std::chrono::high_resolution_clock::now();
        g_msGame = std::chrono::duration<float, std::milli>(t4-g_pt3).count();
        glClearColor(0.08f,0.08f,0.09f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
//    VE2D::RenderFrame(sprites2D, (int)g_VpSize.x, (int)g_VpSize.y);
        ImGui::NewFrame();
        menuH=ImGui::GetFrameHeight();

// ---------------------------------------------------------══════════════════════════════════════════════════════
//   ВИЗУАЛЬНАЯ ТЕМА — чёрный + фиолетовый (Unity6/UE5)
// ---------------------------------------------------------══════════════════════════════════════════════════════
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
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f,0.13f,0.14f,1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f,0.21f,0.24f,1.f));
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

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene",  "Ctrl+N")) logInfo("New scene");
        if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
            std::string sp=projectRoot+"\\Assets\\Scenes\\scene.vescene";
            if(fs::exists(sp)){
                LoadScene(sp,objects,lights,sceneCameras,sel,selType);
                VE::UndoSystem::Get().Clear(); // новая сцена — старая история отмены больше не валидна
                currentScenePath=sp;
                logInfo("Scene loaded: "+sp);
        g_SkipPropagate=true;
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
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, VE::UndoSystem::Get().CanUndo())) VE::UndoSystem::Get().Undo();
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, VE::UndoSystem::Get().CanRedo())) VE::UndoSystem::Get().Redo();
    ImGui::Separator();
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
        if (ImGui::MenuItem("Create Empty")) addObject(objects, PrimitiveType::Empty, sel, selType);
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
            if (ImGui::MenuItem("2D Sprite")) { Sprite2D s; s.name="Sprite2D_"+std::to_string((int)sprites2D.size()+1); sprites2D.push_back(s); selSprite2D=(int)sprites2D.size()-1; logInfo("Created "+s.name); }
            if (ImGui::MenuItem("UI Image")) { UIElement u; u.type=UIElement::Type::Image; u.name="UIImage_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(200,200); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Text")) { UIElement u; u.type=UIElement::Type::Text; u.name="UIText_"+std::to_string((int)uiElements.size()+1); u.text="New Text"; u.size=glm::vec2(200,30); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Button")) { UIElement u; u.type=UIElement::Type::Button; u.name="UIButton_"+std::to_string((int)uiElements.size()+1); u.text="Button"; uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
        if (ImGui::MenuItem("UI Frame")) { UIElement u; u.type=UIElement::Type::Frame; u.name="UIFrame_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(300,200); u.color=glm::vec4(0.15f,0.16f,0.2f,0.9f); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
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
        ImGui::Separator();
        if (ImGui::BeginMenu("Shading")) {
            ImGui::Checkbox("Bloom", &g_BloomEnabled);
            ImGui::SliderFloat("Bloom threshold", &g_BloomThreshold, 0.1f, 3.0f);
            ImGui::SliderFloat("Bloom strength", &g_BloomStrength, 0.0f, 1.5f);
            ImGui::SliderFloat("Exposure", &g_Exposure, 0.2f, 3.0f);
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Material Editor", "Ctrl+M")) { extern bool showMaterialEditor; showMaterialEditor = !showMaterialEditor; }
        if (ImGui::MenuItem("Shader Editor")) { extern bool showShaderEditor; showShaderEditor = !showShaderEditor; }
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
        objects = g_PlaySnapshotObjs; lights = g_PlaySnapshotLights; sceneCameras = g_PlaySnapshotCams; sel=-1; selLight=-1; selCamera=-1; selType=SelectionType::None;
            if(g_MouseCaptured){ g_MouseCaptured=false; glfwSetInputMode(native, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
            VE::Physics::Get().ClearAllBodies();
            VE::ParticleSystem::Get().Clear();
            VE::DebugDraw::Get().Clear();
        VE::InstanceRenderer::Get().Clear();
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
        g_SkipPropagate=true;
        for (auto& o : objects) {
            for (auto& n : g_CcNames) if (n==o.name && !scene.registry.HasComponent<VE::CharacterControllerComponent>(o.ecsID)) scene.registry.AddComponent<VE::CharacterControllerComponent>(o.ecsID);
            for (auto& n : g_ColNames) if (n==o.name && !scene.registry.HasComponent<VE::ColliderComponent>(o.ecsID)) { float hy=(o.type==PrimitiveType::Plane)?0.05f:o.scale.y*0.5f; scene.registry.AddComponent<VE::ColliderComponent>(o.ecsID)=VE::ColliderComponent::Box({o.scale.x*0.5f,hy,o.scale.z*0.5f}); }
        }
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
    ImVec4 pcol = isPaused ? COL_PAUSE : ImVec4(0.15f,0.16f,0.18f,1.f);
    ImGui::PushStyleColor(ImGuiCol_Button, pcol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.90f,.70f,.15f,1.f));
    if (ImGui::Button(" \xe2\x8f\xb8 ", ImVec2(28,22))) isPaused=!isPaused;
    ImGui::PopStyleColor(2);

    // татистика справа
    ImGui::SetCursorPosX(mw - 200.f);
    if (isPlaying) { ImGui::TextColored(COL_PLAY,"  \xe2\x97\x8f "); ImGui::SameLine(0,0); }
            ImGui::SameLine();
            char fpsBuf[256];
            snprintf(fpsBuf, 256, "FPS: %.1f | DC: %d | Scene: %.1fms | Game: %.1fms | Bloom: %.1fms", io.Framerate, g_DrawCallCount, g_msScene, g_msGame, g_msBloom);
            ImGui::TextColored(COL_DIM, "%s", fpsBuf);
    ImGui::SameLine(0,3);
    ImGui::SameLine(0,10);
    ImGui::SameLine(0,3);

    ImGui::EndMainMenuBar();
}

// ---------------------------------------------------------══════════════════════════════════════════════════════
//   DOCKSPACE — область докинга (Hierarchy/Viewport/Inspector/Bottom)
// ---------------------------------------------------------══════════════════════════════════════════════════════
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

    dockspaceId = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0,0), ImGuiDockNodeFlags_None);
    ImGui::End();
    ImGui::PopStyleVar(3);

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

// ---------------------------------------------------------══════════════════════════════════════════════════════
//   PREFERENCES WINDOW — как Editor Settings в Godot
// ---------------------------------------------------------══════════════════════════════════════════════════════
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
        strncpy_s(pathBuf, g_Prefs.defaultProjectPath.c_str(), sizeof(pathBuf)-1);
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
        ImGui::Checkbox("Show grid", &showGrid);
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

// ---------------------------------------------------------══════════════════════════════════════════════════════
//   ENVIRONMENT WINDOW — время суток, облака
// ---------------------------------------------------------══════════════════════════════════════════════════════
// ───────────────────────────────────────────────────────
//   TOOLBAR
// ───────────────────────────────────────────────────────
ImGui::SetNextWindowPos(ImVec2(0, menuH), ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, toolH), ImGuiCond_Always);
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,4));
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f,0.094f,0.106f,1.f));
ImGui::PopStyleColor();
ImGui::PopStyleVar();

// ───────────────────────────────────────────────────────
// ───────────────────────────────────────────────────────
//   SIDE ICON PANEL (like concept - left vertical bar)
// ───────────────────────────────────────────────────────
// -- TOP ICON TOOLBAR (Blender-style) --
ImGui::SetNextWindowPos(ImVec2(0, menuH), ImGuiCond_Always);
ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, toolH), ImGuiCond_Always);
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.094f,0.102f,0.114f,1.f));
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,4));
ImGui::Begin("##toolbar", nullptr,
    ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
    ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus);
if (IconToolButton(IC_SELECT, gizmoMode==GizmoMode::Select, "Select (Q)")) gizmoMode=GizmoMode::Select;
ImGui::SameLine(0,2);
if (IconToolButton(IC_MOVE, gizmoMode==GizmoMode::Move, "Move (W)")) gizmoMode=GizmoMode::Move;
ImGui::SameLine(0,2);
if (IconToolButton(IC_ROTATE, gizmoMode==GizmoMode::Rotate, "Rotate (E)")) gizmoMode=GizmoMode::Rotate;
ImGui::SameLine(0,2);
if (IconToolButton(IC_SCALE, gizmoMode==GizmoMode::Scale, "Scale (R)")) gizmoMode=GizmoMode::Scale;
ImGui::End();
ImGui::PopStyleVar();
ImGui::PopStyleColor();

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
        active ? ImVec4(0.18f,0.20f,0.24f,1.f) : ImVec4(0.f,0.f,0.f,0.f));
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
    IconBtn(IC_HIER, g_SideTab==0, "Hierarchy (H)"); if (ImGui::IsItemClicked()) g_SideTab=0;

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
    if (IconBtn(IC_SETTINGS, g_ShowPreferences, "Settings")) g_ShowPreferences = true;
    if (IconBtn(IC_HELP, false, "Help")) logInfo("Help (coming soon)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Help");
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
ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.11f,0.12f,1.f));
ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 4);
ImGui::InputTextWithHint("##hs", "Search...", hierSearch, sizeof(hierSearch));
ImGui::PopStyleColor();

if (ImGui::BeginPopup("##addobj")) {
    ImGui::PushStyleColor(ImGuiCol_Text, COL_DIM);
    ImGui::Text("  3D Objects"); ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::MenuItem("  Empty"))    addObject(objects,PrimitiveType::Empty,   sel,selType);
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

// -- ��� ����������� ���� ��� � Unity --
if (ImGui::BeginPopup("##hierctx")) {
    bool hasSel = (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size())
               || (selType==SelectionType::Light && selLight>=0 && selLight<(int)lights.size())
               || (selType==SelectionType::Camera && selCamera>=0 && selCamera<(int)sceneCameras.size());
    if (hasSel) {
        if (ImGui::MenuItem("Duplicate") && selType==SelectionType::Object) {
            SceneObject o=objects[sel]; o.name=objects[sel].name+"_copy";
            o.ecsID=scene.CreateEntity(o.name);
            scene.GetTransform(o.ecsID).Position=o.pos;
            objects.push_back(o); sel=(int)objects.size()-1;
            logInfo("Duplicated: "+o.name);
        }
        if (ImGui::MenuItem("Unparent / Detach")) {
            if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) { objects[sel].parentIndex=-1; logInfo("Unparented "+objects[sel].name); }
            else if (selType==SelectionType::Camera && selCamera>=0 && selCamera<(int)sceneCameras.size()) { sceneCameras[selCamera].followTargetIndex=-1; logInfo("Camera detached: "+sceneCameras[selCamera].name); }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            if (selType==SelectionType::Object && sel>=0) { logInfo("Deleted: "+objects[sel].name); objects.erase(objects.begin()+sel); sel=-1; selType=SelectionType::None; selUI=-1; selSprite2D=-1; }
            else if (selType==SelectionType::Light && selLight>=0) { logInfo("Deleted: "+lights[selLight].name); lights.erase(lights.begin()+selLight); selLight=-1; selType=SelectionType::None; selUI=-1; selSprite2D=-1; }
            else if (selType==SelectionType::Camera && selCamera>=0) { logInfo("Deleted: "+sceneCameras[selCamera].name); sceneCameras.erase(sceneCameras.begin()+selCamera); selCamera=-1; selType=SelectionType::None; selUI=-1; selSprite2D=-1; }
        }
        ImGui::Separator();
    }
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Cube"))     addObject(objects,PrimitiveType::Cube,sel,selType);
        if (ImGui::MenuItem("Sphere"))   addObject(objects,PrimitiveType::Sphere,sel,selType);
        if (ImGui::MenuItem("Cylinder")) addObject(objects,PrimitiveType::Cylinder,sel,selType);
        if (ImGui::MenuItem("Pyramid"))  addObject(objects,PrimitiveType::Pyramid,sel,selType);
        if (ImGui::MenuItem("Capsule"))  addObject(objects,PrimitiveType::Capsule,sel,selType);
        if (ImGui::MenuItem("Plane"))    addObject(objects,PrimitiveType::Plane,sel,selType);
        if (ImGui::MenuItem("Empty"))    addObject(objects,PrimitiveType::Empty,sel,selType);
        if (ImGui::MenuItem("Folder")) {
            addObject(objects,PrimitiveType::Empty,sel,selType);
            objects.back().name="Folder_"+std::to_string((int)objects.size());
            logInfo("Created "+objects.back().name);
        }
            if (ImGui::MenuItem("2D Sprite")) { Sprite2D s; s.name="Sprite2D_"+std::to_string((int)sprites2D.size()+1); sprites2D.push_back(s); selSprite2D=(int)sprites2D.size()-1; logInfo("Created "+s.name); }
            if (ImGui::MenuItem("UI Image")) { UIElement u; u.type=UIElement::Type::Image; u.name="UIImage_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(200,200); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Text")) { UIElement u; u.type=UIElement::Type::Text; u.name="UIText_"+std::to_string((int)uiElements.size()+1); u.text="New Text"; u.size=glm::vec2(200,30); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Button")) { UIElement u; u.type=UIElement::Type::Button; u.name="UIButton_"+std::to_string((int)uiElements.size()+1); u.text="Button"; uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
        if (ImGui::MenuItem("UI Frame")) { UIElement u; u.type=UIElement::Type::Frame; u.name="UIFrame_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(300,200); u.color=glm::vec4(0.15f,0.16f,0.2f,0.9f); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
        ImGui::Separator();
        if (ImGui::MenuItem("Point Light")) {
            LightObject l; l.name="PointLight_"+std::to_string(lights.size()+1); l.pos=glm::vec3(0,3,0);
            l.ecsID=scene.CreateEntity(l.name); scene.registry.AddComponent<VE::LightComponent>(l.ecsID,l.color,l.intensity);
            lights.push_back(l); selLight=(int)lights.size()-1; selType=SelectionType::Light; logInfo("Created "+l.name);
        }
        if (ImGui::MenuItem("Camera")) {
            if (ImGui::MenuItem("2D Sprite")) { Sprite2D s; s.name="Sprite2D_"+std::to_string((int)sprites2D.size()+1); sprites2D.push_back(s); selSprite2D=(int)sprites2D.size()-1; logInfo("Created "+s.name); }
            if (ImGui::MenuItem("UI Image")) { UIElement u; u.type=UIElement::Type::Image; u.name="UIImage_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(200,200); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Text")) { UIElement u; u.type=UIElement::Type::Text; u.name="UIText_"+std::to_string((int)uiElements.size()+1); u.text="New Text"; u.size=glm::vec2(200,30); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            if (ImGui::MenuItem("UI Button")) { UIElement u; u.type=UIElement::Type::Button; u.name="UIButton_"+std::to_string((int)uiElements.size()+1); u.text="Button"; uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
        if (ImGui::MenuItem("UI Frame")) { UIElement u; u.type=UIElement::Type::Frame; u.name="UIFrame_"+std::to_string((int)uiElements.size()+1); u.size=glm::vec2(300,200); u.color=glm::vec4(0.15f,0.16f,0.2f,0.9f); uiElements.push_back(u); selUI=(int)uiElements.size()-1; logInfo("Created "+u.name); }
            CameraObject cam; cam.name="Camera_"+std::to_string(sceneCameras.size()+1); cam.pos=glm::vec3(0,2,5);
            cam.ecsID=scene.CreateEntity(cam.name); scene.registry.AddComponent<VE::CameraComponent>(cam.ecsID,false);
            sceneCameras.push_back(cam); selCamera=(int)sceneCameras.size()-1; selType=SelectionType::Camera; logInfo("Created "+cam.name);
        }
        ImGui::EndMenu();
    }
    ImGui::EndPopup();
}
HRule();

// цена дерево
ImGui::SetNextItemOpen(true, ImGuiCond_Once);
ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT_HOV);
bool sceneOpen = ImGui::TreeNodeEx("  Untitled Scene", ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_DefaultOpen);
if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* hp = ImGui::AcceptDragDropPayload("HIER_OBJ")) {
        int src = *(const int*)hp->Data;
        if (src>=0 && src<(int)objects.size()) { objects[src].parentIndex=-1; logInfo("Unparented "+objects[src].name); }
    }
    if (const ImGuiPayload* cp = ImGui::AcceptDragDropPayload("HIER_CAM")) {
        int ci = *(const int*)cp->Data;
        if (ci>=0 && ci<(int)sceneCameras.size()) { sceneCameras[ci].followTargetIndex=-1; logInfo("Camera detached: "+sceneCameras[ci].name); }
    }
    ImGui::EndDragDropTarget();
}
ImGui::PopStyleColor();

if (sceneOpen) {
    // ── Lighting — как сервис в Roblox: постоянный пункт, не объект сцены ──
    {
        bool isEnvSel = (selType == SelectionType::Environment);
        ImGui::PushStyleColor(ImGuiCol_Text, isEnvSel ? ImVec4(1,1,1,1) : ImVec4(1.0f,0.85f,0.4f,1.f));
        if (isEnvSel) ImGui::PushStyleColor(ImGuiCol_Header, COL_ACCENT);
        ImGui::Selectable("  Lighting", isEnvSel);
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

        bool hasChildren = false;
        for (int j=0;j<(int)objects.size();j++) if(objects[j].parentIndex==i){hasChildren=true;break;}
        if(!hasChildren) for (int j=0;j<(int)sceneCameras.size();j++) if(sceneCameras[j].followTargetIndex==i){hasChildren=true;break;}

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selType==SelectionType::Object && i==sel) {
            flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.16f,0.17f,0.20f,1.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f,0.23f,0.26f,1.f));
        }

        ImVec4 labelColor = ImVec4(0.85f,0.85f,0.90f,1.f); // default white
        if (obj.type==PrimitiveType::Model3D) labelColor = ImVec4(0.6f,0.9f,0.6f,1.f); // green
        else if (obj.type==PrimitiveType::Empty) labelColor = ImVec4(0.5f,0.5f,0.55f,1.f); // gray
        if (obj.name.rfind("Folder_",0)==0) labelColor = ImVec4(0.93f,0.79f,0.42f,1.f);
        ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
        std::string label = "  " + obj.name + "##h" + std::to_string(i);
        ImGui::PopStyleColor();
        bool nodeOpen = hasChildren ? ImGui::TreeNodeEx(label.c_str(), flags) : (ImGui::TreeNodeEx(label.c_str(), flags), false);

        if (selType==SelectionType::Object && i==sel) ImGui::PopStyleColor(2);

        if (ImGui::IsItemClicked()) { sel=i; selType=SelectionType::Object; }
        if (ImGui::IsItemClicked(1)) { sel=i; selType=SelectionType::Object; ImGui::OpenPopup("##hierctx"); }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) { ImGui::SetDragDropPayload("HIER_OBJ", &i, sizeof(int)); ImGui::TextUnformatted(obj.name.c_str()); ImGui::EndDragDropSource(); }

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
            if (const ImGuiPayload* hp = ImGui::AcceptDragDropPayload("HIER_OBJ")) {
                int src = *(const int*)hp->Data;
                if (src!=i && src>=0 && src<(int)objects.size()) {
                    bool isDesc=false; for (int p=i; p>=0; p=objects[p].parentIndex) if (p==src) { isDesc=true; break; }
                    if (!isDesc) { objects[src].parentIndex=i; logInfo("Parented "+objects[src].name+" -> "+objects[i].name); }
                }
            }
            if (const ImGuiPayload* cp = ImGui::AcceptDragDropPayload("HIER_CAM")) {
                int ci = *(const int*)cp->Data;
                if (ci>=0 && ci<(int)sceneCameras.size()) {
                    sceneCameras[ci].followTargetIndex=i;
                    sceneCameras[ci].followOffset=sceneCameras[ci].pos-objects[i].pos;
                    logInfo("Camera "+sceneCameras[ci].name+" attached to "+objects[i].name);
                }
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
                strncpy_s(s_HierRenameBuf, objects[i].name.c_str(), sizeof(s_HierRenameBuf)-1);
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
                if(objects.empty()){sel=-1;selType=SelectionType::None;} selUI=-1; selSprite2D=-1;
                ImGui::EndPopup(); if(hasChildren && nodeOpen) ImGui::TreePop(); break;
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

        if (hasChildren) {
            if (nodeOpen) {
                ImGui::Indent(16.f);
        for (int c=0;c<(int)objects.size();c++) {
            if (objects[c].parentIndex!=i) continue;
            bool cSel=(selType==SelectionType::Object&&c==sel);
            if (cSel) { ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0.16f,0.17f,0.20f,1.f)); ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0.22f,0.23f,0.26f,1.f)); }
            ImGui::TreeNodeEx(("  "+objects[c].name+"##h"+std::to_string(c)).c_str(), ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth|(cSel?ImGuiTreeNodeFlags_Selected:0));
            if (cSel) ImGui::PopStyleColor(2);
            if (ImGui::IsItemClicked()) { sel=c; selType=SelectionType::Object; }
            if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) { objects[c].parentIndex=-1; logInfo("Unparented "+objects[c].name); }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) { ImGui::SetDragDropPayload("HIER_OBJ",&c,sizeof(int)); ImGui::TextUnformatted(objects[c].name.c_str()); ImGui::EndDragDropSource(); }
        }
        for (int c=0;c<(int)sceneCameras.size();c++) {
            if (sceneCameras[c].followTargetIndex!=i) continue;
            bool cSel=(selType==SelectionType::Camera&&c==selCamera);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_CAM_OBJ);
            ImGui::TreeNodeEx(("  "+sceneCameras[c].name+"##h"+std::to_string(c)).c_str(), ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth|(cSel?ImGuiTreeNodeFlags_Selected:0));
            ImGui::PopStyleColor(1);
            if (ImGui::IsItemClicked()) { selCamera=c; selType=SelectionType::Camera; }
            if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) { sceneCameras[c].followTargetIndex=-1; logInfo("Camera detached: "+sceneCameras[c].name); }
                if (ImGui::IsItemClicked(1)) { selCamera=c; selType=SelectionType::Camera; ImGui::OpenPopup("##hierctx"); }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) { ImGui::SetDragDropPayload("HIER_CAM",&c,sizeof(int)); ImGui::TextUnformatted(sceneCameras[c].name.c_str()); ImGui::EndDragDropSource(); }
        }
                ImGui::Unindent(16.f);
            }
            ImGui::TreePop();
        }
        }
    }
    // Lights
    for (int i=0;i<(int)lights.size();i++) {
        ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(selType==SelectionType::Light&&i==selLight) flags|=ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Text, COL_LIGHT_OBJ);
        ImGui::TreeNodeEx(("  "+lights[i].name+"##h"+std::to_string(i)).c_str(), flags);
        ImGui::PopStyleColor();
        if(ImGui::IsItemClicked()){selLight=i;selType=SelectionType::Light;}
    }
    // Cameras
    for (int i=0;i<(int)sceneCameras.size();i++) {
        if (sceneCameras[i].followTargetIndex>=0 && sceneCameras[i].followTargetIndex<(int)objects.size()) continue;
        ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if(selType==SelectionType::Camera&&i==selCamera) flags|=ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Text, COL_CAM_OBJ);
        ImGui::TreeNodeEx(("  "+sceneCameras[i].name+"##h"+std::to_string(i)).c_str(), flags);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) { ImGui::SetDragDropPayload("HIER_CAM", &i, sizeof(int)); ImGui::TextUnformatted(sceneCameras[i].name.c_str()); ImGui::EndDragDropSource(); }
        ImGui::PopStyleColor();
        if(ImGui::IsItemClicked()){selCamera=i;selType=SelectionType::Camera;}
    }
    ImGui::TreePop();

    static bool s_uiNodeHover=false; s_uiNodeHover=false;
    // -- Canvas (UI) � ��� � Unity --
    {
        bool canOpen = ImGui::TreeNodeEx("  Canvas", ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_NoTreePushOnOpen);
        if (canOpen) {
                ImGui::Indent(16.f);
            for (int i2=0;i2<(int)uiElements.size();i2++) {
                bool s2=(selUI==i2);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f,0.5f,0.8f,1.f));
                ImGui::TreeNodeEx(("  "+uiElements[i2].name+"##h"+std::to_string(i2)).c_str(), ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth|(s2?ImGuiTreeNodeFlags_Selected:0));
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) s_uiNodeHover=true;
                if (ImGui::IsItemClicked()) { selUI=i2; selSprite2D=-1; sel=-1; selType=SelectionType::None; }
             if (ImGui::BeginDragDropTarget()) {
                 if (const ImGuiPayload* tp =
                     ImGui::AcceptDragDropPayload(
                     "TEXTURE_PATH")) {
                     std::string tp2(
                         (const char*)tp->Data,
                         tp->DataSize-1);
                     UI_TexPick(i2, tp2);
                 }
                 ImGui::EndDragDropTarget();
             }
             if (ImGui::BeginDragDropTarget()) {
                 if (const ImGuiPayload* tp =
                     ImGui::AcceptDragDropPayload(
                     "TEXTURE_PATH")) {
                     std::string tp2(
                         (const char*)tp->Data,
                         tp->DataSize-1);
                     UI_TexPick(i2, tp2);
                 }
                 ImGui::EndDragDropTarget();
             }
             if (ImGui::BeginDragDropTarget()) {
                 if (const ImGuiPayload* tp = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
                     std::string texPath((const char*)tp->Data, tp->DataSize-1);
                     GLuint t = VE::LoadTextureRaw(texPath);
                     if (t) { uiElements[i2].tex = t; uiElements[i2].texPath = texPath; logInfo("Texture -> "+uiElements[i2].name); }
                 }
                 ImGui::EndDragDropTarget();
             }
            }
                ImGui::Unindent(16.f);
            }
        }
        // -- 2D Sprites --
    for (int i2=0;i2<(int)sprites2D.size();i2++) {
        bool s2=(selSprite2D==i2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.9f,0.9f,1.f));
        ImGui::TreeNodeEx(("  "+sprites2D[i2].name).c_str(), ImGuiTreeNodeFlags_Leaf|ImGuiTreeNodeFlags_NoTreePushOnOpen|ImGuiTreeNodeFlags_SpanAvailWidth|(s2?ImGuiTreeNodeFlags_Selected:0));
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) { selSprite2D=i2; selUI=-1; sel=-1; selType=SelectionType::None; }
    }
} // end g_SideTab == 0 (Hierarchy)

    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) ImGui::OpenPopup("##hierctx");
ImGui::End();
ImGui::PopStyleColor();

// ───────────────────────────────────────────────────────
//   VIEWPORT
// ───────────────────────────────────────────────────────
ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.071f,0.078f,0.090f,1.f));
ImGui::PushStyleColor(ImGuiCol_Tab,         ImVec4(0.08f,0.09f,0.10f,1.f));
ImGui::PushStyleColor(ImGuiCol_TabActive,   ImVec4(0.16f,0.17f,0.20f,1.f));
ImGui::PushStyleColor(ImGuiCol_TabHovered,  ImVec4(0.22f,0.23f,0.26f,1.f));
ImGui::Begin("Viewport##viewport", nullptr,
    ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar);

if (ImGui::BeginTabBar("##vptabs")) {
    if (g_WantGameTab) { ImGuiTabBar* tb=ImGui::GetCurrentTabBar(); if (tb) tb->NextSelectedTabId = ImGui::GetID("  Game"); g_WantGameTab=false; }
    if (ImGui::BeginTabItem("  Scene")) {
        float tw=ImGui::GetContentRegionAvail().x, th=ImGui::GetContentRegionAvail().y;
        g_VpPos=ImGui::GetCursorScreenPos(); g_VpSize=ImVec2(tw,th);
        // еперь FBO динамически масштабируется под размер ImGui окна,
        // поэтому UV всегда (0,1)-(1,0) для полного отображения текстуры
        ImGui::Image((ImTextureID)(intptr_t)sceneTex, ImVec2(tw,th), ImVec2(0,1), ImVec2(1,0));
        { ImVec2 gmin=ImGui::GetItemRectMin(); ImVec2 gsz=ImGui::GetItemRectSize(); VEUI::Draw(uiElements, gmin, gsz, true, &selUI); if (VEUI::clickedElement) { sel=-1; selCamera=-1; selLight=-1; selSprite2D=-1; selType=SelectionType::None; } }

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
            static char s_PrefabNameBuf[128] = {};
            if (ImGui::MenuItem("  Save as Prefab...", nullptr, false, hasSel)) {
                if (hasSel) {
                    strncpy_s(s_PrefabNameBuf, objects[sel].name.c_str(), sizeof(s_PrefabNameBuf)-1);
                    s_PrefabNameBuf[sizeof(s_PrefabNameBuf)-1]='\0';
                    ImGui::OpenPopup("##save_prefab");
                }
            }
            if (ImGui::MenuItem("  Delete", "Del", false, hasSel)) {
                if (hasSel) {
                    if(scene.IsAlive(objects[sel].ecsID)) scene.DestroyEntity(objects[sel].ecsID);
                    logInfo("Deleted: "+objects[sel].name);
                    objects.erase(objects.begin()+sel);
                    sel=(int)objects.size()-1;
                    if(objects.empty()){sel=-1;selType=SelectionType::None;} selUI=-1; selSprite2D=-1;
                }
            }

            // ── "Save as Prefab" popup ──
            if (ImGui::BeginPopupModal("##save_prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Prefab name:");
                ImGui::SetNextItemWidth(300);
                ImGui::InputText("##prefab_name_input", s_PrefabNameBuf, sizeof(s_PrefabNameBuf));
                ImGui::Spacing();
                if (ImGui::Button("Save", ImVec2(120,0))) {
                    if (s_PrefabNameBuf[0] && sel>=0 && sel<(int)objects.size()) {
                        try {
                            fs::path prefabDir = fs::path(projectRoot) / "Assets" / "Prefabs";
                            fs::create_directories(prefabDir);
                            fs::path outPath = prefabDir / (std::string(s_PrefabNameBuf) + ".veprefab");

                            PrefabColliderInfo colInfo;
                            auto& srcObj = objects[sel];
                            if (scene.registry.HasComponent<VE::ColliderComponent>(srcObj.ecsID)) {
                                auto& c = scene.registry.GetComponent<VE::ColliderComponent>(srcObj.ecsID);
                                colInfo.hasCollider = true;
                                colInfo.shape = (int)c.Shape;
                                colInfo.hx=c.HalfSize.x; colInfo.hy=c.HalfSize.y; colInfo.hz=c.HalfSize.z;
                                colInfo.radius=c.Radius; colInfo.height=c.Height;
                                colInfo.isTrigger=c.IsTrigger;
                            }
                            SavePrefab(outPath.string(), srcObj, colInfo);
                        } catch(const std::exception& ex){ logError(ex.what()); }
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120,0))) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
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
        if (gameTex) {
            ImVec2 vp = ImGui::GetContentRegionAvail();
            ImVec2 uv0(0,0), uv1(1,1);
            ImGui::Image((ImTextureID)(intptr_t)gameTex, vp, ImVec2(0,1), ImVec2(1,0));
            ImVec2 gmin=ImGui::GetItemRectMin(); ImVec2 gsz=ImGui::GetItemRectSize();
            VEUI::Draw(uiElements, gmin, gsz, false, nullptr);
            if (VEUI::clickedThisFrame>=0 && VEUI::clickedThisFrame<(int)uiElements.size()) {
                std::string clickedName = uiElements[VEUI::clickedThisFrame].name;
                for(auto& obj:objects) for(auto& li:obj.luaInstances) if(li && li->L) {
                    lua_getglobal(li->L,"UI");
                    if(lua_istable(li->L,-1)){
                        lua_getfield(li->L,-1,"_callbacks");
                        if(lua_istable(li->L,-1)){
                            lua_getfield(li->L,-1,clickedName.c_str());
                            if(lua_istable(li->L,-1)){
                                int len=(int)lua_rawlen(li->L,-1);
                                for(int k=1;k<=len;k++){
                                    lua_rawgeti(li->L,-1,k);
                                    if(lua_isfunction(li->L,-1)){
                                        if(lua_pcall(li->L,0,0,0)!=LUA_OK) logError("[Lua] "+std::string(lua_tostring(li->L,-1)));
                                    } else lua_pop(li->L,1);
                                }
                            }
                            lua_pop(li->L,1);
                        }
                        lua_pop(li->L,1);
                    }
                    lua_pop(li->L,1);
                }
            }
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
ImGui::End();
ImGui::PopStyleColor(4); ImGui::PopStyleVar();
// ------------------------------------------------------------------------------
//   INSPECTOR
// ------------------------------------------------------------------------------
ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.078f,0.086f,0.098f,1.f));
ImGui::Begin("Inspector##inspector", nullptr, ImGuiWindowFlags_NoCollapse);

if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
    SceneObject& obj = objects[sel];
    static char nameBuf[128]; static int nameIdx=-1;
    if (nameIdx!=sel) { strncpy_s(nameBuf, obj.name.c_str(), 127); nameIdx=sel; }
    if (ImGui::InputText("Name", nameBuf, 128)) obj.name = nameBuf;
    ImGui::Separator();
    ImGui::DragFloat3("Position", &obj.pos.x, 0.05f);
    ImGui::DragFloat3("Rotation", &obj.rot.x, 0.5f);
    ImGui::DragFloat3("Scale", &obj.scale.x, 0.05f);
    ImGui::ColorEdit3("Color", &obj.color.r);
    ImGui::Checkbox("Active", &obj.active);
    ImGui::Separator();
    static bool s_wantNewScript=false;
            if (selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()) {
                ImGui::TextDisabled("Scripts (%d):", (int)objects[sel].scripts.size());
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f,0.16f,0.19f,1.f));
                ImGui::BeginChild("##scriptsBox", ImVec2(-1, 70), true);
                for (int si=0; si<(int)objects[sel].scripts.size(); si++) {
                    std::string nm = objects[sel].scripts[si];
                    size_t posA = nm.find("Assets"); if (posA!=std::string::npos) nm = nm.substr(posA);
                    size_t fs2 = nm.find_last_of("/\\"); std::string dn = (fs2!=std::string::npos)? nm.substr(fs2+1) : nm;
                    ImGui::TextColored(ImVec4(0.6f,1.f,0.6f,1.f), " %s", dn.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(("X##scrx"+std::to_string(si)).c_str())) { objects[sel].scripts.erase(objects[sel].scripts.begin()+si); si--; }
                    ImGui::Separator();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* pay = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
                        const char* p = (const char*)pay->Data;
                        std::string sp = p;
                        bool dup=false; for (auto& s2 : objects[sel].scripts) if (s2==sp) dup=true;
                        if (!dup) objects[sel].scripts.push_back(sp);
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::TextDisabled("drag .lua from Project (can drop several)");
            }
    if (ImGui::Button("Add Module", ImVec2(-1,0))) ImGui::OpenPopup("##add_comp");
for (size_t si=0; si<obj.scriptPaths.size(); si++) {
    std::string fn = fs::path(obj.scriptPaths[si]).filename().string();
    ImGui::TextColored(ImVec4(0.6f,1.f,0.6f,1.f), "  [lua] %s", fn.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(("x##rmc"+std::to_string(si)).c_str())) {
        obj.scriptPaths.erase(obj.scriptPaths.begin()+si);
        if (obj.scriptPaths.empty()) obj.hasScript=false; }
}
    if (ImGui::BeginPopup("##add_comp")) {
        ImGui::TextColored(COL_DIM, "  Add Module");
        ImGui::Separator();
        ImGui::TextColored(COL_DIM, "  Physics");
        if (ImGui::Selectable("Rigidbody")) {
            if (!obj.hasRigidBody) {
                obj.hasRigidBody=true; obj.mass=1.f; obj.useGravity=true;
                if (scene.IsAlive(obj.ecsID)) { auto& rb=scene.registry.AddComponent<VE::RigidbodyComponent>(obj.ecsID); rb.Mass=1.f; rb.UseGravity=true; }
                logInfo("Rigidbody -> "+obj.name);
            } else logWarn("Already has Rigidbody");
        }
        if (ImGui::Selectable("Box Collider")) {
            if (!obj.hasCollider) { obj.hasCollider=true;
                if (scene.IsAlive(obj.ecsID)) { VE::ColliderComponent c; c.Shape=(VE::ColliderComponent::ShapeType)0; c.HalfSize={obj.scale.x*0.5f,obj.scale.y*0.5f,obj.scale.z*0.5f}; scene.registry.AddComponent<VE::ColliderComponent>(obj.ecsID)=c; }
                logInfo("Box Collider -> "+obj.name);
            } else logWarn("Already has Collider");
        }
        if (ImGui::Selectable("Sphere Collider")) {
            if (!obj.hasCollider) { obj.hasCollider=true;
                if (scene.IsAlive(obj.ecsID)) { VE::ColliderComponent c; c.Shape=(VE::ColliderComponent::ShapeType)1; c.Radius=obj.scale.x*0.5f; scene.registry.AddComponent<VE::ColliderComponent>(obj.ecsID)=c; }
                logInfo("Sphere Collider -> "+obj.name);
            } else logWarn("Already has Collider");
        }
        if (ImGui::Selectable("Capsule Collider")) {
            if (!obj.hasCollider) { obj.hasCollider=true;
                if (scene.IsAlive(obj.ecsID)) { VE::ColliderComponent c; c.Shape=(VE::ColliderComponent::ShapeType)2; c.Radius=obj.scale.x*0.5f; c.Height=obj.scale.y; scene.registry.AddComponent<VE::ColliderComponent>(obj.ecsID)=c; }
                logInfo("Capsule Collider -> "+obj.name);
            } else logWarn("Already has Collider");
        }
        ImGui::Separator();
        ImGui::TextColored(COL_DIM, "  Light");
        if (ImGui::Selectable("Point Light")) {
            LightObject lo; lo.name=obj.name+"_Light"; lo.pos=obj.pos;
            lo.color={1,1,1}; lo.intensity=10.f; lo.range=10.f;
            lo.ecsID=scene.CreateEntity(lo.name);
            scene.registry.AddComponent<VE::LightComponent>(lo.ecsID,lo.color,lo.intensity);
            lights.push_back(lo);
            logInfo("Point Light added at "+obj.name);
        }
        ImGui::Separator();
        ImGui::TextColored(COL_DIM, "  Scripts (.lua)");
        if (ImGui::Selectable("Lua Script...")) { s_wantNewScript=true; ImGui::CloseCurrentPopup(); }



        ImGui::EndPopup();


    }
    if (s_wantNewScript) { ImGui::OpenPopup("##new_comp_script"); s_wantNewScript=false; }
    if (ImGui::BeginPopupModal("##new_comp_script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nsBuf[128] = "NewScript";
        ImGui::Text("Script name:");
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("##ns_name", nsBuf, sizeof(nsBuf));
        if (ImGui::Button("Create", ImVec2(100,0))) {
            std::string name = nsBuf;
            if (name.find(".lua")==std::string::npos) name += ".lua";
            std::string dir = projectRoot + "\\Assets\\Scripts";
            try { fs::create_directories(dir); } catch(...) {}
            std::string sp = dir + "\\" + name;
            std::ofstream f(sp);
            f << "-- " << name << "\nfunction onStart()\nend\nfunction onUpdate(dt)\nend\n";
            f.close();
            bool has=false;
            for(auto& s:obj.scriptPaths) if(s==sp){has=true;break;}
            if(!has){ obj.scriptPaths.push_back(sp); obj.hasScript=true; }
            logInfo("Created+added: "+name+" -> "+obj.name);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::Dummy(ImVec2(-1, 40));
ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0,0,0,0));
if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) {
        std::string sp2((const char*)pl->Data, pl->DataSize-1);
        bool already=false;
        for(auto& s2:obj.scriptPaths) if(s2==sp2){ already=true; break; }
        if(!already){ obj.scriptPaths.push_back(sp2); obj.hasScript=true;
            logInfo("Script -> "+obj.name); }
    }
    ImGui::EndDragDropTarget();
}
ImGui::PopStyleColor();
} else if (selType==SelectionType::Light && selLight>=0 && selLight<(int)lights.size()) {
    LightObject& l = lights[selLight];
    ImGui::TextColored(ImVec4(1,1,.3f,1.f), "%s (Light)", l.name.c_str());
    ImGui::Separator();
    ImGui::DragFloat3("Position", &l.pos.x, 0.1f);
    ImGui::ColorEdit3("Color", &l.color.r);
    ImGui::DragFloat("Intensity", &l.intensity, 0.1f, 0.f, 100.f);
} else if (selType==SelectionType::Camera && selCamera>=0 && selCamera<(int)sceneCameras.size()) {
    CameraObject& cam = sceneCameras[selCamera];
    ImGui::TextColored(ImVec4(.3f,1.f,.5f,1.f), "%s (Camera)", cam.name.c_str());
    ImGui::Separator();
    ImGui::DragFloat3("Position", &cam.pos.x, 0.05f);
    ImGui::DragFloat3("Rotation", &cam.rot.x, 0.5f);
    ImGui::DragFloat("FOV", &cam.fov, 0.5f, 10.f, 120.f);
    ImGui::Checkbox("Primary", &cam.isPrimary);
} else if (selUI>=0 && selUI<(int)uiElements.size()) {
    UIElement& u = uiElements[selUI];
    ImGui::TextColored(ImVec4(0.95f,0.5f,0.8f,1.f), "%s (UI)", u.name.c_str());
    ImGui::Separator();
    static char uiNameBuf[128]; static int uiNameIdx=-1;
    if (uiNameIdx!=selUI) { strncpy_s(uiNameBuf, u.name.c_str(), 127); uiNameIdx=selUI; }
    if (ImGui::InputText("Name", uiNameBuf, 128)) u.name = uiNameBuf;
    ImGui::DragFloat2("Anchor (0..1)", &u.anchor.x, 0.005f, 0.f, 1.f);
    ImGui::DragFloat2("Size (px)", &u.size.x, 1.f, 1.f, 4000.f);
        if (ImGui::BeginCombo("Parent", (u.parentIndex>=0 && u.parentIndex<(int)uiElements.size()) ? uiElements[u.parentIndex].name.c_str() : "None (Canvas)")) {
            if (ImGui::Selectable("None (Canvas)", u.parentIndex<0)) u.parentIndex=-1;
            for (int pi2=0; pi2<(int)uiElements.size(); pi2++) { if (pi2==selUI) continue;
                if (ImGui::Selectable(uiElements[pi2].name.c_str(), u.parentIndex==pi2)) u.parentIndex=pi2; }
            ImGui::EndCombo();
        }
        ImGui::DragFloat2("Offset (px)", &u.posOffset.x, 1.f, -4000.f, 4000.f);
        ImGui::DragFloat2("Size Scale", &u.sizeScale.x, 0.005f, 0.f, 1.f);
        ImGui::DragFloat2("Pivot", &u.anchorPoint.x, 0.005f, 0.f, 1.f);
        ImGui::SliderFloat("Rounding", &u.cornerRadius, 0.f, 40.f, "%.0f");
        ImGui::SliderFloat("Transparency", &u.transparency, 0.f, 1.f);
        if (u.type==UIElement::Type::Image || u.type==UIElement::Type::Button) {
            static char uiTexBuf[256]; static int uiTexIdx=-1;
            if (uiTexIdx!=selUI) { strncpy_s(uiTexBuf, u.texPath.c_str(), 255); uiTexBuf[255]=0; uiTexIdx=selUI; }
            ImGui::InputText("Texture", uiTexBuf, 256);
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                u.texPath = uiTexBuf;
                fs::path fp = u.texPath;
                if (fp.is_relative()) fp = fs::path(projectRoot) / u.texPath;
                GLuint t = VE::LoadTextureRaw(fp.string());
                if (t) { u.tex = t; logInfo("UI texture loaded: "+u.name); } else logError("UI texture failed: "+fp.string());
            }
        }
    ImGui::ColorEdit4("Color", &u.color.r);
    if (u.type!=UIElement::Type::Image) {
        static char uiTxtBuf[256]; static int uiTxtIdx=-1;
        if (uiTxtIdx!=selUI) { strncpy_s(uiTxtBuf, u.text.c_str(), 255); uiTxtIdx=selUI; }
        if (ImGui::InputText("Text", uiTxtBuf, 256)) u.text = uiTxtBuf;
        ImGui::DragFloat("Font Size", &u.fontSize, 0.5f, 6.f, 72.f);
    }
    ImGui::DragInt("Z", &u.z);
    ImGui::Checkbox("Visible", &u.visible);
    ImGui::Checkbox("Hover FX", &u.fx);
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* tp =
            ImGui::AcceptDragDropPayload(
            "TEXTURE_PATH")) {
            std::string tp2(
                (const char*)tp->Data,
                tp->DataSize-1);
            UI_TexPick(selUI, tp2);
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* tp =
            ImGui::AcceptDragDropPayload(
            "TEXTURE_PATH")) {
            std::string tp2(
                (const char*)tp->Data,
                tp->DataSize-1);
            UI_TexPick(selUI, tp2);
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* tp = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
            std::string texPath((const char*)tp->Data, tp->DataSize-1);
            GLuint t = VE::LoadTextureRaw(texPath);
            if (t) { u.tex = t; u.texPath = texPath; logInfo("Texture -> "+u.name); }
        }
        ImGui::EndDragDropTarget();
    }
} else {
    float tw = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((tw - ImGui::CalcTextSize("Nothing selected").x)*0.5f);
    ImGui::TextColored(COL_DIM, "Nothing selected");
    ImGui::Spacing();
    ImGui::SetCursorPosX((tw - ImGui::CalcTextSize("Click an object in the Outliner").x)*0.5f);
    ImGui::TextColored(ImVec4(0.28f,0.29f,0.32f,1.f),"Click an object in the Outliner");
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
    static bool s_Collapse=false;
    static bool s_ShowInfo=true, s_ShowWarn=true, s_ShowErr=true;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f,0.15f,0.17f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f,0.26f,0.30f,1.f));
    if (ImGui::SmallButton(" Clear ")) consoleLog.clear();
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0,8);
    ImGui::Checkbox("Collapse", &s_Collapse);
    ImGui::SameLine(0,16);
    float logH = ImGui::GetContentRegionAvail().y - 28.f;
    if (ImGui::BeginChild("##clog", ImVec2(-1, logH))) {
        for (size_t i=0;i<consoleLog.size();i++) {
            auto& e = consoleLog[i];
            bool show = (e.level==2)?s_ShowErr:(e.level==1)?s_ShowWarn:s_ShowInfo;
            if (!show) continue;
            int run=1;
            if (s_Collapse) {
                if (i>0 && consoleLog[i-1].msg==e.msg && consoleLog[i-1].level==e.level) continue;
                for (size_t j=i+1;j<consoleLog.size() && consoleLog[j].msg==e.msg && consoleLog[j].level==e.level;j++) run++;
            }
            ImVec4 col = e.level==2 ? ImVec4(1.f,.35f,.35f,1.f)
                       : e.level==1 ? ImVec4(1.f,.80f,.25f,1.f)
                       : e.level==3 ? ImVec4(0.55f,0.90f,1.f,1.f)
                       :              ImVec4(.78f,.75f,.88f,1.f);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 ic(p.x+8, p.y+ImGui::GetTextLineHeight()*0.5f+1);
            ImU32 icCol = e.level==2?IM_COL32(220,60,60,255):e.level==1?IM_COL32(230,180,40,255):IM_COL32(80,160,220,255);
            dl->AddCircleFilled(ic, 6.f, icCol, 16);
            const char* ch = e.level==2?"x":e.level==1?"!":"i";
            ImVec2 ts = ImGui::CalcTextSize(ch);
            dl->AddText(ImVec2(ic.x-ts.x*0.5f, ic.y-ts.y*0.5f), IM_COL32(255,255,255,255), ch);
            ImGui::Dummy(ImVec2(20,0));
            ImGui::SameLine(0,0);
            if (run>1) ImGui::TextColored(col, "%s  (x%d)", e.msg.c_str(), run);
            else       ImGui::TextColored(col, "%s", e.msg.c_str());
        }
        if (ImGui::GetScrollY()>=ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
    }
    ImGui::EndChild();
    HRule();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f,0.09f,0.11f,1.f));
    ImGui::TextColored(ImVec4(0.50f,0.52f,0.58f,1.f), ">");
    ImGui::SameLine(0,4);
    ImGui::PushItemWidth(-60.f);
    bool enterPressed = false;
    struct CmdCallback {
        static int cb(ImGuiInputTextCallbackData* d) {
            if (d->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (g_CmdHistory.empty()) return 0;
                if (d->EventKey == ImGuiKey_UpArrow) { if (g_CmdHistoryIdx < (int)g_CmdHistory.size()-1) g_CmdHistoryIdx++; }
                else if (d->EventKey == ImGuiKey_DownArrow) { if (g_CmdHistoryIdx > -1) g_CmdHistoryIdx--; }
                std::string val = g_CmdHistoryIdx >= 0 ? g_CmdHistory[g_CmdHistoryIdx] : "";
                d->DeleteChars(0, d->BufTextLen);
                d->InsertChars(0, val.c_str());
            }
            return 0;
        }
    };
    if (g_ConsoleFocusInput) { ImGui::SetKeyboardFocusHere(); g_ConsoleFocusInput=false; }
    if (ImGui::InputTextWithHint("##cmd", "help", g_CmdBuf, sizeof(g_CmdBuf),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
        CmdCallback::cb)) {
        enterPressed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    ImGui::SameLine(0,4);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f,0.17f,0.20f,1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f,0.44f,0.48f,1.f));
    if (ImGui::Button("Run", ImVec2(56,0))) enterPressed = true;
    ImGui::PopStyleColor(2);
    if (enterPressed && g_CmdBuf[0] != '\0') {
        std::string cmd = g_CmdBuf;
        consoleLog.push_back({cmd, 3});
        g_CmdHistory.insert(g_CmdHistory.begin(), cmd);
        if (g_CmdHistory.size() > 50) g_CmdHistory.pop_back();
        g_CmdHistoryIdx = -1;
        memset(g_CmdBuf, 0, sizeof(g_CmdBuf));
        g_ConsoleFocusInput = true;
        std::istringstream ss(cmd);
        std::string token; std::vector<std::string> args;
        while (ss >> token) args.push_back(token);
        std::string c = args.empty() ? "" : args[0];
        if (c=="help") {
            logInfo("Commands: help, clear, echo, fps, ls, mkdir, list objects|lights,");
            logInfo("  select <name>, create <type>, move/scale <name> x y z,");
            logInfo("  rename <n>, color r g b, delete <name>, spawn <type>,");
            logInfo("  volume <0..1>, play <path>, time/sun/ambient <v>,");
logInfo("  fog <d> [r g b], bloom <t> [s]|on|off, exposure <v>, skybox/grid on|off");
            logInfo("  scene save|load <path>|reload|current");
        }
        else if (c=="clear") consoleLog.clear();
        else if (c=="echo") { std::string out; for(size_t i=1;i<args.size();i++) out+=args[i]+" "; logInfo(out); }
        else if (c=="fps") logInfo("FPS: "+std::to_string((int)io.Framerate));
        else if (c=="mkdir" && args.size()>=2) {
            try { fs::path p=args[1]; if(p.is_relative()) p=fs::path(projectRoot)/p; fs::create_directories(p); logInfo("Created: "+p.string()); } catch(const std::exception& ex){ logError(ex.what()); }
        }
        else if (c=="ls"||c=="dir") {
            fs::path p = args.size()>=2 ? fs::path(args[1]) : fs::path(assetCurrentPath);
            if (p.is_relative()) p = fs::path(projectRoot)/p;
            try { for (auto& e : fs::directory_iterator(p)) logInfo(std::string(e.is_directory()?"[dir] ":"     ")+e.path().filename().string()); } catch(...){ logError("Folder not found: "+p.string()); }
        }
        else if (c=="list" && args.size()>=2 && args[1]=="objects") { for(size_t i=0;i<objects.size();i++) logInfo("["+std::to_string(i)+"] "+objects[i].name); }
        else if (c=="list" && args.size()>=2 && args[1]=="lights") { for(size_t i=0;i<lights.size();i++) logInfo("["+std::to_string(i)+"] "+lights[i].name); }
        else if (c=="select" && args.size()>=2) {
            bool f=false;
            for(int i=0;i<(int)objects.size();i++) if(objects[i].name==args[1]){sel=i;selType=SelectionType::Object;f=true;logInfo("Selected: "+objects[i].name);break;}
            if(!f) logWarn("Object not found: "+args[1]);
        }
        else if (c=="create" && args.size()>=2) {
            std::string t=args[1];
            const char* tn[]={"cube","sphere","cylinder","pyramid","capsule","plane","model","empty"};
            bool done=false;
            for(int ti=0;ti<8;ti++){ if(t==tn[ti]){ addObject(objects,(PrimitiveType)ti,sel,selType); done=true; break; } }
            if(!done) logInfo("Unknown type: "+t);
        }
        else if (c=="move" && args.size()>=5) {
            bool f=false;
            for(auto& o : objects){ if(o.name==args[1]){ o.pos=glm::vec3(std::stof(args[2]),std::stof(args[3]),std::stof(args[4])); logInfo("Moved: "+o.name); f=true; break; } }
            if(!f) logInfo("Not found: "+args[1]);
        }
        else if (c=="move" && args.size()>=4 && selType==SelectionType::Object && sel>=0) {
            objects[sel].pos={std::stof(args[1]),std::stof(args[2]),std::stof(args[3])};
            logInfo("Moved: "+objects[sel].name);
        }
        else if (c=="scale" && args.size()>=5) {
            bool f=false;
            for(auto& o : objects){ if(o.name==args[1]){ o.scale=glm::vec3(std::stof(args[2]),std::stof(args[3]),std::stof(args[4])); logInfo("Scaled: "+o.name); f=true; break; } }
            if(!f) logInfo("Not found: "+args[1]);
        }
        else if (c=="scale" && args.size()>=4 && selType==SelectionType::Object && sel>=0) {
            objects[sel].scale=glm::vec3(std::stof(args[1]),std::stof(args[2]),std::stof(args[3]));
            logInfo("Scaled: "+objects[sel].name);
        }
        else if (c=="rename" && args.size()>=3 && selType==SelectionType::Object && sel>=0) {
            std::string oldName = objects[sel].name;
            objects[sel].name = args[1];
            logInfo("Renamed: "+oldName+" -> "+args[1]);
        }
        else if (c=="color" && args.size()>=4 && selType==SelectionType::Object && sel>=0) {
            objects[sel].color={std::stof(args[1]),std::stof(args[2]),std::stof(args[3])};
            logInfo("Color changed: "+objects[sel].name);
        }
        else if (c=="delete" && args.size()>=2) {
            bool done=false;
            for (int i=0;i<(int)objects.size() && !done;i++) if (objects[i].name==args[1]) { logInfo("Deleted: "+objects[i].name); if(sel==i){sel=-1;selType=SelectionType::None;} objects.erase(objects.begin()+i); done=true; selUI=-1; selSprite2D=-1; }
            for (int i=0;i<(int)lights.size() && !done;i++) if (lights[i].name==args[1]) { logInfo("Deleted: "+lights[i].name); if(selLight==i)selLight=-1; lights.erase(lights.begin()+i); done=true; }
            for (int i=0;i<(int)sceneCameras.size() && !done;i++) if (sceneCameras[i].name==args[1]) { logInfo("Deleted: "+sceneCameras[i].name); if(selCamera==i)selCamera=-1; sceneCameras.erase(sceneCameras.begin()+i); done=true; }
            if(!done) logInfo("Not found: "+args[1]);
        }
        else if (c=="spawn" && args.size()>=2) {
            PrimitiveType pt=PrimitiveType::Cube;
            if(args[1]=="sphere")pt=PrimitiveType::Sphere;
            else if(args[1]=="plane")pt=PrimitiveType::Plane;
            else if(args[1]=="cylinder")pt=PrimitiveType::Cylinder;
            else if(args[1]=="capsule")pt=PrimitiveType::Capsule;
            else if(args[1]=="pyramid")pt=PrimitiveType::Pyramid;
            SceneObject o; o.name=args.size()>=3?args[2]:(args[1]+"_"+std::to_string(objects.size()+1));
            o.type=pt; o.pos={0,0.5f,0}; o.color={0.8f,0.6f,0.3f};
            o.ecsID=scene.CreateEntity(o.name);
            scene.GetTransform(o.ecsID).Position=o.pos;
            scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
            objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
            logInfo("Spawned: "+o.name);
        }
        else if (c=="volume" && args.size()>=2) { VE::AudioEngine::Get().SetMasterVolume(std::stof(args[1])); logInfo("Volume: "+args[1]); }
        else if (c=="play" && args.size()>=2) { VE::AudioEngine::Get().PlaySound(args[1]); logInfo("Playing: "+args[1]); }
        else if (c=="time" && args.size()>=2) { g_TimeOfDay=std::stof(args[1]); logInfo("Time of day: "+args[1]); }
        else if (c=="sun" && args.size()>=2) { g_SunIntensity=std::stof(args[1]); logInfo("Sun intensity: "+args[1]); }
        else if (c=="ambient" && args.size()>=2) { g_AmbientStrength=std::stof(args[1]); logInfo("Ambient: "+args[1]); }
        // ── Graphics console commands: fog / bloom / exposure / skybox / grid ──
        else if (c=="fog" && args.size()>=2) { extern float g_FogDensity; extern glm::vec3 g_FogColor; g_FogDensity=std::stof(args[1]); if(args.size()>=5){ g_FogColor.x=std::stof(args[2]); g_FogColor.y=std::stof(args[3]); g_FogColor.z=std::stof(args[4]); } logInfo("Fog: density "+args[1]); }
        else if (c=="bloom" && args.size()>=2) { if(args[1]=="on"){g_BloomEnabled=true; logInfo("Bloom: ON");} else if(args[1]=="off"){g_BloomEnabled=false; logInfo("Bloom: OFF");} else { g_BloomThreshold=std::stof(args[1]); if(args.size()>=3) g_BloomStrength=std::stof(args[2]); logInfo("Bloom: threshold "+args[1]); } }
        else if (c=="exposure" && args.size()>=2) { g_Exposure=std::stof(args[1]); logInfo("Exposure: "+args[1]); }
        else if (c=="skybox" && args.size()>=2) { showSkybox=(args[1]=="on"||args[1]=="1"); logInfo(std::string("Skybox: ")+(showSkybox?"ON":"OFF")); }
        else if (c=="grid" && args.size()>=2) { showGrid=(args[1]=="on"||args[1]=="1"); logInfo(std::string("Grid: ")+(showGrid?"ON":"OFF")); }
        else if (c=="scene" && args.size()>=2 && args[1]=="save") {
            if(currentScenePath.empty()) currentScenePath=projectRoot+"\\Assets\\Scenes\\scene.vescene";
            SaveScene(currentScenePath,objects,lights,sceneCameras);
            logInfo("Scene saved: "+currentScenePath);
        }
        else if (c=="scene" && args.size()>=3 && args[1]=="load") { VE::SceneManager::Get().RequestLoad(args[2]); logInfo("Loading scene: "+args[2]); }
        else if (c=="scene" && args.size()>=2 && args[1]=="reload") { VE::SceneManager::Get().RequestReload(); logInfo("Reloading scene..."); }
        else if (c=="scene" && args.size()>=2 && args[1]=="current") { logInfo("Current: "+VE::SceneManager::Get().GetCurrent()); }
        else logWarn("Unknown command: "+c+" (type 'help')");
    }
    ImGui::EndTabItem();
}
 if (ImGui::BeginTabItem("  Project")) {
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
        static bool s_wantCreateScript=false;
        static bool s_wantProjCtx=false;

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
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f,0.11f,0.12f,1.f));
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
                    enum class IconType { Folder, Image, Script, Mesh3D, Scene, Audio, Save, MaterialIcon, Prefab, Generic };
                    IconType iconType;
                    if      (isDir)                                                             iconType = IconType::Folder;
                    else if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".bmp"||ext==".tga") iconType = IconType::Image;
                    else if (ext==".lua")                                                       iconType = IconType::Script;
                    else if (ext==".obj"||ext==".fbx"||ext==".gltf"||ext==".glb")             iconType = IconType::Mesh3D;
                    else if (ext==".vescene")                                                   iconType = IconType::Scene;
                    else if (ext==".wav"||ext==".mp3"||ext==".ogg"||ext==".flac")             iconType = IconType::Audio;
                    else if (ext==".vesave")                                                    iconType = IconType::Save;
                    else if (ext==".mat")                                                       iconType = IconType::MaterialIcon;
                    else if (ext==".veprefab")                                                   iconType = IconType::Prefab;
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
                    else if (iconType == IconType::Prefab) {
                        // ил-звезда (как значок префаба в Unity) — сразу отличается от обычной модели
                        float R = S*.30f;
                        ImVec2 star[10];
                        for (int i=0;i<10;i++) {
                            float ang = -3.14159265f/2.f + i*3.14159265f/5.f;
                            float rr = (i%2==0) ? R : R*0.45f;
                            star[i] = ImVec2(cx + cosf(ang)*rr, cy + sinf(ang)*rr);
                        }
                        dl->AddConvexPolyFilled(star, 10, IM_COL32(40,150,140,255));
                        dl->AddPolyline(star, 10, IM_COL32(90,230,210,230), ImDrawFlags_Closed, 1.5f);
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
                        static std::unordered_map<std::string, ImU32> matIconCache;
                        auto itC = matIconCache.find(e.path().string());
                        ImU32 sphereCol;
                        if (itC != matIconCache.end()) { sphereCol = itC->second; }
                        else { Material pm = LoadMaterial(e.path().string()); sphereCol = IM_COL32((int)(pm.color.r*255),(int)(pm.color.g*255),(int)(pm.color.b*255),255); matIconCache[e.path().string()] = sphereCol; }
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
                    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) { assetSelected = e.path().string(); s_wantProjCtx=true; }
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

                if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".bmp"||ext==".tga"||ext==".gif") {
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        std::string dragPath = e.path().string();
                        ImGui::SetDragDropPayload("TEXTURE_PATH", dragPath.c_str(), dragPath.size()+1, ImGuiCond_Once);
                        ImGui::TextColored(ImVec4(0.6f,0.8f,1.f,1.f), "Texture: %s", name.c_str());
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
                                    int target = (g_MatPickTarget!=0) ? g_MatPickTarget : 1; // без явного выбора — по умолчанию база
                                    if (target==2) {
                                        tmat.layer2TexturePath = e.path().string(); tmat.layer2TextureID = tid;
                                        logInfo("Layer 2 -> "+tobj.name+" ["+tmat.name+"]");
                                    } else if (target==3) {
                                        tmat.maskTexturePath = e.path().string(); tmat.maskTextureID = tid;
                                        logInfo("Mask -> "+tobj.name+" ["+tmat.name+"]");
                                    } else {
                                        tmat.texturePath = e.path().string(); tmat.textureID = tid;
                                        if (tobj.activeMaterial==0) { tobj.texturePath=tmat.texturePath; tobj.textureID=tid; }
                                        logInfo("Texture -> "+tobj.name+" ["+tmat.name+"]");
                                    }
                                    g_MatPickTarget = 0; // режим выбора сбрасывается сразу после назначения
                                }
                        } else if (selUI>=0 && selUI<(int)uiElements.size()) { GLuint tid=VE::LoadTextureRaw(e.path().string()); if(tid){ uiElements[selUI].tex=tid; uiElements[selUI].texPath=e.path().string(); logInfo("Texture -> "+uiElements[selUI].name); } } else logInfo("Select object or UI first");
                        } else if (ext==".obj"||ext==".fbx"||ext==".gltf"||ext==".glb") {
                            SceneObject o; o.name=e.path().stem().string(); o.type=PrimitiveType::Model3D;
                            o.modelPath=e.path().string(); o.model=std::make_shared<VE::Model>(); o.model->Load(o.modelPath);
                            o.color=glm::vec3(0.8f,0.8f,0.8f); o.ecsID=scene.CreateEntity(o.name);
                            scene.GetTransform(o.ecsID).Position=o.pos;
                            scene.registry.AddComponent<VE::MeshComponent>(o.ecsID,VE::Mesh{},o.color);
                            objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                            logInfo("Model: "+o.name);
                        } else if (ext==".veprefab") {
                            SceneObject o;
                            PrefabColliderInfo colInfo;
                            if (LoadPrefab(e.path().string(), o, colInfo)) {
                                static int s_PrefabDropCounter=0;
                                o.name = o.name + "_" + std::to_string(++s_PrefabDropCounter);
                                o.ecsID = scene.CreateEntity(o.name);
                                scene.GetTransform(o.ecsID).Position = o.pos;
                                scene.GetTransform(o.ecsID).Scale = o.scale;
                                scene.registry.AddComponent<VE::MeshComponent>(o.ecsID, VE::Mesh{}, o.color);
                                if (o.hasRigidBody) {
                                    auto& rb = scene.registry.AddComponent<VE::RigidbodyComponent>(o.ecsID);
                                    rb.Mass = o.mass; rb.UseGravity = o.useGravity;
                                }
                                if (colInfo.hasCollider) {
                                    VE::ColliderComponent col;
                                    col.Shape = (VE::ColliderComponent::ShapeType)colInfo.shape;
                                    col.HalfSize = {colInfo.hx, colInfo.hy, colInfo.hz};
                                    col.Radius = colInfo.radius; col.Height = colInfo.height;
                                    col.IsTrigger = colInfo.isTrigger;
                                    scene.registry.AddComponent<VE::ColliderComponent>(o.ecsID) = col;
                                }
                                objects.push_back(o); sel=(int)objects.size()-1; selType=SelectionType::Object;
                                logInfo("Prefab: "+o.name);
                            }
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
        if ((ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) || s_wantProjCtx) { ImGui::OpenPopup("##projctx"); s_wantProjCtx=false; }
        if (ImGui::BeginPopup("##projctx")) {
            ImGui::TextColored(ImVec4(0.45f,0.45f,0.50f,1.f), "  Create"); ImGui::Separator();
            if (ImGui::MenuItem("  Lua Script")) {
                s_wantCreateScript = true;
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
        // -- Create Script popup (��� � Unity: �������� ���) --
        if (s_wantCreateScript) { ImGui::OpenPopup("##create_script"); s_wantCreateScript=false; }
        if (ImGui::BeginPopupModal("##create_script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char scriptNameBuf[128] = "NewScript";
            ImGui::Text("Script name:");
            ImGui::SetNextItemWidth(250);
            bool enter = ImGui::InputText("##script_name", scriptNameBuf, sizeof(scriptNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            if (enter || ImGui::Button("Create", ImVec2(120,0))) {
                if (scriptNameBuf[0]) {
                    std::string name = scriptNameBuf;
                    if (name.find(".lua")==std::string::npos) name += ".lua";
                    std::string sp = assetCurrentPath + "\\" + name;
                    std::ofstream f(sp); f << "-- " << name << "\nfunction onStart()\nend\nfunction onUpdate(dt)\nend\n"; f.close();
                    logInfo("Created: "+name);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120,0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
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
                    if (oldP.extension()==".lua" && newP.extension()!=".lua") {
                        newP.replace_extension(".lua");
                    }
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

                    // етка по секундам + подписи
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
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f,0.15f,0.18f,1.f));
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
    // ---------------------------------------------------------══════════════════════════════════════════════════════
    //   PLAYER MODE — полноэкранный вид игры, без редактора
    // ---------------------------------------------------------══════════════════════════════════════════════════════
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
    void RenderMaterialEditor();
    void VE_SetMaterialBridge(std::vector<SceneObject>*, int*, SelectionType*);
    VE_SetMaterialBridge(&objects, &sel, &selType);
    RenderMaterialEditor();
    extern void RenderShaderEditor();
    RenderShaderEditor();
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0,0,(int)ImGui::GetIO().DisplaySize.x,(int)ImGui::GetIO().DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    g_RawMouseDX=0; g_RawMouseDY=0;
    window->OnUpdate();
} // end while

g_Prefs.Save();
ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();
VE::AudioEngine::Get().Shutdown();
shader.Delete();outlineShader.Delete();gridShader.Delete();gizmoShader.Delete();skyboxShader.Delete();
glDeleteFramebuffers(1,&sceneFBO);glDeleteFramebuffers(1,&gameFBO);
glDeleteFramebuffers(1,&sceneMSFBO);glDeleteFramebuffers(1,&gameMSFBO);
glDeleteFramebuffers(1,&sceneHDRFBO);glDeleteFramebuffers(1,&gameHDRFBO);
glDeleteTextures(1,&sceneHDRTex);glDeleteTextures(1,&gameHDRTex);
glDeleteFramebuffers(1,&brightFBO);glDeleteTextures(1,&brightTex);
glDeleteFramebuffers(2,pingpongFBO);glDeleteTextures(2,pingpongTex);
glDeleteVertexArrays(1,&quadVAO);glDeleteBuffers(1,&quadVBO);
bloomBrightShader.Delete();bloomBlurShader.Delete();bloomCompositeShader.Delete();
glDeleteRenderbuffers(1,&sceneMSColorRBO);glDeleteRenderbuffers(1,&sceneMSDepthRBO);
glDeleteRenderbuffers(1,&gameMSColorRBO);glDeleteRenderbuffers(1,&gameMSDepthRBO);
delete window;
return 0;



// ---- Material Editor bridge ----
static std::vector<SceneObject>* g_veObjects = nullptr;
static int* g_veSel = nullptr;
static SelectionType* g_veSelType = nullptr;
void VE_SetMaterialBridge(std::vector<SceneObject>* o, int* s, SelectionType* st) { g_veObjects = o; g_veSel = s; g_veSelType = st; }
void VE_ApplyMaterialToSelected(float r, float g, float b, float metallic, float roughness) {
    if (g_veObjects && g_veSel && g_veSelType && *g_veSelType == SelectionType::Object && *g_veSel >= 0 && *g_veSel < (int)g_veObjects->size()) {
        (*g_veObjects)[*g_veSel].color = glm::vec3(r, g, b);
        logInfo("Material applied to " + (*g_veObjects)[*g_veSel].name);
    }
}









