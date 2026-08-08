#pragma once
// ── Material: цвет/текстура/тайлинг/слои + кисть для покраски маски (вынесено из main.cpp) ──

struct Material {
    std::string name = "Material";
    glm::vec3   color = {1.f,1.f,1.f};
    std::string texturePath;
    GLuint      textureID = 0;
    float       roughness = 0.5f;
    float       metallic  = 0.0f;
    float       tilingX = 1.f, tilingY = 1.f;
    std::string assetPath;

    std::string layer2TexturePath;
    GLuint      layer2TextureID = 0;
    float       layer2TilingX = 1.f, layer2TilingY = 1.f;
    std::string maskTexturePath;
    GLuint      maskTextureID = 0;
    std::vector<unsigned char> maskPixels; // CPU-копия маски для рисования (grayscale), пусто если маска — обычный файл
    int maskPixelSize = 0;
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
    f << "layer2Texture=" << m.layer2TexturePath << "\n";
    f << "layer2TilingX=" << m.layer2TilingX << "\n";
    f << "layer2TilingY=" << m.layer2TilingY << "\n";
    f << "maskTexture=" << m.maskTexturePath << "\n";
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
        else if (key=="layer2Texture") {
            m.layer2TexturePath = val;
            if (!val.empty() && fs::exists(val)) m.layer2TextureID = VE::LoadTexture(val);
        }
        else if (key=="layer2TilingX") m.layer2TilingX = std::stof(val);
        else if (key=="layer2TilingY") m.layer2TilingY = std::stof(val);
        else if (key=="maskTexture") {
            m.maskTexturePath = val;
            if (!val.empty() && fs::exists(val)) m.maskTextureID = VE::LoadTexture(val);
        }
    }
    return m;
}

// ═══════════════════════════════════════════════════════
//   КИСТЬ ДЛЯ РИСОВАНИЯ МАСКИ (Layer2 blend mask), без внешних либ
// ═══════════════════════════════════════════════════════
struct MeshTri { glm::vec3 p0,p1,p2; glm::vec2 uv0,uv1,uv2; };

inline const std::vector<MeshTri>& GetCubeTrisForPaint() {
    static std::vector<MeshTri> tris;
    if (tris.empty()) {
        struct V{float x,y,z,u,v;};
        V verts[24] = {
            {-0.5f,-0.5f,-0.5f,0,0},{0.5f,-0.5f,-0.5f,1,0},{0.5f,0.5f,-0.5f,1,1},{-0.5f,0.5f,-0.5f,0,1},
            {-0.5f,-0.5f,0.5f,0,0}, {0.5f,-0.5f,0.5f,1,0}, {0.5f,0.5f,0.5f,1,1}, {-0.5f,0.5f,0.5f,0,1},
            {-0.5f,-0.5f,-0.5f,0,0},{-0.5f,0.5f,-0.5f,1,0},{-0.5f,0.5f,0.5f,1,1},{-0.5f,-0.5f,0.5f,0,1},
            {0.5f,-0.5f,-0.5f,0,0}, {0.5f,0.5f,-0.5f,1,0}, {0.5f,0.5f,0.5f,1,1}, {0.5f,-0.5f,0.5f,0,1},
            {-0.5f,-0.5f,-0.5f,0,0},{0.5f,-0.5f,-0.5f,1,0},{0.5f,-0.5f,0.5f,1,1},{-0.5f,-0.5f,0.5f,0,1},
            {-0.5f,0.5f,-0.5f,0,0}, {0.5f,0.5f,-0.5f,1,0}, {0.5f,0.5f,0.5f,1,1}, {-0.5f,0.5f,0.5f,0,1}
        };
        int idx[36]={0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
        for (int i=0;i<36;i+=3) {
            auto& a=verts[idx[i]]; auto& b=verts[idx[i+1]]; auto& c=verts[idx[i+2]];
            tris.push_back({ {a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z}, {a.u,a.v},{b.u,b.v},{c.u,c.v} });
        }
    }
    return tris;
}
inline const std::vector<MeshTri>& GetPlaneTrisForPaint() {
    static std::vector<MeshTri> tris;
    if (tris.empty()) {
        glm::vec3 p00(-0.5f,0,-0.5f), p10(0.5f,0,-0.5f), p01(-0.5f,0,0.5f), p11(0.5f,0,0.5f);
        tris.push_back({p00,p01,p10, {0,0},{0,1},{1,0}});
        tris.push_back({p10,p01,p11, {1,0},{0,1},{1,1}});
    }
    return tris;
}

inline bool RayTriIntersect(const glm::vec3& ro,const glm::vec3& rd,
    const glm::vec3& p0,const glm::vec3& p1,const glm::vec3& p2,
    float& outT,float& outU,float& outV)
{
    glm::vec3 e1=p1-p0, e2=p2-p0;
    glm::vec3 h=glm::cross(rd,e2);
    float a=glm::dot(e1,h);
    if (fabs(a)<1e-6f) return false;
    float f=1.f/a;
    glm::vec3 s=ro-p0;
    float u=f*glm::dot(s,h);
    if (u<0.f||u>1.f) return false;
    glm::vec3 q=glm::cross(s,e1);
    float v=f*glm::dot(rd,q);
    if (v<0.f||u+v>1.f) return false;
    float t=f*glm::dot(e2,q);
    if (t<=1e-5f) return false;
    outT=t; outU=u; outV=v;
    return true;
}

inline bool RaycastObjectUV(const struct Ray& worldRay, const struct SceneObject& obj, glm::vec2& outUV);

inline void CreateBlankMask(Material& m, int size) {
    m.maskPixelSize = size;
    m.maskPixels.assign((size_t)size*size, 255);
}

inline void UploadMaskTexture(Material& m) {
    if (m.maskPixels.empty()) return;
    if (m.maskTextureID == 0) {
        glGenTextures(1,&m.maskTextureID);
        glBindTexture(GL_TEXTURE_2D,m.maskTextureID);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D,m.maskTextureID);
    }
    glTexImage2D(GL_TEXTURE_2D,0,GL_RED,m.maskPixelSize,m.maskPixelSize,0,GL_RED,GL_UNSIGNED_BYTE,m.maskPixels.data());
    glBindTexture(GL_TEXTURE_2D,0);
}

inline void StampBrush(Material& m, glm::vec2 uv, float radiusUV, bool paintWhite) {
    if (m.maskPixelSize<=0) return;
    int size=m.maskPixelSize;
    int cx=(int)(uv.x*size), cy=(int)(uv.y*size);
    int r=std::max(1,(int)(radiusUV*size));
    unsigned char target = paintWhite?255:0;
    for (int y=cy-r;y<=cy+r;y++){
        if (y<0||y>=size) continue;
        for (int x=cx-r;x<=cx+r;x++){
            if (x<0||x>=size) continue;
            float d=sqrtf((float)((x-cx)*(x-cx)+(y-cy)*(y-cy)));
            if (d>r) continue;
            float falloff = 1.f - (d/(float)r);
            unsigned char& px = m.maskPixels[(size_t)y*size+x];
            px = (unsigned char)(px + (target-(int)px)*std::min(1.f,falloff*1.5f));
        }
    }
    UploadMaskTexture(m);
}

inline void SaveMaskPGM(const std::string& path, const std::vector<unsigned char>& pixels, int size) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    f << "P5\n" << size << " " << size << "\n255\n";
    f.write((const char*)pixels.data(), pixels.size());
}
inline bool LoadMaskPGM(const std::string& path, std::vector<unsigned char>& outPixels, int& outSize) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string magic; f >> magic;
    if (magic != "P5") return false;
    int w,h,maxv; f >> w >> h >> maxv; f.get();
    outSize = w;
    outPixels.resize((size_t)w*h);
    f.read((char*)outPixels.data(), outPixels.size());
    return true;
}

