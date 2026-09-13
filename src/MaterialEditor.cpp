// MaterialEditor.cpp — VisualEngine node-based material editor.
#define _CRT_SECURE_NO_WARNINGS

#include "imnodes.h"
#include <imgui.h>
#include "imgui_internal.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Core/TextureLoader.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

bool showMaterialEditor = false;
bool g_matMinimized = false;
bool g_materialEmbedded = false;

typedef void (*ApplyMaterialFn)(float r, float g, float b, float metallic, float roughness);
ApplyMaterialFn g_applyMaterialFn = nullptr;
extern void VE_ApplyMaterialToSelected(float r, float g, float b, float metallic, float roughness);

enum NodeType { NT_Color = 0, NT_Scalar, NT_TexCoord, NT_Texture, NT_Multiply, NT_Add, NT_Mix, NT_Noise, NT_Output };

struct NodeData {
    int id = 0;
    NodeType type = NT_Color;
    float color[4] = { 1.0f, 0.2f, 0.2f, 1.0f };
    float scalar = 0.5f;
    float tiling[2] = { 1.0f, 1.0f };
    float offset[2] = { 0.0f, 0.0f };
    float noiseScale = 8.0f;
    char texPath[260] = "";
    GLuint texId = 0;
    bool showThumb = true;
    ImVec2 spawn = ImVec2(100, 150);
    bool hasSpawn = false;
    bool spawnApplied = false;
};

struct LinkData { int id; int from; int to; };

static std::vector<NodeData> g_nodes;
static std::vector<LinkData> g_links;
static int g_nextNodeId = 1;
static int g_nextLinkId = 1;
static char g_glslBuffer[4096] = "";
static char g_statusMsg[256] = "";
static std::unordered_map<std::string, GLuint> g_texCache;
static int g_ctxNode = -1;
static bool g_shaderDirty = true;
static std::vector<std::string> g_texFiles;
static bool g_texScanDone = false;

static int g_blendMode = 0;
static int g_shadingModel = 0;
static bool g_twoSided = false;

static int AttrIn(int id, int slot) { return id * 10 + 1 + slot; }
static int AttrOut(int id) { return id * 10 + 9; }
static void MarkDirty() { g_shaderDirty = true; }

static std::string ResolveTexPath(const std::string& p) {
    namespace fs = std::filesystem;
    if (fs::exists(p)) return p;
    std::string fn = fs::path(p).filename().string();
    const char* roots[] = { ".", "..", "../..", "../../.." };
    const char* subs[] = { "", "Assets/Textures/", "assets/textures/", "Assets/", "assets/", "dist/Assets/Textures/" };
    for (auto r : roots) for (auto s : subs) {
        std::string c = std::string(r) + "/" + s + fn;
        if (fs::exists(c)) return c;
    }
    return p;
}

static GLuint GetOrCreateTexture(const std::string& path) {
    auto it = g_texCache.find(path);
    if (it != g_texCache.end()) return it->second;
    GLuint t = VE::LoadTextureRaw(ResolveTexPath(path));
    g_texCache[path] = t;
    return t;
}

static void ScanProjectTextures() {
    namespace fs = std::filesystem;
    g_texFiles.clear();
    const char* roots[] = { ".", "..", "../.." };
    const char* subs[] = { "Assets/Textures", "assets/textures", "Assets", "assets" };
    for (auto r : roots) for (auto s : subs) {
        fs::path d = fs::path(r) / s;
        std::error_code ec;
        if (!fs::is_directory(d, ec)) continue;
        for (auto& e : fs::directory_iterator(d, ec)) {
            std::string ext = e.path().extension().string();
            for (auto& c : ext) c = (char)tolower(c);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
                g_texFiles.push_back(e.path().string());
        }
        if (!g_texFiles.empty()) { g_texScanDone = true; return; }
    }
    g_texScanDone = true;
}

static void AddNode(NodeType t) {
    NodeData n;
    n.id = g_nextNodeId++;
    n.type = t;
    n.spawn = ImVec2(90.0f + (float)g_nodes.size() * 45.0f, 130.0f + (float)g_nodes.size() * 45.0f);
    g_nodes.push_back(n);
    MarkDirty();
}

static void AddTextureNode(const std::string& path) {
    NodeData n;
    n.id = g_nextNodeId++;
    n.type = NT_Texture;
    n.spawn = ImVec2(120.0f + (float)g_nodes.size() * 40.0f, 150.0f + (float)g_nodes.size() * 40.0f);
    strncpy(n.texPath, path.c_str(), sizeof(n.texPath) - 1);
    n.texId = GetOrCreateTexture(path);
    g_nodes.push_back(n);
    MarkDirty();
    snprintf(g_statusMsg, sizeof(g_statusMsg), "Texture node: %s", path.c_str());
}

static void DeleteNode(int id) {
    for (int k = (int)g_links.size() - 1; k >= 0; k--)
        if (g_links[k].from / 10 == id || g_links[k].to / 10 == id)
            g_links.erase(g_links.begin() + k);
    for (int nn = (int)g_nodes.size() - 1; nn >= 0; nn--)
        if (g_nodes[nn].id == id) g_nodes.erase(g_nodes.begin() + nn);
    MarkDirty();
}

static void DuplicateNode(int id) {
    for (size_t i = 0; i < g_nodes.size(); i++) {
        if (g_nodes[i].id != id) continue;
        NodeData c = g_nodes[i];
        c.id = g_nextNodeId++;
        c.spawn = ImVec2(g_nodes[i].spawn.x + 40, g_nodes[i].spawn.y + 40);
        c.hasSpawn = true; c.spawnApplied = false;
        g_nodes.push_back(c);
        MarkDirty();
        break;
    }
}

static void DeleteSelected() {
    int lc = ImNodes::NumSelectedLinks();
    if (lc > 0) {
        std::vector<int> sl(lc);
        ImNodes::GetSelectedLinks(&sl[0]);
        for (int i = 0; i < lc; i++)
            for (int k = (int)g_links.size() - 1; k >= 0; k--)
                if (g_links[k].id == sl[i]) g_links.erase(g_links.begin() + k);
        MarkDirty();
    }
    int nc = ImNodes::NumSelectedNodes();
    if (nc > 0) {
        std::vector<int> sn(nc);
        ImNodes::GetSelectedNodes(&sn[0]);
        for (int i = 0; i < nc; i++) DeleteNode(sn[i]);
    }
    if (lc > 0 || nc > 0) snprintf(g_statusMsg, sizeof(g_statusMsg), "Deleted");
}

static void InitDefaultGraph() {
    g_nodes.clear(); g_links.clear(); g_nextNodeId = 1; g_nextLinkId = 1;
    AddNode(NT_Color);
    AddNode(NT_Scalar);
    AddNode(NT_Scalar);
    AddNode(NT_Output);
    g_links.push_back({ g_nextLinkId++, AttrOut(1), AttrIn(4, 0) });
    g_links.push_back({ g_nextLinkId++, AttrOut(2), AttrIn(4, 1) });
    g_links.push_back({ g_nextLinkId++, AttrOut(3), AttrIn(4, 2) });
    MarkDirty();
}

static void Eval(int inputAttr, float out[4], int depth) {
    out[0] = out[1] = out[2] = 1.0f; out[3] = 1.0f;
    if (depth > 8) return;
    for (size_t i = 0; i < g_links.size(); i++) {
        if (g_links[i].to != inputAttr) continue;
        NodeData* src = nullptr;
        for (size_t j = 0; j < g_nodes.size(); j++)
            if (AttrOut(g_nodes[j].id) == g_links[i].from) src = &g_nodes[j];
        if (!src) return;
        switch (src->type) {
        case NT_Color: for (int k = 0; k < 4; k++) out[k] = src->color[k]; break;
        case NT_Scalar: out[0] = out[1] = out[2] = src->scalar; break;
        case NT_Texture: out[0] = out[1] = out[2] = 0.8f; break;
        case NT_TexCoord: out[0] = out[1] = out[2] = 1.0f; break;
        case NT_Noise: out[0] = out[1] = out[2] = 0.5f; break;
        case NT_Multiply: case NT_Add: case NT_Mix: {
            float a[4], b[4], t[4];
            Eval(AttrIn(src->id, 0), a, depth + 1);
            Eval(AttrIn(src->id, 1), b, depth + 1);
            if (src->type == NT_Multiply) for (int k = 0; k < 3; k++) out[k] = a[k] * b[k];
            else if (src->type == NT_Add) for (int k = 0; k < 3; k++) out[k] = a[k] + b[k];
            else { Eval(AttrIn(src->id, 2), t, depth + 1); for (int k = 0; k < 3; k++) out[k] = a[k] + (b[k] - a[k]) * t[0]; }
            break;
        }
        default: break;
        }
        return;
    }
}

static int FindOutputNode() {
    for (size_t i = 0; i < g_nodes.size(); i++) if (g_nodes[i].type == NT_Output) return g_nodes[i].id;
    return -1;
}

static NodeData* FindNodeById(int id) {
    for (size_t i = 0; i < g_nodes.size(); i++) if (g_nodes[i].id == id) return &g_nodes[i];
    return nullptr;
}

static std::string SrcVarFor(int inputAttr, const char* def) {
    for (size_t i = 0; i < g_links.size(); i++)
        if (g_links[i].to == inputAttr)
            for (size_t j = 0; j < g_nodes.size(); j++)
                if (AttrOut(g_nodes[j].id) == g_links[i].from)
                    return "n" + std::to_string(g_nodes[j].id);
    return def;
}
static std::string SrcVarFor(int inputAttr) { return SrcVarFor(inputAttr, "vec4(0.8)"); }

static std::string GenPreviewFrag() {
    char buf[256];
    std::string s = "#version 330 core\nin vec3 N; in vec3 W; in vec3 Nlocal;\nuniform vec3 uCamPos;\nout vec4 frag;\n";
    for (auto& n : g_nodes) if (n.type == NT_Texture) s += "uniform sampler2D uT" + std::to_string(n.id) + ";\n";
    s += "float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n";
    s += "float noise(vec2 p){vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}\n";
    s += "void main(){\n";
    s += "  vec3 nl=normalize(Nlocal);\n  vec2 uv0=vec2(atan(nl.z,nl.x)/6.2831853+0.5,nl.y*0.5+0.5);\n  vec2 uvm=vec2(1.0-abs(uv0.x*2.0-1.0),uv0.y);\n";
    for (auto& n : g_nodes) {
        std::string id = std::to_string(n.id);
        switch (n.type) {
        case NT_Color:
            snprintf(buf, 256, "  vec4 n%s=vec4(%.3f,%.3f,%.3f,1.0);\n", id.c_str(), n.color[0], n.color[1], n.color[2]);
            s += buf; break;
        case NT_Scalar:
            snprintf(buf, 256, "  vec4 n%s=vec4(%.3f);\n", id.c_str(), n.scalar); s += buf; break;
        case NT_TexCoord:
            snprintf(buf, 256, "  vec4 n%s=vec4(uv0*vec2(%.2f,%.2f)+vec2(%.2f,%.2f),0.0,0.0);\n", id.c_str(), n.tiling[0], n.tiling[1], n.offset[0], n.offset[1]);
            s += buf; break;
        case NT_Noise:
            snprintf(buf, 256, "  vec4 n%s=vec4(noise(uv0*%.2f));\n", id.c_str(), n.noiseScale); s += buf; break;
        case NT_Texture: {
            std::string uv = "uv0";
            for (auto& l : g_links) if (l.to == AttrIn(n.id, 0)) { uv = SrcVarFor(AttrIn(n.id, 0)) + ".xy"; break; }
            s += "  vec4 n" + id + "=textureLod(uT" + id + "," + uv + ",0.0);\n";
            break;
        }
        case NT_Multiply:
            s += "  vec4 n" + id + "=" + SrcVarFor(AttrIn(n.id, 0)) + "*" + SrcVarFor(AttrIn(n.id, 1)) + ";\n"; break;
        case NT_Add:
            s += "  vec4 n" + id + "=" + SrcVarFor(AttrIn(n.id, 0)) + "+" + SrcVarFor(AttrIn(n.id, 1)) + ";\n"; break;
        case NT_Mix:
            s += "  vec4 n" + id + "=mix(" + SrcVarFor(AttrIn(n.id, 0)) + "," + SrcVarFor(AttrIn(n.id, 1)) + ",clamp(" + SrcVarFor(AttrIn(n.id, 2)) + ".r,0.0,1.0));\n"; break;
        default: break;
        }
    }
    int outId = FindOutputNode();
    std::string base = outId >= 0 ? SrcVarFor(AttrIn(outId, 0), "vec4(0.8)") : "vec4(0.8)";
    std::string met = outId >= 0 ? SrcVarFor(AttrIn(outId, 1), "vec4(0.0)") : "vec4(0.0)";
    std::string rgh = outId >= 0 ? SrcVarFor(AttrIn(outId, 2), "vec4(0.5)") : "vec4(0.5)";
    s += "  vec3 base=" + base + ".rgb;\n";
    s += "  float metallic=clamp(" + met + ".r,0.0,1.0);\n";
    s += "  float roughness=clamp(" + rgh + ".r,0.03,1.0);\n";
    s +=
        "  vec3 n=normalize(N);\n"
        "  vec3 v=normalize(uCamPos-W);\n"
        "  vec3 l=normalize(vec3(0.6,0.8,0.45));\n"
        "  vec3 h=normalize(v+l);\n"
        "  float NdL=max(dot(n,l),0.0);\n"
        "  float NdH=max(dot(n,h),0.0);\n"
        "  float VdH=max(dot(v,h),0.0);\n"
        "  float a=roughness*roughness; float a2=a*a;\n"
        "  float d=NdH*NdH*(a2-1.0)+1.0;\n"
        "  float D=a2/(3.14159*d*d);\n"
        "  float F0=mix(0.04,0.9,metallic);\n"
        "  float F=F0+(1.0-F0)*pow(1.0-VdH,5.0);\n"
        "  vec3 diff=base*NdL*(1.0-metallic*0.7);\n"
        "  vec3 spec=vec3(D*F*0.25)*NdL;\n"
        "  if(metallic>0.5) spec*=base;\n"
        "  vec3 amb=vec3(0.10,0.10,0.11)*base;\n"
        "  float rim=pow(1.0-max(dot(n,v),0.0),3.0)*0.10;\n"
        "  vec3 col=diff+spec+amb+vec3(rim);\n"
        "  vec3 rd=reflect(-v,n);\n"
        "  vec3 env=mix(vec3(0.50,0.62,0.70),vec3(0.18,0.38,0.70),clamp(rd.y,0.0,1.0));\n"
        "  col+=env*F*(0.3+0.7*metallic)*0.15;\n"
        "  col=clamp((col*(2.51*col+0.03))/(col*(2.43*col+0.59)+0.14),0.0,1.0);\n"
        "  col=pow(col,vec3(0.4545));\n"
        "  frag=vec4(col,1.0);\n}\n";
    return s;
}

static void CompileGraph() {
    std::string s = GenPreviewFrag();
    snprintf(g_glslBuffer, sizeof(g_glslBuffer), "%s", s.c_str());
    snprintf(g_statusMsg, sizeof(g_statusMsg), "Shader compiled (%d nodes, %d links)", (int)g_nodes.size(), (int)g_links.size());
    MarkDirty();
}

static void SaveMaterial() {
    int outId = FindOutputNode();
    float base[4], met[4], rgh[4];
    if (outId >= 0) { Eval(AttrIn(outId, 0), base, 0); Eval(AttrIn(outId, 1), met, 0); Eval(AttrIn(outId, 2), rgh, 0); }
    std::string texPathSave;
    if (outId >= 0) {
        for (auto& lk : g_links) if (lk.to == AttrIn(outId, 0)) {
            NodeData* nn = FindNodeById(lk.from / 10);
            if (nn && nn->type == NT_Texture) { texPathSave = nn->texPath; break; }
            if (nn && nn->type == NT_Multiply) {
                for (auto& l2 : g_links) if (l2.to == AttrIn(nn->id, 0) || l2.to == AttrIn(nn->id, 1)) {
                    NodeData* tt = FindNodeById(l2.from / 10);
                    if (tt && tt->type == NT_Texture) { texPathSave = tt->texPath; }
                }
            }
        }
    }
    namespace fs = std::filesystem;
    const char* roots[] = { ".", "project", "..", "../.." };
    bool saved = false;
    for (auto r : roots) {
        std::error_code ec;
        if (!fs::exists(fs::path(r) / "Assets", ec)) continue;
        fs::path dir = fs::path(r) / "Assets" / "Materials";
        fs::create_directories(dir, ec);
        std::ofstream f(dir / "M_NewMaterial.mat");
        if (!f) continue;
        f << "name=M_NewMaterial\n";
        if (!texPathSave.empty()) { f << "color=1,1,1\n"; f << "texture=" << texPathSave << "\n"; } else { f << "color=" << base[0] << "," << base[1] << "," << base[2] << "\n"; }
        f << "metallic=" << met[0] << "\n";
        f << "roughness=" << rgh[0] << "\n";
        f.close();
        snprintf(g_statusMsg, sizeof(g_statusMsg), "Saved: %s", (dir / "M_NewMaterial.mat").string().c_str());
        saved = true;
        break;
    }
    if (!saved) snprintf(g_statusMsg, sizeof(g_statusMsg), "Save failed: no Assets dir");
}
static void ApplyMaterial() {
    int outId = FindOutputNode();
    if (outId < 0) { snprintf(g_statusMsg, sizeof(g_statusMsg), "No Output node"); return; }
    float base[4], met[4], rgh[4];
    Eval(AttrIn(outId, 0), base, 0); Eval(AttrIn(outId, 1), met, 0); Eval(AttrIn(outId, 2), rgh, 0);
    if (g_applyMaterialFn) { g_applyMaterialFn(base[0], base[1], base[2], met[0], rgh[0]); snprintf(g_statusMsg, sizeof(g_statusMsg), "Applied to selected object"); }
    else snprintf(g_statusMsg, sizeof(g_statusMsg), "Bridge not connected yet");
}

static GLuint g_fbo = 0, g_fboTex = 0, g_fboDepth = 0;
static GLuint g_sphereVAO = 0, g_sphereVBO = 0, g_sphereEBO = 0;
static int    g_sphereIdxCount = 0;
static GLuint g_planeVAO = 0, g_planeVBO = 0;
static GLuint g_bgVAO = 0, g_bgVBO = 0;
static GLuint g_dynProg = 0, g_groundProg = 0, g_bgProg = 0;
static float  g_yaw = 0.6f, g_pitch = 0.3f;
static const int FBO_SIZE = 512;

static const char* kVertSrc =
"#version 330 core\n"
"layout(location=0) in vec3 aPos;\n"
"layout(location=1) in vec3 aNormal;\n"
"uniform mat4 uProj; uniform mat4 uView; uniform mat4 uModel;\n"
"out vec3 N; out vec3 W; out vec3 Nlocal;\n"
"void main(){\n"
"  vec4 w = uModel * vec4(aPos,1.0);\n"
"  W = w.xyz;\n"
"  N = mat3(uModel)*aNormal;\n"
"  Nlocal = aPos;\n"
"  gl_Position = uProj*uView*w;\n"
"}\n";

static const char* kGroundFrag =
"#version 330 core\n"
"in vec3 N; in vec3 W; in vec3 Nlocal;\n"
"uniform vec3 uCamPos;\nout vec4 frag;\n"
"out vec4 frag;\n"
"float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}\n"
"float noise(vec2 p){vec2 i=floor(p);vec2 f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);}\n"
"void main(){\n"
"  vec3 n=normalize(N); vec3 v=normalize(uCamPos-W);\n"
"  vec3 l=normalize(vec3(0.6,0.8,0.45));\n"
"  float NdL=max(dot(n,l),0.0);\n"
"  vec3 base=vec3(0.055,0.055,0.06);\n"
"  float gn=noise(W.xz*1.5)*0.6+noise(W.xz*6.0)*0.4;\n"
"  base*=mix(0.6,1.15,gn);\n"
"  vec3 col=base*NdL+vec3(0.10,0.10,0.11)*base;\n"
"  float sh=smoothstep(0.8,1.8,length(W.xz-vec2(0.35,0.35)));\n"
"  col*=mix(0.4,1.0,sh);\n"
"  float fog=clamp(1.0-exp(-length(uCamPos-W)*0.05),0.0,1.0);\n"
"  col=mix(col,vec3(0.07,0.07,0.09),fog);\n"
"  col=clamp((col*(2.51*col+0.03))/(col*(2.43*col+0.59)+0.14),0.0,1.0);\n"
"  col=pow(col,vec3(0.4545));\n"
"  frag=vec4(col,1.0);\n"
"}\n";

static const char* kBgVert =
"#version 330 core\n"
"layout(location=0) in vec2 aPos;\n"
"out vec2 vUv;\n"
"void main(){ vUv = aPos*0.5+0.5; gl_Position = vec4(aPos,0.999,1.0); }\n";

static const char* kBgFrag =
"#version 330 core\n"
"in vec2 vUv;\n"
"out vec4 frag;\n"
"void main(){\n"
"  vec3 top = vec3(0.16,0.17,0.20);\n"
"  vec3 bot = vec3(0.05,0.05,0.07);\n"
"  vec3 col = mix(bot, top, smoothstep(0.0,1.0,vUv.y));\n"
"  float d = distance(vUv, vec2(0.5,0.62));\n"
"  col += vec3(0.10,0.11,0.13) * exp(-d*3.5);\n"
"  col = mix(col, vec3(0.09,0.09,0.11), smoothstep(0.42,0.0,vUv.y)*0.6);\n"
"  frag = vec4(col,1.0);\n"
"}\n";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static GLuint BuildProgram(const char* vs, const char* fs, bool* ok) {
    GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    GLint st = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &st);
    if (ok) *ok = (st == GL_TRUE);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

static void BuildSphere() {
    const int lat = 32, lon = 32;
    std::vector<float> v; std::vector<unsigned int> ix;
    for (int i = 0; i <= lat; i++) {
        float theta = i * 3.14159265f / lat;
        for (int j = 0; j <= lon; j++) {
            float phi = j * 2.0f * 3.14159265f / lon;
            float x = sinf(theta) * cosf(phi), y = cosf(theta), z = sinf(theta) * sinf(phi);
            v.push_back(x); v.push_back(y); v.push_back(z);
            v.push_back(x); v.push_back(y); v.push_back(z);
        }
    }
    for (int i = 0; i < lat; i++) for (int j = 0; j < lon; j++) {
        unsigned int a = i * (lon + 1) + j, b = a + lon + 1;
        ix.push_back(a); ix.push_back(b); ix.push_back(a + 1);
        ix.push_back(a + 1); ix.push_back(b); ix.push_back(b + 1);
    }
    g_sphereIdxCount = (int)ix.size();
    glGenVertexArrays(1, &g_sphereVAO); glGenBuffers(1, &g_sphereVBO); glGenBuffers(1, &g_sphereEBO);
    glBindVertexArray(g_sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(ix.size() * sizeof(unsigned int)), ix.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static void BuildPlane() {
    float s = 30.0f, y = -1.15f;
    float v[] = {
        -s,y,-s, 0,1,0,   s,y,-s, 0,1,0,   s,y,s, 0,1,0,
        -s,y,-s, 0,1,0,   s,y,s,  0,1,0,  -s,y,s, 0,1,0
    };
    glGenVertexArrays(1, &g_planeVAO); glGenBuffers(1, &g_planeVBO);
    glBindVertexArray(g_planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

static void BuildBg() {
    float v[] = { -1,-1,  3,-1,  -1,3 };
    glGenVertexArrays(1, &g_bgVAO); glGenBuffers(1, &g_bgVBO);
    glBindVertexArray(g_bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_bgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

static void EnsureGLObjects() {
    if (!g_fbo) {
        glGenFramebuffers(1, &g_fbo);
        glGenTextures(1, &g_fboTex);
        glBindTexture(GL_TEXTURE_2D, g_fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FBO_SIZE, FBO_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenRenderbuffers(1, &g_fboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, g_fboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, FBO_SIZE, FBO_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fboTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_fboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    if (!g_sphereVAO) BuildSphere();
    if (!g_planeVAO)  BuildPlane();
    if (!g_bgVAO)     BuildBg();
    if (!g_groundProg) { bool ok; g_groundProg = BuildProgram(kVertSrc, kGroundFrag, &ok); }
    if (!g_bgProg) { bool ok; g_bgProg = BuildProgram(kBgVert, kBgFrag, &ok); }
}

static void Render3DPreview(const float base[4], float metallic, float roughness) {
    EnsureGLObjects();

    if (g_shaderDirty) {
        std::string fs = GenPreviewFrag();
        bool ok = false;
        GLuint p = BuildProgram(kVertSrc, fs.c_str(), &ok);
        if (ok) {
            if (g_dynProg) glDeleteProgram(g_dynProg);
            g_dynProg = p;
        }
        else {
            glDeleteProgram(p);
            snprintf(g_statusMsg, sizeof(g_statusMsg), "GLSL link error — check graph");
        }
        g_shaderDirty = false;
    }
    if (!g_dynProg) { bool ok; g_dynProg = BuildProgram(kVertSrc, kGroundFrag, &ok); }

    GLint prevFbo = 0, prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);
    GLint prevProg = 0, prevTex = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glViewport(0, 0, FBO_SIZE, FBO_SIZE);

    glDisable(GL_DEPTH_TEST);
    glUseProgram(g_bgProg);
    glBindVertexArray(g_bgVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glm::vec3 camPos(0, 0, 3);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f)) * glm::rotate(glm::mat4(1.0f), g_yaw, glm::vec3(0, 1, 0)) *
        glm::rotate(glm::mat4(1.0f), g_pitch, glm::vec3(1, 0, 0));

    glUseProgram(g_groundProg);
    glUniformMatrix4fv(glGetUniformLocation(g_groundProg, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_groundProg, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniform3f(glGetUniformLocation(g_groundProg, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glm::mat4 ident(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(g_groundProg, "uModel"), 1, GL_FALSE, &ident[0][0]);
    glBindVertexArray(g_planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUseProgram(g_dynProg);
    glUniformMatrix4fv(glGetUniformLocation(g_dynProg, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(g_dynProg, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniform3f(glGetUniformLocation(g_dynProg, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniformMatrix4fv(glGetUniformLocation(g_dynProg, "uModel"), 1, GL_FALSE, &model[0][0]);
    int unit = 0;
    for (auto& n : g_nodes) {
        if (n.type != NT_Texture) continue;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, n.texId ? n.texId : 0);
        glUniform1i(glGetUniformLocation(g_dynProg, ("uT" + std::to_string(n.id)).c_str()), unit);
        unit++;
    }
    glBindVertexArray(g_sphereVAO);
    glDrawElements(GL_TRIANGLES, g_sphereIdxCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glUseProgram(prevProg);
    glBindTexture(GL_TEXTURE_2D, prevTex);
    if (!prevDepth) glDisable(GL_DEPTH_TEST);
}

void RenderMaterialEditor() {
    ImGuiIO& io0 = ImGui::GetIO();
    if (io0.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_M)) showMaterialEditor = !showMaterialEditor;

    if (!showMaterialEditor || g_matMinimized) return;

    static bool init = false;
    if (!init) { ImNodes::StyleColorsDark(); InitDefaultGraph(); g_applyMaterialFn = VE_ApplyMaterialToSelected; init = true; }

    static bool dockInit2 = false;
    if (!dockInit2) {
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        ImGuiWindow* best = nullptr;
        float bestArea = 0.0f;
        for (int i = 0; i < ctx->Windows.Size; i++) {
            ImGuiWindow* w = ctx->Windows[i];
            if (w->DockId == 0) continue;
            if (strcmp(w->Name, "Material Editor") == 0) continue;
            float area = w->Size.x * w->Size.y;
            if (area > bestArea) { bestArea = area; best = w; }
        }
        if (best) { ImGui::SetNextWindowDockID(best->DockId, ImGuiCond_Always); dockInit2 = true; }
    }

    ImGuiWindowFlags wflags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Material Editor", nullptr, wflags)) { ImGui::End(); return; }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Material")) { InitDefaultGraph(); snprintf(g_statusMsg, sizeof(g_statusMsg), "New material created"); }
            if (ImGui::MenuItem("Save"))          SaveMaterial();
            if (ImGui::MenuItem("Apply to Selection")) ApplyMaterial();
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) showMaterialEditor = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete Selected", "Del")) DeleteSelected();
            if (ImGui::MenuItem("Compile Shader")) CompileGraph();
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 64); if (ImGui::SmallButton(" - ##mat")) { g_matMinimized=true; showMaterialEditor=false; } ImGui::SameLine(); if (ImGui::SmallButton(" X ##mat")) { showMaterialEditor=false; g_matMinimized=false; } ImGui::EndMenuBar();
    }

    if (g_statusMsg[0]) ImGui::TextDisabled("%s", g_statusMsg);
    ImGui::TextDisabled("Assets / Materials / ");
    ImGui::SameLine();
    ImGui::Text("M_NewMaterial");
    ImGui::Separator();

    float availH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##PreviewPanel", ImVec2(360, availH), true);
    {
        ImGui::TextUnformatted("Preview");
        ImGui::TextDisabled("LMB drag: rotate | RMB: menu");
        ImGui::Separator();

        int outId = FindOutputNode();
        float base[4], met[4], rgh[4];
        if (outId >= 0) { Eval(AttrIn(outId, 0), base, 0); Eval(AttrIn(outId, 1), met, 0); Eval(AttrIn(outId, 2), rgh, 0); }
        else { base[0] = base[1] = base[2] = 1; met[0] = 0; rgh[0] = 0.5f; }

        Render3DPreview(base, met[0], rgh[0]);
        ImVec2 sz(330, 330);
        ImGui::Image((ImTextureID)(intptr_t)g_fboTex, sz, ImVec2(0, 1), ImVec2(1, 0));

        ImVec2 rmin = ImGui::GetItemRectMin();
        ImVec2 rsize = ImGui::GetItemRectSize();
        ImGuiIO& io = ImGui::GetIO();
        bool over = io.MousePos.x >= rmin.x && io.MousePos.x <= rmin.x + rsize.x &&
            io.MousePos.y >= rmin.y && io.MousePos.y <= rmin.y + rsize.y;
        if (over && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            g_yaw += delta.x * 0.008f;
            g_pitch += delta.y * 0.008f;
            if (g_pitch > 1.2f) g_pitch = 1.2f;
            if (g_pitch < -1.2f) g_pitch = -1.2f;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Material Properties");
        ImGui::Text("Metallic:   %.2f", met[0]);
        ImGui::Text("Roughness:  %.2f", rgh[0]);
        ImGui::Separator();
        ImGui::TextUnformatted("Details");
        ImGui::Combo("Blend Mode", &g_blendMode, "Opaque\0Translucent\0Additive\0");
        ImGui::Combo("Shading Model", &g_shadingModel, "Default Lit\0Unlit\0");
        ImGui::Checkbox("Two Sided", &g_twoSided);
        if (ImGui::CollapsingHeader("Generated GLSL")) ImGui::TextUnformatted(g_glslBuffer);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##GraphPanel", ImVec2(0, availH), false);
    {
        ImNodes::BeginNodeEditor();
        g_ctxNode = -1;

        for (size_t i = 0; i < g_nodes.size(); i++) {
            NodeData& n = g_nodes[i];
            if (n.hasSpawn && !n.spawnApplied) { ImNodes::SetNodeEditorSpacePos(n.id, n.spawn); n.spawnApplied = true; }

            if (n.type == NT_Color)         ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.16f, 0.28f, 0.48f, 1.0f)));
            else if (n.type == NT_Texture)  ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.32f, 0.38f, 1.0f)));
            else if (n.type == NT_Multiply) ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.38f, 0.12f, 1.0f)));
            else if (n.type == NT_Add)      ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.35f, 0.30f, 1.0f)));
            else if (n.type == NT_Mix)      ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.40f, 0.30f, 0.10f, 1.0f)));
            else if (n.type == NT_TexCoord) ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.30f, 0.15f, 0.35f, 1.0f)));
            else if (n.type == NT_Noise)    ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.35f, 0.10f, 1.0f)));
            else if (n.type == NT_Scalar)   ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.22f, 0.45f, 0.18f, 1.0f)));
            else {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.25f, 0.05f, 1.0f)));
                ImNodes::PushColorStyle(ImNodesCol_NodeOutline, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.62f, 0.10f, 1.0f)));
            }

            ImNodes::BeginNode(n.id);
            ImNodes::BeginNodeTitleBar();
            switch (n.type) {
            case NT_Color:    ImGui::TextUnformatted("Color"); break;
            case NT_Texture:  ImGui::TextUnformatted("Texture Sample"); break;
            case NT_Multiply: ImGui::TextUnformatted("Multiply"); break;
            case NT_Add:      ImGui::TextUnformatted("Add"); break;
            case NT_Mix:      ImGui::TextUnformatted("Mix (mask=holes)"); break;
            case NT_TexCoord: ImGui::TextUnformatted("UV / Tiling"); break;
            case NT_Noise:    ImGui::TextUnformatted("Noise"); break;
            case NT_Scalar:   ImGui::TextUnformatted("Scalar"); break;
            default:          ImGui::TextUnformatted("Output (PBR)"); break;
            }
            ImNodes::EndNodeTitleBar();

            switch (n.type) {
            case NT_Color:
                ImGui::ColorEdit4("##col", n.color);
                if (ImGui::IsItemEdited()) MarkDirty();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "R"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "G"); ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 1, 1), "B");
                ImNodes::EndOutputAttribute();
                break;
            case NT_Texture:
                if (ImGui::Button("Pick texture...")) { ImGui::OpenPopup("##texpick"); }
                if (ImGui::BeginPopup("##texpick")) {
                    if (!g_texScanDone) ScanProjectTextures();
                    ImGui::TextDisabled("Project textures:");
                    int col = 0;
                    for (auto& f : g_texFiles) {
                        GLuint t = GetOrCreateTexture(f);
                        if (t) {
                            if (col > 0) ImGui::SameLine();
                            ImGui::Image((ImTextureID)(intptr_t)t, ImVec2(64, 64), ImVec2(0, 1), ImVec2(1, 0));
                            if (ImGui::IsItemClicked()) {
                                strncpy(n.texPath, f.c_str(), sizeof(n.texPath) - 1);
                                n.texId = t;
                                MarkDirty();
                                ImGui::CloseCurrentPopup();
                            }
                            col = (col + 1) % 4;
                        }
                    }
                    if (g_texFiles.empty()) ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1), "No textures in Assets/Textures");
                    ImGui::Separator();
                    if (ImGui::MenuItem("Refresh")) { g_texScanDone = false; }
                    ImGui::EndPopup();
                }
                ImGui::Checkbox("thumb", &n.showThumb);
                if (n.showThumb) {
                    if (n.texId > 0) ImGui::Image((ImTextureID)(intptr_t)n.texId, ImVec2(110, 110), ImVec2(0, 1), ImVec2(1, 0));
                    else ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1), "[no texture]");
                }
                ImNodes::BeginInputAttribute(AttrIn(n.id, 0)); ImGui::TextUnformatted("UV"); ImNodes::EndInputAttribute();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));  ImGui::TextColored(ImVec4(1, 0.85f, 0.4f, 1), "RGBA"); ImNodes::EndOutputAttribute();
                ImGui::BeginDragDropTarget(); if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) { const char* path = (const char*)pl->Data; strncpy(n.texPath, path, sizeof(n.texPath)-1); n.texId = GetOrCreateTexture(path); MarkDirty(); snprintf(g_statusMsg, sizeof(g_statusMsg), "Drop: %s", path); } ImGui::EndDragDropTarget();
                break;
            case NT_Scalar:
                ImGui::SetNextItemWidth(90);
                ImGui::DragFloat("##val", &n.scalar, 0.01f, 0.0f, 1.0f);
                if (ImGui::IsItemEdited()) MarkDirty();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1), "Value");
                ImNodes::EndOutputAttribute();
                break;
            case NT_TexCoord:
                ImGui::SetNextItemWidth(120);
                ImGui::DragFloat2("tiling", n.tiling, 0.05f, 0.05f, 20.0f);
                ImGui::SetNextItemWidth(120);
                ImGui::DragFloat2("offset", n.offset, 0.01f, -10.0f, 10.0f);
                if (ImGui::IsItemEdited()) MarkDirty();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1), "UV");
                ImNodes::EndOutputAttribute();
                break;
            case NT_Noise:
                ImGui::SetNextItemWidth(120);
                ImGui::DragFloat("scale", &n.noiseScale, 0.5f, 1.0f, 100.0f);
                if (ImGui::IsItemEdited()) MarkDirty();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1), "Val");
                ImNodes::EndOutputAttribute();
                break;
            case NT_Multiply:
            case NT_Add:
                ImNodes::BeginInputAttribute(AttrIn(n.id, 0)); ImGui::TextUnformatted("A"); ImNodes::EndInputAttribute();
                ImNodes::BeginInputAttribute(AttrIn(n.id, 1)); ImGui::TextUnformatted("B"); ImNodes::EndInputAttribute();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));  ImGui::TextUnformatted(n.type == NT_Multiply ? "A*B" : "A+B"); ImNodes::EndOutputAttribute();
                break;
            case NT_Mix:
                ImNodes::BeginInputAttribute(AttrIn(n.id, 0)); ImGui::TextUnformatted("A"); ImNodes::EndInputAttribute();
                ImNodes::BeginInputAttribute(AttrIn(n.id, 1)); ImGui::TextUnformatted("B"); ImNodes::EndInputAttribute();
                ImNodes::BeginInputAttribute(AttrIn(n.id, 2)); ImGui::TextUnformatted("Mask"); ImNodes::EndInputAttribute();
                ImNodes::BeginOutputAttribute(AttrOut(n.id));  ImGui::TextUnformatted("Result"); ImNodes::EndOutputAttribute();
                break;
            default:
                ImNodes::BeginInputAttribute(AttrIn(n.id, 0)); ImGui::TextUnformatted("Base Color"); ImNodes::EndInputAttribute();
                ImNodes::BeginInputAttribute(AttrIn(n.id, 1)); ImGui::TextUnformatted("Metallic");    ImNodes::EndInputAttribute();
                ImNodes::BeginInputAttribute(AttrIn(n.id, 2)); ImGui::TextUnformatted("Roughness");   ImNodes::EndInputAttribute();
                break;
            }
            ImNodes::EndNode();

            if (n.type == NT_Texture) { if (ImGui::BeginDragDropTarget()) { if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) { const char* p = (const char*)pl->Data; strncpy(n.texPath, p, sizeof(n.texPath)-1); n.texId = GetOrCreateTexture(p); MarkDirty(); snprintf(g_statusMsg, sizeof(g_statusMsg), "Texture assigned to node"); } ImGui::EndDragDropTarget(); } }
            if (ImGui::IsItemHovered()) g_ctxNode = n.id;

            if (n.type == NT_Output) { ImNodes::PopColorStyle(); ImNodes::PopColorStyle(); }
            else ImNodes::PopColorStyle();

            if (!n.hasSpawn) n.hasSpawn = true;
        }

        for (size_t l = 0; l < g_links.size(); l++)
            ImNodes::Link(g_links[l].id, g_links[l].from, g_links[l].to);

        ImNodes::EndNodeEditor();

        int a = 0, b = 0; bool snap = false;
        if (ImNodes::IsLinkCreated(&a, &b, &snap)) {
            for (int i = (int)g_links.size() - 1; i >= 0; i--)
                if (g_links[i].to == b) g_links.erase(g_links.begin() + i);
            g_links.push_back({ g_nextLinkId++, a, b });
            MarkDirty();
            snprintf(g_statusMsg, sizeof(g_statusMsg), "Linked");
        }
        int d = 0;
        if (ImNodes::IsLinkDestroyed(&d)) {
            for (int i = (int)g_links.size() - 1; i >= 0; i--)
                if (g_links[i].id == d) { g_links.erase(g_links.begin() + i); break; }
            MarkDirty();
            snprintf(g_statusMsg, sizeof(g_statusMsg), "Unlinked");
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
                const char* path = (const char*)pl->Data;
                AddTextureNode(path);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelected();

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
            ImGui::OpenPopup("##graphctx");
        if (ImGui::BeginPopup("##graphctx")) {
            if (g_ctxNode >= 0) {
                ImGui::TextDisabled("Node %d", g_ctxNode);
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate")) DuplicateNode(g_ctxNode);
                if (ImGui::MenuItem("Delete Node")) { DeleteNode(g_ctxNode); snprintf(g_statusMsg, sizeof(g_statusMsg), "Deleted node"); }
            }
            else {
                if (ImGui::MenuItem("Color"))        AddNode(NT_Color);
                if (ImGui::MenuItem("Texture"))      AddNode(NT_Texture);
                if (ImGui::MenuItem("UV / Tiling"))  AddNode(NT_TexCoord);
                if (ImGui::MenuItem("Scalar"))       AddNode(NT_Scalar);
                if (ImGui::MenuItem("Multiply"))     AddNode(NT_Multiply);
                if (ImGui::MenuItem("Add"))          AddNode(NT_Add);
                if (ImGui::MenuItem("Mix (mask)"))   AddNode(NT_Mix);
                if (ImGui::MenuItem("Noise"))        AddNode(NT_Noise);
                if (ImGui::MenuItem("Output"))       AddNode(NT_Output);
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Selected", "Del")) DeleteSelected();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}



















