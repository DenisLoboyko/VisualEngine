#pragma once
// в”Ђв”Ђ РСЃС…РѕРґРЅРёРєРё С€РµР№РґРµСЂРѕРІ (РІС‹РЅРµСЃРµРЅРѕ РёР· main.cpp) в”Ђв”Ђ

const char* vertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
out vec3 FragPos; out vec3 Normal; out vec2 TexCoord; out vec4 FragPosLightSpace;
uniform mat4 model,view,projection;
uniform mat4 lightSpaceMatrix;
void main(){
    FragPos=vec3(model*vec4(aPos,1.0));
    Normal=mat3(transpose(inverse(model)))*aNormal;
    TexCoord=aTexCoord;
    gl_Position=projection*view*vec4(FragPos,1.0);
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
})";

// в”Ђв”Ђ Skinned-РІРµСЂСЃРёСЏ РґР»СЏ РјРѕРґРµР»РµР№ СЃРѕ СЃРєРµР»РµС‚РЅРѕР№ Р°РЅРёРјР°С†РёРµР№ в”Ђв”Ђ
// РўРµ Р¶Рµ Р°С‚СЂРёР±СѓС‚С‹ + boneIDs/weights, РІРµСЂС€РёРЅР° СЃРјРµС€РёРІР°РµС‚СЃСЏ РјР°С‚СЂРёС†Р°РјРё РєРѕСЃС‚РµР№
// Р”Рћ РѕР±С‹С‡РЅРѕР№ model-РјР°С‚СЂРёС†С‹. Р¤СЂР°РіРјРµРЅС‚РЅС‹Р№ С€РµР№РґРµСЂ РѕР±С‰РёР№ СЃ РѕР±С‹С‡РЅС‹РјРё РѕР±СЉРµРєС‚Р°РјРё.
const char* vertSkinnedSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec4 aBoneIDs;   // РїСЂРёС…РѕРґСЏС‚ РєР°Рє float, РїСЂРёРІРѕРґРёРј Рє int
layout(location=4) in vec4 aWeights;
out vec3 FragPos; out vec3 Normal; out vec2 TexCoord; out vec4 FragPosLightSpace;
uniform mat4 model,view,projection;
const int MAX_BONES=100;
uniform mat4 boneMatrices[MAX_BONES];
uniform mat4 lightSpaceMatrix;
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
        FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection*view*vec4(FragPos,1.0);
})";;
const char* fragSrc = R"(
#version 330 core
in vec3 FragPos,Normal; in vec2 TexCoord; in vec4 FragPosLightSpace;
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
uniform vec3 ambientColor;
uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.0015 * (1.0 - dot(normal, lightDir)), 0.0004);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}
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
    vec3 result=ambientColor*baseColor;

    // в”Ђв”Ђ РќР°РїСЂР°РІР»РµРЅРЅС‹Р№ СЃРІРµС‚ СЃРѕР»РЅС†Р° вЂ” РѕСЃРІРµС‰Р°РµС‚ РІСЃСЋ СЃС†РµРЅСѓ РѕРґРёРЅР°РєРѕРІРѕ, РєР°Рє РІ СЂРµР°Р»СЊРЅРѕСЃС‚Рё в”Ђв”Ђ
    if (sunIntensity > 0.001) {
        float sunDiff = max(dot(norm, sunDir), 0.0);
        float sunSpec = pow(max(dot(vd, reflect(-sunDir,norm)),0.0), 48.0);
        float shadow = ShadowCalculation(FragPosLightSpace, norm, sunDir);
        result += (sunDiff*1.1 + sunSpec*0.4) * baseColor * sunColor * sunIntensity * (1.0 - shadow);
    }

    for(int i=0;i<lightCount;i++){
        vec3 ld=normalize(lightPos[i]-FragPos);
        float dist=length(lightPos[i]-FragPos);
        float att=clamp(1.0-dist/lightRange[i],0.0,1.0); att*=att;
        float diff=max(dot(norm,ld),0.0);
        float spec=pow(max(dot(vd,reflect(-ld,norm)),0.0),32);
        result+=(diff*0.8+spec*0.3)*baseColor*lightColor[i]*lightIntensity[i]*att;
    }
    // в”Ђв”Ђ Fresnel/rim-lighting: Р»С‘РіРєРѕРµ СЃРІРµС‡РµРЅРёРµ РїРѕ РєСЂР°СЋ РѕР±СЉРµРєС‚Р° РїРѕРґ РѕСЃС‚СЂС‹Рј СѓРіР»РѕРј Рє РєР°РјРµСЂРµ в”Ђв”Ђ
    float rim = pow(1.0 - max(dot(norm, vd), 0.0), 3.0);
    result += rim * 0.22 * mix(vec3(0.6), sunColor, clamp(sunIntensity,0.0,1.0)) * clamp(sunIntensity,0.0,1.0);

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
uniform mat4 lightSpaceMatrix;
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
uniform vec3 sunDir;          // РЅР°РїСЂР°РІР»РµРЅРёРµ РЅР° СЃРѕР»РЅС†Рµ (РЅРѕСЂРјР°Р»РёР·РѕРІР°РЅРѕ)
uniform float time;           // СЃРµРєСѓРЅРґС‹ (РїРѕРєР° РЅРµ РёСЃРїРѕР»СЊР·СѓРµС‚СЃСЏ, РѕСЃС‚Р°РІР»РµРЅРѕ РЅР° Р±СѓРґСѓС‰РµРµ)

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453123); }

void main(){
    vec3 dir = normalize(TexCoords);

    // в”Ђв”Ђ Р”РµРЅСЊ/РЅРѕС‡СЊ РїРѕ РІС‹СЃРѕС‚Рµ СЃРѕР»РЅС†Р° в”Ђв”Ђ
    float sunH   = sunDir.y;
    float dayF   = smoothstep(-0.2, 0.25, sunH);          // 0=РЅРѕС‡СЊ, 1=РґРµРЅСЊ
    float duskF  = clamp(1.0 - abs(sunH)*2.5, 0.0, 1.0);  // РїРёРє РЅР° РІРѕСЃС…РѕРґРµ/Р·Р°РєР°С‚Рµ

    vec3 dayZenith    = vec3(0.18, 0.38, 0.85);
    vec3 dayHorizon   = vec3(0.45, 0.58, 0.78);
    vec3 nightZenith  = vec3(0.010,0.012,0.035);
    vec3 nightHorizon = vec3(0.030,0.035,0.070);
    vec3 duskHorizon  = vec3(1.00, 0.55, 0.28);

    vec3 zenith  = mix(nightZenith,  dayZenith,  dayF);
    vec3 horizon = mix(nightHorizon, dayHorizon, dayF);
    horizon = mix(horizon, duskHorizon, duskF*0.75);

    float h = clamp(dir.y, -1.0, 1.0);
    float horizonBlend = smoothstep(-0.05, 0.18, max(h,0.0));
    float haze = smoothstep(-0.1, 0.05, h) * (1.0 - smoothstep(0.05, 0.25, h));
    vec3 skyColor = mix(horizon, zenith, horizonBlend);
    vec3 hazeColor = mix(horizon, vec3(0.60, 0.66, 0.76), 0.4);
    skyColor = mix(skyColor, hazeColor, haze * 0.3 * dayF);
    if (h < 0.0) {
        vec3 groundCol = vec3(0.20, 0.22, 0.25);
        skyColor = mix(groundCol, skyColor, smoothstep(-0.12, 0.0, h));
    }

    // в”Ђв”Ђ РЎРѕР»РЅС†Рµ в”Ђв”Ђ
    float sunDot = dot(dir, normalize(sunDir));
    float sunDisc = smoothstep(0.9993, 0.9998, sunDot);
    float sunGlow = pow(max(sunDot,0.0), 26.0) * 0.55;
    vec3 sunColor = mix(vec3(1.0,0.65,0.35), vec3(1.0,0.97,0.85), dayF);
    skyColor += (sunDisc*1.4 + sunGlow) * sunColor * step(-0.05, sunH);

    // в”Ђв”Ђ Р›СѓРЅР° (РїСЂРѕС‚РёРІРѕРїРѕР»РѕР¶РЅР° СЃРѕР»РЅС†Сѓ, РІРёРґРЅР° РЅРѕС‡СЊСЋ) в”Ђв”Ђ
    vec3 moonDir = -normalize(sunDir);
    float moonDot = dot(dir, moonDir);
    float moonDisc = smoothstep(0.9990, 0.9996, moonDot);
    float moonGlow = pow(max(moonDot,0.0), 40.0) * 0.15;
    skyColor += (moonDisc + moonGlow) * vec3(0.85,0.87,1.0) * (1.0-dayF);

    // в”Ђв”Ђ Р—РІС‘Р·РґС‹ РЅРѕС‡СЊСЋ вЂ” РјР°Р»РµРЅСЊРєРёРµ РєСЂСѓРіР»С‹Рµ С‚РѕС‡РєРё, РЅРµ С†РµР»С‹Рµ СЏС‡РµР№РєРё в”Ђв”Ђ
    float lon = atan(dir.z, dir.x);           // -pi..pi
    float lat = asin(clamp(dir.y,-1.0,1.0));  // -pi/2..pi/2
    vec2 starUV = vec2(lon, lat) * 120.0;
    vec2 starCell = floor(starUV);
    vec2 starLocal = fract(starUV) - 0.5;     // РїРѕР·РёС†РёСЏ РІРЅСѓС‚СЂРё СЏС‡РµР№РєРё, С†РµРЅС‚СЂ = (0,0)
    float starPick = hash(starCell);
    float hasStar  = step(0.985, starPick);
    // СЃР»СѓС‡Р°Р№РЅРѕРµ СЃРјРµС‰РµРЅРёРµ С‚РѕС‡РєРё РІРЅСѓС‚СЂРё СЏС‡РµР№РєРё, С‡С‚РѕР±С‹ РЅРµ Р±С‹Р»Рё СЃС‚СЂРѕРіРѕ РїРѕ СЃРµС‚РєРµ
    vec2 starOffset = vec2(hash(starCell+vec2(3.1,1.7)), hash(starCell+vec2(7.2,9.4))) - 0.5;
    float starDist  = length(starLocal - starOffset*0.4);
    float starDot   = smoothstep(0.16, 0.0, starDist) * hasStar;
    float twinkle   = 0.6 + 0.4*hash(starCell+vec2(time*0.0001,0.0));
    float stars = starDot * twinkle * (1.0-dayF) * smoothstep(0.0,0.3,h);
    skyColor += stars;

    FragColor = vec4(skyColor,1.0);
})";

// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
//   BLOOM: bright-pass + РіР°СѓСЃСЃРѕРІРѕ СЂР°Р·РјС‹С‚РёРµ + ACES tonemapping
// в•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђв•ђ
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
    float k = clamp((brightness-uThreshold)*2.0, 0.0, 1.0); // РјСЏРіРєРёР№ РїРµСЂРµС…РѕРґ, Р±РµР· Р¶С‘СЃС‚РєРѕРіРѕ РѕР±СЂРµР·Р°РЅРёСЏ
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


// ── Depth-only шейдеры для shadow map ──
const char* depthVertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
void main(){
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
})";

const char* depthFragSrc = R"(
#version 330 core
void main(){
    // Depth-only pass, ничего не рисуем
})";

// ── Depth-only шейдер для скиннутых моделей ──
const char* depthSkinnedVertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=3) in vec4 aBoneIDs;
layout(location=4) in vec4 aWeights;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;
const int MAX_BONES=100;
uniform mat4 boneMatrices[MAX_BONES];
void main(){
    vec4 skinnedPos = vec4(0.0);
    float totalWeight = 0.0;
    for(int i=0;i<4;i++){
        int id = int(aBoneIDs[i]);
        float w = aWeights[i];
        if(id<0 || w<=0.0) continue;
        mat4 bm = boneMatrices[id];
        skinnedPos += w * (bm * vec4(aPos,1.0));
        totalWeight += w;
    }
    if(totalWeight < 0.001){ skinnedPos = vec4(aPos,1.0); }
    gl_Position = lightSpaceMatrix * model * skinnedPos;
})";








