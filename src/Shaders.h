#pragma once
// ── Исходники шейдеров (вынесено из main.cpp) ──

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
uniform vec2 uTiling;
uniform sampler2D uLayer2Texture;
uniform bool useLayer2;
uniform vec2 uLayer2Tiling;
uniform sampler2D uMaskTexture;
uniform bool useMask;
uniform vec3 objectColor,viewPos;
uniform vec3  lightPos[8];
uniform vec3  lightColor[8];
uniform float lightIntensity[8];
uniform float lightRange[8];
uniform int   lightCount;
uniform vec3  fogColor;
uniform float fogDensity;
uniform vec3  sunDir;
uniform vec3  sunColor;
uniform float sunIntensity;
uniform float ambientStrength;
void main(){
    vec3 baseColor;
    if (useTexture) {
        vec3 layer1 = texture(uTexture, TexCoord*uTiling).rgb;
        if (useLayer2 && useMask) {
            vec3 layer2 = texture(uLayer2Texture, TexCoord*uLayer2Tiling).rgb;
            float maskVal = texture(uMaskTexture, TexCoord).r;
            baseColor = mix(layer1, layer2, maskVal) * objectColor;
        } else {
            baseColor = layer1 * objectColor;
        }
    } else {
        baseColor = objectColor;
    }
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
    // ── Fresnel/rim-lighting: лёгкое свечение по краю объекта под острым углом к камере ──
    float rim = pow(1.0 - max(dot(norm, vd), 0.0), 3.0);
    result += rim * 0.15 * mix(vec3(0.5), sunColor, sunIntensity);

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

// ═══════════════════════════════════════════════════════
//   BLOOM: bright-pass + гауссово размытие + ACES tonemapping
// ═══════════════════════════════════════════════════════
const char* postVertSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); })";

const char* brightPassFragSrc = R"(
#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene;
uniform float uThreshold;
void main(){
    vec3 c = texture(uScene, vUV).rgb;
    float brightness = dot(c, vec3(0.2126,0.7152,0.0722));
    float k = clamp((brightness-uThreshold)*2.0, 0.0, 1.0); // мягкий переход, без жёсткого обрезания
    FragColor = vec4(c*k, 1.0);
})";

const char* blurFragSrc = R"(
#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uImage;
uniform bool uHorizontal;
uniform vec2 uTexelSize;
void main(){
    float weights[5] = float[](0.227027,0.1945946,0.1216216,0.054054,0.016216);
    vec3 result = texture(uImage, vUV).rgb * weights[0];
    vec2 dir = uHorizontal ? vec2(uTexelSize.x,0.0) : vec2(0.0,uTexelSize.y);
    for(int i=1;i<5;i++){
        result += texture(uImage, vUV + dir*float(i)).rgb * weights[i];
        result += texture(uImage, vUV - dir*float(i)).rgb * weights[i];
    }
    FragColor = vec4(result,1.0);
})";

const char* compositeFragSrc = R"(
#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomStrength;
uniform float uExposure;
vec3 ACESFilm(vec3 x){
    float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
void main(){
    vec3 hdr = texture(uScene, vUV).rgb + texture(uBloom, vUV).rgb * uBloomStrength;
    hdr *= uExposure;
    vec3 mapped = ACESFilm(hdr);
    mapped = pow(mapped, vec3(1.0/2.2));
    FragColor = vec4(mapped, 1.0);
})";

enum class PrimitiveType { Cube, Sphere, Cylinder, Pyramid, Capsule, Plane, Model3D, Empty };
enum class GizmoMode { Select, Move, Rotate, Scale };
enum class GizmoAxis { None, X, Y, Z };
enum class SelectionType { None, Object, Light, Camera, Environment };

