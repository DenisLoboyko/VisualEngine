#define _CRT_SECURE_NO_WARNINGS
// ShaderEditor.cpp — VisualEngine code-based shader editor.
// Devs write GLSL in .shader files ([vertex] / [fragment] sections),
// live-compile on a preview sphere, assign shaders to objects.
#include <imgui.h>
#include "imgui_internal.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <regex>

bool showShaderEditor = false;
bool g_shMinimized = false;
bool g_shaderEmbedded = false;

static std::vector<std::string> g_files;
static int g_cur = -1;
static char g_code[16384] = "";
static char g_log[4096] = "";
static GLuint g_prog = 0;
static GLuint g_fbo = 0, g_fboTex = 0, g_fboDepth = 0;
static GLuint g_sphVAO = 0, g_sphVBO = 0, g_sphEBO = 0;
static int g_sphIdx = 0;
static float g_yaw = 0.6f, g_pitch = 0.25f;
GLuint g_CustomSceneProgram = 0;
static bool g_liveApply = false;
static std::filesystem::file_time_type g_lastMtime{};
static std::string g_lastCompiled;
struct ShParam { std::string name; int type; float v[3]; };
static std::vector<ShParam> g_params;
static void ParseParams();

static const char* kTplToon =
"// Toon: cel-shading, 4 light bands\n"
"[vertex]\n"
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;\n"
"out vec3 N; out vec3 W;\n"
"void main(){ vec4 w=uModel*vec4(aPos,1.0); W=w.xyz; N=mat3(uModel)*aNormal; gl_Position=uProj*uView*w; }\n"
"[fragment]\n"
"#version 330 core\n"
"in vec3 N; in vec3 W;\n"
"uniform vec3 uCamPos; uniform vec3 uSunDir; uniform vec3 uSunColor;\n"
"uniform float uSunIntensity; uniform vec3 uAmbient; uniform vec3 uBaseColor; uniform float uTime;\n"
"out vec4 frag;\n"
"void main(){\n"
"  vec3 n=normalize(N);\n"
"  float d=dot(n,normalize(uSunDir));\n"
"  float band = d>0.6?1.0 : d>0.2?0.7 : d>-0.1?0.45 : 0.3;\n"
"  vec3 col = uBaseColor*(uAmbient + uSunColor*uSunIntensity*band);\n"
"  col = pow(col, vec3(0.4545));\n"
"  frag=vec4(col,1.0);\n"
"}\n";

static const char* kTplReal =
"// Realistic: GGX specular + fresnel + soft ambient\n"
"[vertex]\n"
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;\n"
"out vec3 N; out vec3 W;\n"
"void main(){ vec4 w=uModel*vec4(aPos,1.0); W=w.xyz; N=mat3(uModel)*aNormal; gl_Position=uProj*uView*w; }\n"
"[fragment]\n"
"#version 330 core\n"
"in vec3 N; in vec3 W;\n"
"uniform vec3 uCamPos; uniform vec3 uSunDir; uniform vec3 uSunColor;\n"
"uniform float uSunIntensity; uniform vec3 uAmbient; uniform vec3 uBaseColor; uniform float uTime;\n"
"out vec4 frag;\n"
"void main(){\n"
"  vec3 n=normalize(N); vec3 v=normalize(uCamPos-W); vec3 l=normalize(uSunDir); vec3 h=normalize(v+l);\n"
"  float NdL=max(dot(n,l),0.0); float NdH=max(dot(n,h),0.0); float VdH=max(dot(v,h),0.0);\n"
"  float rough=0.35; float a=rough*rough; float a2=a*a;\n"
"  float d=NdH*NdH*(a2-1.0)+1.0; float D=a2/(3.14159*d*d);\n"
"  float F=0.04+0.96*pow(1.0-VdH,5.0);\n"
"  vec3 diff=uBaseColor*NdL;\n"
"  vec3 spec=vec3(D*F*0.25)*NdL;\n"
"  vec3 col=diff*uSunColor*uSunIntensity + spec*uSunIntensity + uBaseColor*uAmbient;\n"
"  col=clamp((col*(2.51*col+0.03))/(col*(2.43*col+0.59)+0.14),0.0,1.0);\n"
"  col=pow(col,vec3(0.4545));\n"
"  frag=vec4(col,1.0);\n"
"}\n";

static const char* kTplUnlit =
"// Unlit: flat color, no lighting\n"
"[vertex]\n"
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;\n"
"void main(){ gl_Position=uProj*uView*uModel*vec4(aPos,1.0); }\n"
"[fragment]\n"
"#version 330 core\n"
"uniform vec3 uBaseColor; uniform float uTime;\n"
"out vec4 frag;\n"
"void main(){ frag=vec4(uBaseColor,1.0); }\n";

static std::string ShadersDir() {
    namespace fs = std::filesystem;
    const char* roots[] = { ".", "project", "..", "../.." };
    for (auto r : roots) if (fs::exists(fs::path(r) / "Assets")) return (fs::path(r) / "Assets" / "Shaders").string();
    return "Assets/Shaders";
}

static void EnsureTemplates() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(ShadersDir(), ec);
    auto put = [](const char* n, const char* c) {
        fs::path p = fs::path(ShadersDir()) / n;
        if (!fs::exists(p)) { std::ofstream o(p); o << c; }
    };
    put("toon.shader", kTplToon);
    put("realistic.shader", kTplReal);
    put("unlit.shader", kTplUnlit);
}

static void Scan() {
    namespace fs = std::filesystem;
    g_files.clear();
    std::error_code ec;
    for (auto& e : fs::directory_iterator(ShadersDir(), ec))
        if (e.path().extension() == ".shader") g_files.push_back(e.path().string());
    std::sort(g_files.begin(), g_files.end());
}

static void Load(int i) {
    if (i < 0 || i >= (int)g_files.size()) return;
    g_cur = i;
    std::ifstream f(g_files[i]);
    std::stringstream ss; ss << f.rdbuf();
    strncpy(g_code, ss.str().c_str(), sizeof(g_code) - 1);
    g_code[sizeof(g_code) - 1] = 0;
}

static GLuint CompileSh(GLenum t, const char* s, std::string& log) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok = 0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char buf[2048]; glGetShaderInfoLog(sh, 2048, nullptr, buf); log += buf; log += "\n"; }
    return sh;
}

static bool Build(const std::string& src, GLuint& outProg, std::string& log) {
    size_t vpos = src.find("[vertex]");
    size_t fpos = src.find("[fragment]");
    if (vpos == std::string::npos || fpos == std::string::npos) { log = "Missing [vertex] or [fragment] section"; return false; }
    std::string vs = src.substr(vpos + 8, fpos - vpos - 8);
    std::string fs = src.substr(fpos + 10);
    log = "";
    GLuint v = CompileSh(GL_VERTEX_SHADER, vs.c_str(), log);
    GLuint f = CompileSh(GL_FRAGMENT_SHADER, fs.c_str(), log);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char buf[2048]; glGetProgramInfoLog(p, 2048, nullptr, buf); log += buf; }
    glDeleteShader(v); glDeleteShader(f);
    if (!ok) { glDeleteProgram(p); return false; }
    outProg = p;
    return true;
}

static void DoCompile() {
    std::string log;
    GLuint p = 0;
    if (Build(g_code, p, log)) {
        if (g_prog) glDeleteProgram(g_prog);
        g_prog = p;
        snprintf(g_log, sizeof(g_log), "Compiled OK");
        ParseParams(); if (g_cur >= 0) { std::ofstream af(g_files[g_cur]); af << g_code; }
    } else {
        snprintf(g_log, sizeof(g_log), "ERRORS:\n%s", log.c_str());
    }
}

static void SaveFile() {
    if (g_cur < 0) return;
    std::ofstream f(g_files[g_cur]);
    f << g_code;
    snprintf(g_log, sizeof(g_log), "Saved: %s", g_files[g_cur].c_str());
}

static void BuildSphere() {
    const int lat = 32, lon = 32;
    std::vector<float> v; std::vector<unsigned int> ix;
    for (int i = 0; i <= lat; i++) {
        float th = i * 3.14159265f / lat;
        for (int j = 0; j <= lon; j++) {
            float ph = j * 2.0f * 3.14159265f / lon;
            float x = sinf(th)*cosf(ph), y = cosf(th), z = sinf(th)*sinf(ph);
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(x); v.push_back(y); v.push_back(z);
        }
    }
    for (int i = 0; i < lat; i++) for (int j = 0; j < lon; j++) {
        unsigned int a = i*(lon+1)+j, b = a+lon+1;
        ix.push_back(a); ix.push_back(b); ix.push_back(a+1);
        ix.push_back(a+1); ix.push_back(b); ix.push_back(b+1);
    }
    g_sphIdx = (int)ix.size();
    glGenVertexArrays(1, &g_sphVAO); glGenBuffers(1, &g_sphVBO); glGenBuffers(1, &g_sphEBO);
    glBindVertexArray(g_sphVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_sphVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size()*sizeof(float)), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sphEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(ix.size()*sizeof(unsigned int)), ix.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static const char* kEngineUniforms[] = { "uProj","uView","uModel","uCamPos","uSunDir","uSunColor","uSunIntensity","uAmbient","uBaseColor","uTime","lightSpaceMatrix","shadowMap","uTexture","useTexture" };
static bool IsEngineUniform(const std::string& n){ for(auto e:kEngineUniforms) if(n==e) return true; return false; }
static void ParseParams(){
    g_params.clear();
    std::string src = g_code;
    std::regex re("uniform\\s+(float|vec3)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*;");
    for (auto it = std::sregex_iterator(src.begin(), src.end(), re); it != std::sregex_iterator(); ++it) {
        std::string tn = (*it)[1].str(), nm = (*it)[2].str();
        if (IsEngineUniform(nm)) continue;
        ShParam p; p.name = nm; p.type = (tn=="float")?1:3; p.v[0]=p.v[1]=p.v[2]=0.5f;
        g_params.push_back(p);
    }
}
void VE_ShaderEditorApplyParams(GLuint p){
    for (auto& pr : g_params) {
        GLint l = glGetUniformLocation(p, pr.name.c_str());
        if (l < 0) continue;
        if (pr.type==1) glUniform1f(l, pr.v[0]); else glUniform3f(l, pr.v[0], pr.v[1], pr.v[2]);
    }
}
static void SetUniforms(GLuint p) {
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0), glm::vec3(0,1,0));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), g_yaw, glm::vec3(0,1,0)) * glm::rotate(glm::mat4(1.0f), g_pitch, glm::vec3(1,0,0));
    GLint l;
    l = glGetUniformLocation(p, "uProj");  if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, &proj[0][0]);
    l = glGetUniformLocation(p, "uView");  if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, &view[0][0]);
    l = glGetUniformLocation(p, "uModel"); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, &model[0][0]);
    l = glGetUniformLocation(p, "uCamPos"); if (l >= 0) glUniform3f(l, 0, 0, 3);
    l = glGetUniformLocation(p, "uSunDir"); if (l >= 0) glUniform3f(l, 0.5f, 0.8f, 0.45f);
    l = glGetUniformLocation(p, "uSunColor"); if (l >= 0) glUniform3f(l, 1.0f, 0.95f, 0.85f);
    l = glGetUniformLocation(p, "uSunIntensity"); if (l >= 0) glUniform1f(l, 1.2f);
    l = glGetUniformLocation(p, "uAmbient"); if (l >= 0) glUniform3f(l, 0.15f, 0.18f, 0.22f);
    l = glGetUniformLocation(p, "uBaseColor"); if (l >= 0) glUniform3f(l, 0.85f, 0.25f, 0.2f);
    l = glGetUniformLocation(p, "uTime"); if (l >= 0) glUniform1f(l, (float)ImGui::GetTime());
    VE_ShaderEditorApplyParams(p);
}

static void RenderPreview() {
    if (!g_sphVAO) BuildSphere();
    if (!g_fbo) {
        glGenFramebuffers(1, &g_fbo);
        glGenTextures(1, &g_fboTex);
        glBindTexture(GL_TEXTURE_2D, g_fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenRenderbuffers(1, &g_fboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, g_fboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fboTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_fboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    GLint pf = 0, pv[4]; GLint pp = 0; GLboolean pd = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &pf);
    glGetIntegerv(GL_VIEWPORT, pv);
    glGetIntegerv(GL_CURRENT_PROGRAM, &pp);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glViewport(0, 0, 512, 512);
    glClearColor(0.10f, 0.11f, 0.13f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (g_prog) {
        glEnable(GL_DEPTH_TEST);
        glUseProgram(g_prog);
        SetUniforms(g_prog);
        glBindVertexArray(g_sphVAO);
        glDrawElements(GL_TRIANGLES, g_sphIdx, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, pf);
    glViewport(pv[0], pv[1], pv[2], pv[3]);
    glUseProgram(pp);
    if (!pd) glDisable(GL_DEPTH_TEST);
}

void RenderShaderEditor() {
    if (!showShaderEditor || g_shMinimized) return;
    static bool init = false;
    if (!init) { EnsureTemplates(); Scan(); if (!g_files.empty()) { Load(0); DoCompile(); } init = true; }

    static bool dockInit = false;
    if (!dockInit) {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        ImGuiWindow* best = nullptr; float bestArea = 0;
        for (int i = 0; i < ctx->Windows.Size; i++) {
            ImGuiWindow* w = ctx->Windows[i];
            if (w->DockId == 0) continue;
            if (strcmp(w->Name, "Shader Editor") == 0) continue;
            float area = w->Size.x * w->Size.y;
            if (area > bestArea) { bestArea = area; best = w; }
        }
        if (best) { ImGui::SetNextWindowDockID(best->DockId, ImGuiCond_Always); dockInit = true; }
    }
    bool began = false;
    if (!g_shaderEmbedded) { began = true; ImGui::Begin("Shader Editor", nullptr, ImGuiWindowFlags_NoCollapse); ImGui::SetCursorPosX(ImGui::GetWindowWidth()-64); if (ImGui::SmallButton(" - ##sh")) { g_shMinimized=true; showShaderEditor=false; } ImGui::SameLine(); if (ImGui::SmallButton(" X ##sh")) { showShaderEditor=false; g_shMinimized=false; } }
    if (g_liveApply && g_cur >= 0) { namespace fs = std::filesystem; std::error_code ec; auto mt = fs::last_write_time(g_files[g_cur], ec); if (!ec && mt != g_lastMtime) { g_lastMtime = mt; Load(g_cur); } if (strcmp(g_code, g_lastCompiled.c_str()) != 0) { g_lastCompiled = g_code; DoCompile(); if (g_prog) g_CustomSceneProgram = g_prog; } }
    ImGui::TextDisabled("Assets / Shaders");
    ImGui::Separator();

    ImGui::BeginChild("##shlist", ImVec2(180, 0), true);
    for (int i = 0; i < (int)g_files.size(); i++) {
        std::string nm = std::filesystem::path(g_files[i]).filename().string();
        if (ImGui::Selectable(nm.c_str(), g_cur == i)) { Load(i); DoCompile(); }
    }
    ImGui::Separator();
    if (ImGui::Button("New")) {
        std::string p = ShadersDir() + "/custom_" + std::to_string(g_files.size() + 1) + ".shader";
        std::ofstream o(p); o << kTplToon;
        Scan();
        for (int i = 0; i < (int)g_files.size(); i++) if (g_files[i] == p) { Load(i); DoCompile(); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) { Scan(); }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##shedit", ImVec2(0, 0), false);
    if (g_cur >= 0) {
        if (ImGui::Button("Save (Ctrl+S)")) SaveFile();
        ImGui::SameLine();
        if (ImGui::Button("Compile")) DoCompile();
        ImGui::SameLine();
        if (ImGui::Button("Apply to Scene")) { DoCompile(); if (g_prog) { g_CustomSceneProgram = g_prog; snprintf(g_log, sizeof(g_log), "Applied to scene!"); } }
        ImGui::SameLine();
        if (ImGui::Button("Reset Scene Shader")) { g_CustomSceneProgram = 0; snprintf(g_log, sizeof(g_log), "Scene shader reset"); }
        ImGui::SameLine(); ImGui::Checkbox("Live apply", &g_liveApply);
        ImGui::SameLine();
        ImGui::TextDisabled("LMB drag on preview: rotate");
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", g_log);
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& pr : g_params) {
                if (pr.type==1) ImGui::DragFloat((pr.name+"##p").c_str(), &pr.v[0], 0.01f, -10.0f, 10.0f);
                else ImGui::ColorEdit3(pr.name.c_str(), pr.v);
            }
            if (g_params.empty()) ImGui::TextDisabled("(add uniform float/vec3 in shader code)");
        }

        RenderPreview();
        ImVec2 r0 = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)(intptr_t)g_fboTex, ImVec2(220, 220), ImVec2(0,1), ImVec2(1,0));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            g_yaw += d.x * 0.008f; g_pitch += d.y * 0.008f;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
        ImGui::SameLine();
        ImGui::BeginChild("##code", ImVec2(0, 0), true);
        ImGui::InputTextMultiline("##code", g_code, sizeof(g_code), ImVec2(-1, -1));
        if (ImGui::IsItemActive() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) SaveFile();
        if (ImGui::IsItemActive() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter)) DoCompile();
        ImGui::EndChild();
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1), "No .shader files found");
    }
    ImGui::EndChild();

    ImGui::End();
}









