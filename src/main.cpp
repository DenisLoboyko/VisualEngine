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


// ── Модули движка (вынесены из main.cpp для читаемости) ──
#include "Shaders.h"
#include "SceneTypes.h"
#include "Material.h"
#include "Animation.h"
#include "SceneObject.h"
#include "EditorGlobals.h"
#include "InputCallbacks.h"
#include "SceneRenderer.h"
#include "SceneIO.h"
#include "PrefabIO.h"

// ════════════════════════════════════════════════════════════════
// Функция для динамического ресайза Viewport FBO
// Вызывается когда размер ImGui Viewport изменился
// ════════════════════════════════════════════════════════════════
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

    logInfo("ResizeViewportFBO: " + std::to_string(newWidth) + "x" + std::to_string(newHeight));

    const int MSAA_SAMPLES = 4;

    // ══ Удаляем старые MSAA FBO ══
    glDeleteFramebuffers(1, &sceneMSFBO);
    glDeleteRenderbuffers(1, &sceneMSColorRBO);
    glDeleteRenderbuffers(1, &sceneMSDepthRBO);
    glDeleteFramebuffers(1, &gameMSFBO);
    glDeleteRenderbuffers(1, &gameMSColorRBO);
    glDeleteRenderbuffers(1, &gameMSDepthRBO);

    // ══ Удаляем старые Resolve FBO ══
    glDeleteFramebuffers(1, &sceneFBO);
    glDeleteTextures(1, &sceneTex);
    glDeleteRenderbuffers(1, &sceneRBO);
    glDeleteFramebuffers(1, &gameFBO);
    glDeleteTextures(1, &gameTex);
    glDeleteRenderbuffers(1, &gameRBO);

    // ══ Удаляем старые HDR FBO (если есть) ══
    glDeleteFramebuffers(1, &sceneHDRFBO);
    glDeleteTextures(1, &sceneHDRTex);
    glDeleteFramebuffers(1, &gameHDRFBO);
    glDeleteTextures(1, &gameHDRTex);

    // ════════════════════════════════════════════════════════════════
    // Пересоздаём Scene MSAA FBO
    // ══════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════
    // Пересоздаём Game MSAA FBO
    // ══════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════
    // Пересоздаём Scene Resolve FBO (MSAA → single-sampled)
    // ══════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════
    // Пересоздаём Game Resolve FBO (MSAA → single-sampled)
    // ══════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════
    // Пересоздаём HDR FBO для Scene
    // ══════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════
    // Пересоздаём HDR FBO для Game
    // ══════════════════════════════════════════════════════════════════
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

    logInfo("ViewportFBO resize complete");
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

    // ── Шрифт: дефолтный шрифт ImGui поддерживает только ASCII (0x20-0xFF),
    //    из-за чего кириллица и даже длинное тире "—" рисовались как "?".
    //    Грузим системный Segoe UI (есть на любом Windows) с расширенным
    //    диапазоном символов: латиница + кириллица + типографские тире/кавычки. ──
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        static const ImWchar extraChars[] = { 0x2010,0x2015, 0x2018,0x201F, 0x2026,0x2026, 0 }; // тире, кавычки-ёлочки, многоточие
        builder.AddRanges(extraChars);
        static ImVector<ImWchar> ranges;
        builder.BuildRanges(&ranges);

        const char* fontCandidates[] = {
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
        };
        bool fontLoaded = false;
        for (auto* path : fontCandidates) {
            if (fs::exists(path)) {
                io.Fonts->AddFontFromFileTTF(path, 17.0f, nullptr, ranges.Data);
                fontLoaded = true;
                break;
            }
        }
        if (!fontLoaded) io.Fonts->AddFontDefault(); // на случай если система совсем без стандартных шрифтов
    }

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

    // ── MSAA: рендерим в мультисэмпл-буфер, потом resolve-блитом переносим в уже
    //    существующие sceneTex/gameTex — их саму текстуру и ImGui::Image трогать не пришлось ──
    const int MSAA_SAMPLES = 4;
    unsigned int sceneMSFBO,sceneMSColorRBO,sceneMSDepthRBO;
    glGenFramebuffers(1,&sceneMSFBO);glBindFramebuffer(GL_FRAMEBUFFER,sceneMSFBO);
    glGenRenderbuffers(1,&sceneMSColorRBO);glBindRenderbuffer(GL_RENDERBUFFER,sceneMSColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,MSAA_SAMPLES,GL_RGBA16F,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,sceneMSColorRBO);
    glGenRenderbuffers(1,&sceneMSDepthRBO);glBindRenderbuffer(GL_RENDERBUFFER,sceneMSDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,MSAA_SAMPLES,GL_DEPTH24_STENCIL8,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,sceneMSDepthRBO);
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    unsigned int gameMSFBO,gameMSColorRBO,gameMSDepthRBO;
    glGenFramebuffers(1,&gameMSFBO);glBindFramebuffer(GL_FRAMEBUFFER,gameMSFBO);
    glGenRenderbuffers(1,&gameMSColorRBO);glBindRenderbuffer(GL_RENDERBUFFER,gameMSColorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,MSAA_SAMPLES,GL_RGBA16F,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,gameMSColorRBO);
    glGenRenderbuffers(1,&gameMSDepthRBO);glBindRenderbuffer(GL_RENDERBUFFER,gameMSDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,MSAA_SAMPLES,GL_DEPTH24_STENCIL8,3840,2160);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,gameMSDepthRBO);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glEnable(GL_MULTISAMPLE);

    // ── Bloom: HDR-резолв (float, без tonemap) + буферы под bright-pass/blur ──
    auto makeHDRFBO = [](unsigned int& fbo, unsigned int& tex, int w, int h){
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    };
    unsigned int sceneHDRFBO,sceneHDRTex; makeHDRFBO(sceneHDRFBO,sceneHDRTex,3840,2160);
    unsigned int gameHDRFBO,gameHDRTex;   makeHDRFBO(gameHDRFBO,gameHDRTex,3840,2160);
    // bright-pass/blur — половинное разрешение, этого достаточно для мягкого свечения и заметно быстрее
    unsigned int brightFBO,brightTex; makeHDRFBO(brightFBO,brightTex,1920,1080);
    unsigned int pingpongFBO[2],pingpongTex[2];
    for(int i=0;i<2;i++) makeHDRFBO(pingpongFBO[i],pingpongTex[i],1920,1080);

    // fullscreen-quad для всех пост-процесс проходов
    unsigned int quadVAO,quadVBO;
    {
        float qv[] = { -1,1,0,1,  -1,-1,0,0,  1,-1,1,0,   -1,1,0,1,  1,-1,1,0,  1,1,1,1 };
        glGenVertexArrays(1,&quadVAO); glGenBuffers(1,&quadVBO);
        glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER,quadVBO);
        glBufferData(GL_ARRAY_BUFFER,sizeof(qv),qv,GL_STATIC_DRAW);
        glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float))); glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    VE::Shader bloomBrightShader(postVertSrc,brightPassFragSrc);
    VE::Shader bloomBlurShader(postVertSrc,blurFragSrc);
    VE::Shader bloomCompositeShader(postVertSrc,compositeFragSrc);

    float g_BloomThreshold=1.0f, g_BloomStrength=0.6f, g_Exposure=1.0f;
    bool  g_BloomEnabled=true;

    // Резолвит HDR-сцену (hdrTex) в bright-pass -> размытие -> композит+tonemap,
    // финальный LDR-результат пишет в outputFBO (это sceneFBO/gameFBO — их сама текстура
    // для ImGui::Image не меняется, меняется только то, что в неё рисуется).
    auto ApplyBloomAndTonemap = [&](unsigned int hdrTex, unsigned int outputFBO, int w, int h){
        int bw=w/4, bh=h/4; if(bw<1)bw=1; if(bh<1)bh=1; // четверть разрешения — заметно дешевле, для bloom этого достаточно

        // Полноэкранные quad-проходы — 2D, без глубины/блендинга. Если оставить
        // GL_DEPTH_TEST включённым (он включён после 3D-рендера сцены), quad проваливает
        // тест глубины против неочищенного буфера глубины и просто не рисуется — чёрный экран.
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        // 1) bright-pass в половинном разрешении
        glBindFramebuffer(GL_FRAMEBUFFER,brightFBO); glViewport(0,0,bw,bh);
        bloomBrightShader.Use();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,hdrTex);
        glUniform1i(glGetUniformLocation(bloomBrightShader.ID,"uScene"),0);
        glUniform1f(glGetUniformLocation(bloomBrightShader.ID,"uThreshold"),g_BloomThreshold);
        glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES,0,6);

        // 2) гауссово размытие пинг-понгом (5 проходов туда-обратно = мягкое широкое свечение)
        bool horizontal=true; unsigned int srcTex=brightTex;
        bloomBlurShader.Use();
        glUniform2f(glGetUniformLocation(bloomBlurShader.ID,"uTexelSize"),1.0f/bw,1.0f/bh);
        for(int i=0;i<4;i++){
            glBindFramebuffer(GL_FRAMEBUFFER,pingpongFBO[horizontal?0:1]); glViewport(0,0,bw,bh);
            glUniform1i(glGetUniformLocation(bloomBlurShader.ID,"uHorizontal"),horizontal);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,srcTex);
            glUniform1i(glGetUniformLocation(bloomBlurShader.ID,"uImage"),0);
            glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES,0,6);
            srcTex=pingpongTex[horizontal?0:1];
            horizontal=!horizontal;
        }

        // 3) композит: HDR-сцена + размытый bloom -> ACES tonemap -> итоговый LDR в outputFBO
        glBindFramebuffer(GL_FRAMEBUFFER,outputFBO); glViewport(0,0,w,h);
        bloomCompositeShader.Use();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,hdrTex);
        glUniform1i(glGetUniformLocation(bloomCompositeShader.ID,"uScene"),0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,srcTex);
        glUniform1i(glGetUniformLocation(bloomCompositeShader.ID,"uBloom"),1);
        glUniform1f(glGetUniformLocation(bloomCompositeShader.ID,"uBloomStrength"),g_BloomEnabled?g_BloomStrength:0.f);
        glUniform1f(glGetUniformLocation(bloomCompositeShader.ID,"uExposure"),g_Exposure);
        glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES,0,6);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_DEPTH_TEST); // возвращаем состояние для следующего 3D-рендера
    };

    std::vector<SceneObject> objects;
    g_LuaObjectsPtr = &objects; // для Animation.* API из Lua
    std::vector<LightObject> lights;
    std::vector<CameraObject> sceneCameras;
    g_LuaLightsPtr  = &lights;      // для SaveScene из Lua
    g_LuaCamerasPtr = &sceneCameras; // для Scene.SetCamera из Lua
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
        VE::UndoSystem::Get().Clear(); // новая сцена — старая история отмены больше не валидна
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

    // ── SceneManager: регистрируем коллбэк сохранения сцены (для SceneManager.SaveScene / SaveScene из Lua) ──
    VE::SceneManager::Get().SetSaveCallback([&](const std::string& path){
        SaveScene(path, objects, lights, sceneCameras);
        currentScenePath = path;
        logInfo("Scene saved: "+path);
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

                // ── Physics.AddRigidbody(mass?, useGravity?) — включить физику для СВОЕГО объекта.
                //    Если у объекта ещё нет коллайдера — добавляет Box-коллайдер по размеру объекта
                //    (иначе тело будет падать, но ни с чем не сталкиваться — Physics.Step требует
                //    Rigidbody+Collider+Transform одновременно, см. Physics.h::SyncToBullet). ──
                pushSelfFn("AddRigidbody", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    float mass = (float)luaL_optnumber(L,1,1.0);
                    bool useGravity = lua_isboolean(L,2) ? (lua_toboolean(L,2)!=0) : true;

                    if(scene.registry.HasComponent<VE::RigidbodyComponent>(id)){
                        auto& rb = scene.registry.GetComponent<VE::RigidbodyComponent>(id);
                        rb.Mass = mass; rb.UseGravity = useGravity;
                    } else {
                        auto& rb = scene.registry.AddComponent<VE::RigidbodyComponent>(id);
                        rb.Mass = mass; rb.UseGravity = useGravity;
                    }

                    extern std::vector<SceneObject>* g_LuaObjectsPtr;
                    glm::vec3 half(0.5f,0.5f,0.5f);
                    if(g_LuaObjectsPtr) for(auto& o : *g_LuaObjectsPtr) if(o.ecsID==id){ half = o.scale*0.5f; o.hasRigidBody=true; break; }

                    if(!scene.registry.HasComponent<VE::ColliderComponent>(id))
                        scene.registry.AddComponent<VE::ColliderComponent>(id) = VE::ColliderComponent::Box(half);

                    return 0;
                });

                // ── Physics.AddCollider(shape, a, b, c, isTrigger?) — добавить/заменить форму коллизий.
                //    shape="box":     a,b,c = half-size по X,Y,Z (по умолчанию 0.5 каждый)
                //    shape="sphere":  a = radius
                //    shape="capsule": a = radius, b = height
                //    isTrigger — необязательный bool, последним аргументом (объекты проходят
                //    сквозь, но событие столкновения всё равно происходит). ──
                pushSelfFn("AddCollider", [](lua_State* L)->int{
                    VE::EntityID id=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    std::string shape = luaL_optstring(L,1,"box");
                    bool isTrigger = lua_isboolean(L,5) ? (lua_toboolean(L,5)!=0) : false;

                    VE::ColliderComponent col;
                    if(shape=="sphere"){
                        col = VE::ColliderComponent::Sphere((float)luaL_optnumber(L,2,0.5), isTrigger);
                    } else if(shape=="capsule"){
                        col = VE::ColliderComponent::Capsule((float)luaL_optnumber(L,2,0.5), (float)luaL_optnumber(L,3,1.0));
                        col.IsTrigger = isTrigger;
                    } else {
                        glm::vec3 half((float)luaL_optnumber(L,2,0.5),(float)luaL_optnumber(L,3,0.5),(float)luaL_optnumber(L,4,0.5));
                        col = VE::ColliderComponent::Box(half, isTrigger);
                    }

                    if(scene.registry.HasComponent<VE::ColliderComponent>(id))
                        scene.registry.GetComponent<VE::ColliderComponent>(id) = col;
                    else
                        scene.registry.AddComponent<VE::ColliderComponent>(id) = col;
                    return 0;
                });

                // ── Physics.Raycast(ox,oy,oz, dx,dy,dz, maxDist) — для выстрелов, проверки земли, клика по объекту.
                //    Возвращает: hit(bool), entity(int|nil), hitX,hitY,hitZ, normalX,normalY,normalZ, distance ──
                lua_pushstring(LL, "Raycast");
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    glm::vec3 origin((float)luaL_optnumber(L,1,0), (float)luaL_optnumber(L,2,0), (float)luaL_optnumber(L,3,0));
                    glm::vec3 dir((float)luaL_optnumber(L,4,0), (float)luaL_optnumber(L,5,0), (float)luaL_optnumber(L,6,0));
                    float maxDist = (float)luaL_optnumber(L,7,1000.0);
                    auto hit = VE::Physics::Get().Raycast(origin, dir, maxDist);
                    lua_pushboolean(L, hit.hit);
                    if (hit.hit) lua_pushinteger(L, (lua_Integer)hit.entity); else lua_pushnil(L);
                    lua_pushnumber(L, hit.point.x);  lua_pushnumber(L, hit.point.y);  lua_pushnumber(L, hit.point.z);
                    lua_pushnumber(L, hit.normal.x); lua_pushnumber(L, hit.normal.y); lua_pushnumber(L, hit.normal.z);
                    lua_pushnumber(L, hit.distance);
                    return 9;
                }, 0);
                lua_settable(LL, -3);

                // ── Physics.RaycastAll(ox,oy,oz, dx,dy,dz, maxDist?) — ВСЕ пересечения вдоль луча
                //    (не только ближайшее, как Raycast) — для дробовика/луча сквозь стекло и т.п.
                //    Возвращает Lua-массив таблиц: { {entity=.., x=.., y=.., z=.., nx=.., ny=.., nz=.., distance=..}, ... }
                //    отсортированный по расстоянию (ближайший первый). ──
                lua_pushstring(LL, "RaycastAll");
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    glm::vec3 origin((float)luaL_optnumber(L,1,0), (float)luaL_optnumber(L,2,0), (float)luaL_optnumber(L,3,0));
                    glm::vec3 dir((float)luaL_optnumber(L,4,0), (float)luaL_optnumber(L,5,0), (float)luaL_optnumber(L,6,0));
                    float maxDist = (float)luaL_optnumber(L,7,1000.0);
                    auto hits = VE::Physics::Get().RaycastAll(origin, dir, maxDist);

                    lua_newtable(L);
                    for(size_t i=0;i<hits.size();i++){
                        lua_newtable(L);
                        lua_pushinteger(L,(lua_Integer)hits[i].entity); lua_setfield(L,-2,"entity");
                        lua_pushnumber(L,hits[i].point.x);   lua_setfield(L,-2,"x");
                        lua_pushnumber(L,hits[i].point.y);   lua_setfield(L,-2,"y");
                        lua_pushnumber(L,hits[i].point.z);   lua_setfield(L,-2,"z");
                        lua_pushnumber(L,hits[i].normal.x);  lua_setfield(L,-2,"nx");
                        lua_pushnumber(L,hits[i].normal.y);  lua_setfield(L,-2,"ny");
                        lua_pushnumber(L,hits[i].normal.z);  lua_setfield(L,-2,"nz");
                        lua_pushnumber(L,hits[i].distance);  lua_setfield(L,-2,"distance");
                        lua_rawseti(L,-2,(int)(i+1)); // Lua-массивы с 1
                    }
                    return 1;
                }, 0);
                lua_settable(LL, -3);

                // ── Physics.CheckCollision(a, b) — соприкасаются ли Entity a и b прямо сейчас.
                //    Physics.CheckCollision(otherID) — короткая форма: проверить СВОЙ объект и otherID. ──
                pushSelfFn("CheckCollision", [](lua_State* L)->int{
                    VE::EntityID selfId=(VE::EntityID)lua_tointeger(L, lua_upvalueindex(1));
                    VE::EntityID a, b;
                    if(lua_gettop(L) >= 2){
                        a=(VE::EntityID)luaL_optinteger(L,1,0);
                        b=(VE::EntityID)luaL_optinteger(L,2,0);
                    } else {
                        a=selfId;
                        b=(VE::EntityID)luaL_optinteger(L,1,0);
                    }
                    lua_pushboolean(L, VE::Physics::Get().CheckCollision(a,b));
                    return 1;
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

                // ── Scene.Instantiate(templateName, x,y,z) -> newName | nil ──
                // Клонирует уже существующий объект (используй его как "префаб": разместил в сцене
                // один раз, дальше спавнишь копии по имени). Копирует меш/материалы/физику/скейл.
                // Свои Lua-скрипты клон НЕ запускает — им управляет скрипт, который его заспавнил
                // (так проще для пуль/врагов: один "менеджер" двигает и удаляет их по имени).
                lua_pushstring(LL, "Instantiate");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* tname=luaL_checkstring(L,1);
                    float x=(float)luaL_optnumber(L,2,0), y=(float)luaL_optnumber(L,3,0), z=(float)luaL_optnumber(L,4,0);
                    SceneObject* tmpl=nullptr;
                    for(auto& o:*objs) if(o.name==tname){ tmpl=&o; break; }
                    if(!tmpl){ lua_pushnil(L); return 1; }
                    static int s_InstCounter=0;
                    SceneObject clone = *tmpl;
                    clone.name = tmpl->name + "_Clone" + std::to_string(++s_InstCounter);
                    clone.pos = glm::vec3(x,y,z);
                    clone.luaInstances.clear();
                    clone.ecsID = scene.CreateEntity(clone.name);
                    scene.GetTransform(clone.ecsID).Position = clone.pos;
                    scene.GetTransform(clone.ecsID).Scale = clone.scale;
                    scene.registry.AddComponent<VE::MeshComponent>(clone.ecsID, VE::Mesh{}, clone.color);
                    if (tmpl->hasRigidBody) {
                        VE::RigidbodyComponent rb; rb.Mass=tmpl->mass; rb.UseGravity=tmpl->useGravity;
                        scene.registry.AddComponent<VE::RigidbodyComponent>(clone.ecsID, rb);
                        if (scene.registry.HasComponent<VE::ColliderComponent>(tmpl->ecsID))
                            scene.registry.AddComponent<VE::ColliderComponent>(clone.ecsID, scene.registry.GetComponent<VE::ColliderComponent>(tmpl->ecsID));
                        // Само тело Bullet создастся автоматически на следующем Physics::Step()
                        // (SyncToBullet сам находит все Entity с Rigidbody+Collider+Transform).
                    }
                    objs->push_back(clone);
                    lua_pushstring(L, clone.name.c_str());
                    return 1;
                }, 1);
                lua_settable(LL, -3);

                // ── Scene.InstantiatePrefab(path, x,y,z) -> newName | nil ──
                // Как Instantiate(), но грузит объект из .veprefab файла на диске —
                // работает даже если такого объекта нет в текущей сцене (в отличие
                // от Instantiate, который клонирует объект, уже стоящий в сцене).
                lua_pushstring(LL, "InstantiatePrefab");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* path=luaL_checkstring(L,1);
                    float x=(float)luaL_optnumber(L,2,0), y=(float)luaL_optnumber(L,3,0), z=(float)luaL_optnumber(L,4,0);

                    SceneObject o;
                    PrefabColliderInfo colInfo;
                    if (!LoadPrefab(path, o, colInfo)) { lua_pushnil(L); return 1; }

                    static int s_PrefabInstCounter=0;
                    o.name = o.name + "_Inst" + std::to_string(++s_PrefabInstCounter);
                    o.pos = glm::vec3(x,y,z);
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
                    objs->push_back(o);
                    lua_pushstring(L, o.name.c_str());
                    return 1;
                }, 1);
                lua_settable(LL, -3);

                // ── Scene.InstantiatePrimitive("Sphere", x,y,z, scale) -> newName ──
                // Быстрый спавн без заранее подготовленного шаблона (напр. пуля-заглушка сферой).
                lua_pushstring(LL, "InstantiatePrimitive");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    std::string tn = luaL_checkstring(L,1);
                    float x=(float)luaL_optnumber(L,2,0), y=(float)luaL_optnumber(L,3,0), z=(float)luaL_optnumber(L,4,0);
                    float sc=(float)luaL_optnumber(L,5,1.0);
                    PrimitiveType t = PrimitiveType::Cube;
                    if (tn=="Sphere") t=PrimitiveType::Sphere;
                    else if (tn=="Cylinder") t=PrimitiveType::Cylinder;
                    else if (tn=="Pyramid") t=PrimitiveType::Pyramid;
                    else if (tn=="Capsule") t=PrimitiveType::Capsule;
                    else if (tn=="Plane") t=PrimitiveType::Plane;
                    static int s_PrimCounter=0;
                    SceneObject o;
                    o.name = tn + "_Spawned" + std::to_string(++s_PrimCounter);
                    o.pos = glm::vec3(x,y,z);
                    o.scale = glm::vec3(sc,sc,sc);
                    o.type = t;
                    o.color = glm::vec3(0.7f,0.7f,0.75f);
                    o.ecsID = scene.CreateEntity(o.name);
                    scene.GetTransform(o.ecsID).Position = o.pos;
                    scene.GetTransform(o.ecsID).Scale = o.scale;
                    scene.registry.AddComponent<VE::MeshComponent>(o.ecsID, VE::Mesh{}, o.color);
                    objs->push_back(o);
                    lua_pushstring(L, o.name.c_str());
                    return 1;
                }, 1);
                lua_settable(LL, -3);

                // ── Scene.Destroy(name) -> bool ──
                // Удаляет объект из сцены прямо во время игры (пуля улетела/попала, враг умер и т.п.)
                lua_pushstring(LL, "Destroy");
                lua_pushlightuserdata(LL, (void*)&objects);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* objs=(std::vector<SceneObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* name=luaL_checkstring(L,1);
                    for(size_t i=0;i<objs->size();i++){
                        if((*objs)[i].name==name){
                            VE::EntityID id=(*objs)[i].ecsID;
                            VE::Physics::Get().RemoveBody(id);
                            if (scene.registry.IsAlive(id)) scene.registry.DestroyEntity(id);
                            objs->erase(objs->begin()+i);
                            lua_pushboolean(L,1);
                            return 1;
                        }
                    }
                    lua_pushboolean(L,0);
                    return 1;
                }, 1);
                lua_settable(LL, -3);

                // ── Scene.SetCamera(name) -> bool ──
                // Делает камеру с этим именем активной (isPrimary=true), остальные сцены-камеры
                // становятся неактивными — тот же флаг, что и переключение камеры в редакторе.
                lua_pushstring(LL, "SetCamera");
                lua_pushlightuserdata(LL, (void*)&sceneCameras);
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    auto* cams=(std::vector<CameraObject>*)lua_touserdata(L, lua_upvalueindex(1));
                    const char* name=luaL_checkstring(L,1);
                    bool found=false;
                    for(auto& c:*cams){
                        bool match=(c.name==name);
                        c.isPrimary=match;
                        if(match) found=true;
                    }
                    lua_pushboolean(L, found);
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
                lua_pushcclosure(LL, [](lua_State* L)->int{
                    extern float g_TimeOfDay;
                    float h=(float)luaL_optnumber(L,1,12.0);
                    while(h<0.f)h+=24.f; g_TimeOfDay=fmodf(h,24.f);
                    return 0;
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

    const char* typeNames[]={"Cube","Sphere","Cylinder","Pyramid","Capsule","Plane","Model","Empty"};
    static float hierW=240.f, inspW=280.f, bottomH=190.f;
    const float sideW=48.f, toolH=38.f;

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
        g_DragHoverObj = -1; // сброс каждый кадр, обновляется в drop target
        float menuH=20.f;
        float viewH=io.DisplaySize.y-menuH-bottomH-toolH;
        float vpW=io.DisplaySize.x-sideW-hierW-inspW;

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
                        if(selType==SelectionType::Object&&sel>=0){dragStartRot=objects[sel].rot;dragStartScale=objects[sel].scale;}
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
        
        } // if (g_EditorMode==EditorMode::Object)
        leftClickThisFrame=false;

        if(isPlaying&&!isPaused){
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

        glDisable(GL_SCISSOR_TEST); // ImGui мог оставить scissor включённым с маленьким прямоугольником прошлого кадра

        // ════════════════════════════════════════════════════════════════
        // Проверяем, изменился ли размер Viewport, и пересоздаём FBO если нужно
        // ════════════════════════════════════════════════════════════════
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

        glBindFramebuffer(GL_FRAMEBUFFER,sceneMSFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,sel,false,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,camera,vpAspect,gizmoMode,dragAxis,showSkybox,showGrid,showGizmos,gs,lights,sceneCameras,selLight,selCamera,selType);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,sceneMSFBO);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,sceneHDRFBO);
        glBlitFramebuffer(0,0,(int)g_VpSize.x,(int)g_VpSize.y,0,0,(int)g_VpSize.x,(int)g_VpSize.y,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        ApplyBloomAndTonemap(sceneHDRTex, sceneFBO, (int)g_VpSize.x, (int)g_VpSize.y);
        // ── Своё же тело не должно быть видно от первого лица (как в Unity/Godot) ──
        int fpExcludeIdx=-1;
        for(auto& sc:sceneCameras){
            if(sc.isPrimary && sc.followTargetIndex>=0 && sc.followTargetIndex<(int)objects.size()){
                fpExcludeIdx=sc.followTargetIndex; break;
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER,gameMSFBO);glViewport(0,0,(int)g_VpSize.x,(int)g_VpSize.y);
        renderScene(objects,-1,true,shader,skinnedShader,outlineShader,gridShader,gizmoShader,skyboxShader,skybox,grid,cubeVAO,sphere,cylinder,pyramid,capsule,plane,arrowVAO,arrowCnt,gameCamera,vpAspect,gizmoMode,dragAxis,showSkybox,false,false,gs,lights,sceneCameras,-1,-1,SelectionType::None,fpExcludeIdx);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,gameMSFBO);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,gameHDRFBO);
        glBlitFramebuffer(0,0,(int)g_VpSize.x,(int)g_VpSize.y,0,0,(int)g_VpSize.x,(int)g_VpSize.y,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        ApplyBloomAndTonemap(gameHDRTex, gameFBO, (int)g_VpSize.x, (int)g_VpSize.y);
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
                VE::UndoSystem::Get().Clear(); // новая сцена — старая история отмены больше не валидна
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
            VE::ParticleSystem::Get().Clear();
            VE::DebugDraw::Get().Clear();
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
ImGui::SameLine();
if (ToggleBtn("Bloom", g_BloomEnabled, ImVec2(58,24))) g_BloomEnabled=!g_BloomEnabled;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bloom + ACES tonemapping");

ImGui::SameLine(0,16);
ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.176f,0.188f,0.212f,1.f));
ImGui::Text("|");
ImGui::PopStyleColor();
ImGui::SameLine(0,16);

// ── Переключатель режима редактора (Object / Paint Mask) — как Mode в Blender ──
if (ToggleBtn("Object", g_EditorMode==EditorMode::Object, ImVec2(56,24))) g_EditorMode=EditorMode::Object;
if (ImGui::IsItemHovered()) ImGui::SetTooltip("Object Mode — normal select/move/rotate");
ImGui::SameLine(0,2);
bool canPaint = selType==SelectionType::Object && sel>=0 && sel<(int)objects.size()
    && !objects[sel].materials.empty() && objects[sel].materials[0].maskPixelSize>0;
if (!canPaint) ImGui::BeginDisabled();
if (ToggleBtn("Paint", g_EditorMode==EditorMode::PaintMask, ImVec2(50,24))) g_EditorMode=EditorMode::PaintMask;
if (!canPaint) ImGui::EndDisabled();
if (ImGui::IsItemHovered()) ImGui::SetTooltip(canPaint ? "Paint Mask Mode — LMB erases Layer2, Shift+LMB restores it" : "Create a Paintable Mask on the object's material first (Inspector tab)");
if (g_EditorMode==EditorMode::PaintMask) {
    ImGui::SameLine(0,10);
    if (ToggleBtn("Erase", !g_BrushPaintMode, ImVec2(50,24))) g_BrushPaintMode=false;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("LMB erases Layer 2 (shows base texture through)");
    ImGui::SameLine(0,2);
    if (ToggleBtn("Fill##brushmode", g_BrushPaintMode, ImVec2(50,24))) g_BrushPaintMode=true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("LMB restores Layer 2 (hides base texture again)");
    ImGui::SameLine(0,10);
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Brush", &g_BrushRadius, 0.02f, 0.4f, "%.2f");
}

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

        const char* icons[] = {"[#]","[o]","[|]","[^]","[*]","[-]","[M]","[+]"};
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
        // Теперь FBO динамически масштабируется под размер ImGui окна,
        // поэтому UV всегда (0,1)-(1,0) для полного отображения текстуры
        ImGui::Image((ImTextureID)(intptr_t)sceneTex, ImVec2(tw,th), ImVec2(0,1), ImVec2(1,0));

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
                    if(objects.empty()){sel=-1;selType=SelectionType::None;}
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
        float tw=ImGui::GetContentRegionAvail().x, th=ImGui::GetContentRegionAvail().y;
        if (isPlaying) {
            ImVec2 gamePos = ImGui::GetCursorScreenPos();
            // FBO теперь динамически масштабируется под размер ImGui окна
            ImGui::Image((ImTextureID)(intptr_t)gameTex, ImVec2(tw,th), ImVec2(0,1), ImVec2(1,0));
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
        strncpy_s(s_MatNameBuf, mat.name.c_str(), sizeof(s_MatNameBuf)-1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##matname", s_MatNameBuf, sizeof(s_MatNameBuf)))
            mat.name = s_MatNameBuf;

        ImGui::Text("Color:"); ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit3("##matcol", glm::value_ptr(mat.color))) {
            if (obj.activeMaterial == 0) obj.color = mat.color; // главный материал красит весь объект
        }

        ImGui::Text("Texture:"); ImGui::SameLine(80);
        if (mat.texturePath.empty()) ImGui::TextColored(COL_DIM, "(none)");
        else ImGui::TextColored(COL_GREEN, "%s", fs::path(mat.texturePath).filename().string().c_str());
        if (ImGui::Button(g_MatPickTarget==1 ? "Waiting for click..." : "Set Texture...", ImVec2(150,0)))
            g_MatPickTarget = (g_MatPickTarget==1) ? 0 : 1;
        if (!mat.texturePath.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Clear##tex")) { mat.texturePath.clear(); mat.textureID = 0; }
        }

        ImGui::Spacing();
        ImGui::TextColored(COL_DIM, "Layer 2 (e.g. concrete over bricks):");
        ImGui::Text("Layer 2:"); ImGui::SameLine(80);
        if (mat.layer2TexturePath.empty()) ImGui::TextColored(COL_DIM, "(none)");
        else ImGui::TextColored(COL_GREEN, "%s", fs::path(mat.layer2TexturePath).filename().string().c_str());
        if (ImGui::Button(g_MatPickTarget==2 ? "Waiting for click..." : "Set Layer 2...", ImVec2(150,0)))
            g_MatPickTarget = (g_MatPickTarget==2) ? 0 : 2;
        if (!mat.layer2TexturePath.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Clear##l2")) { mat.layer2TexturePath.clear(); mat.layer2TextureID = 0; }
            ImGui::Text("L2 Tiling X:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##l2tilex", &mat.layer2TilingX, 0.05f, 0.1f, 20.f, "%.2f");
            ImGui::Text("L2 Tiling Y:"); ImGui::SameLine(90); ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##l2tiley", &mat.layer2TilingY, 0.05f, 0.1f, 20.f, "%.2f");
        }

        ImGui::Spacing();
        ImGui::Text("Mask:"); ImGui::SameLine(80);
        if (mat.maskTexturePath.empty()) ImGui::TextColored(COL_DIM, "(none)");
        else ImGui::TextColored(COL_GREEN, "%s", fs::path(mat.maskTexturePath).filename().string().c_str());
        if (ImGui::Button(g_MatPickTarget==3 ? "Waiting for click..." : "Set Mask...", ImVec2(150,0)))
            g_MatPickTarget = (g_MatPickTarget==3) ? 0 : 3;
        if (!mat.maskTexturePath.empty()) {
            ImGui::SameLine();
            if (ImGui::Button("Clear##mask")) { mat.maskTexturePath.clear(); mat.maskTextureID = 0; }
        }

        if (g_MatPickTarget != 0) {
            const char* tgtName = g_MatPickTarget==1?"Texture":g_MatPickTarget==2?"Layer 2":"Mask";
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.55f,0.85f,0.95f,1.f), "Double-click an image in the Project panel below to assign it as %s.", tgtName);
            if (ImGui::Button("Cancel", ImVec2(-1,0))) g_MatPickTarget = 0;
        }

        if (!mat.layer2TexturePath.empty() && mat.maskTexturePath.empty() && mat.maskPixelSize==0)
            ImGui::TextColored(ImVec4(1.f,0.75f,0.3f,1.f), "Layer 2 is set but has no mask yet — set a Mask above, or create a paintable one below.");

        // ── Кисть: создать рисуемую маску и переключиться в Paint Mode ──
        if (!mat.layer2TexturePath.empty()) {
            ImGui::Spacing();
            if (mat.maskPixelSize==0) {
                if (ImGui::Button("Create Paintable Mask", ImVec2(-1,0))) {
                    CreateBlankMask(mat, 256);
                    UploadMaskTexture(mat);
                    mat.maskTexturePath.clear();
                    g_EditorMode = EditorMode::PaintMask;
                }
            } else {
                ImGui::TextColored(COL_GREEN, "Paintable mask: %dx%d", mat.maskPixelSize, mat.maskPixelSize);
                if (ImGui::Button(g_EditorMode==EditorMode::PaintMask ? "Stop Painting" : "Start Painting", ImVec2(-1,0)))
                    g_EditorMode = (g_EditorMode==EditorMode::PaintMask) ? EditorMode::Object : EditorMode::PaintMask;
                if (g_EditorMode==EditorMode::PaintMask)
                    ImGui::TextColored(COL_DIM, "LMB on object — erase Layer2, Shift+LMB — restore it");
                if (ImGui::Button("Save Mask to File...", ImVec2(-1,0))) {
                    std::string outPath = projectRoot + "\\Assets\\Textures\\" +
                        (mat.name.empty()?std::string("mask"):mat.name) + "_mask.pgm";
                    SaveMaskPGM(outPath, mat.maskPixels, mat.maskPixelSize);
                    mat.maskTexturePath = outPath;
                    logInfo("Mask saved: "+outPath);
                }
            }
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
    strncpy_s(s_AssetMatName, s_EditMat.name.c_str(), sizeof(s_AssetMatName)-1);
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
                        // Тил-звезда (как значок префаба в Unity) — сразу отличается от обычной модели
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
                            } else logInfo("Select object first");
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
    glDeleteFramebuffers(1,&sceneMSFBO);glDeleteFramebuffers(1,&gameMSFBO);
    glDeleteFramebuffers(1,&sceneHDRFBO);glDeleteFramebuffers(1,&gameHDRFBO);
    glDeleteTextures(1,&sceneHDRTex);glDeleteTextures(1,&gameHDRTex);
    glDeleteFramebuffers(1,&brightFBO);glDeleteTextures(1,&brightTex);
    glDeleteFramebuffers(2,pingpongFBO);glDeleteTextures(2,pingpongTex);
    glDeleteVertexArrays(1,&quadVAO);glDeleteBuffers(1,&quadVBO);
    bloomBrightShader.Delete();bloomBlurShader.Delete();bloomCompositeShader.Delete();
    glDeleteRenderbuffers(1,&sceneMSColorRBO);glDeleteRenderbuffers(1,&sceneMSDepthRBO);
    glDeleteRenderbuffers(1,&gameMSColorRBO);glDeleteRenderbuffers(1,&gameMSDepthRBO);
    delete window;return 0;
}