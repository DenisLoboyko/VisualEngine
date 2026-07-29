#pragma once
// =========================================================
//  Model.h — загрузка 3D-моделей через Assimp + скелетная анимация
//
//  Умеет: FBX/glTF/OBJ/DAE и др. форматы, чтение костей (skinning
//  weights), чтение клипов анимации из файла и сэмплинг позы кости
//  на произвольный момент времени (с интерполяцией между кадрами).
//
//  Публичный интерфейс для статичных моделей (Load/Draw/loaded)
//  не менялся. Новое — hasSkeleton, animations[], GetBoneMatrices().
// =========================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <algorithm>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace VE {

    constexpr int MAX_BONE_INFLUENCE = 4;
    constexpr int MAX_BONES = 100;

    inline glm::mat4 AiToGlm(const aiMatrix4x4& m) {
        glm::mat4 r;
        r[0][0]=m.a1; r[1][0]=m.a2; r[2][0]=m.a3; r[3][0]=m.a4;
        r[0][1]=m.b1; r[1][1]=m.b2; r[2][1]=m.b3; r[3][1]=m.b4;
        r[0][2]=m.c1; r[1][2]=m.c2; r[2][2]=m.c3; r[3][2]=m.c4;
        r[0][3]=m.d1; r[1][3]=m.d2; r[2][3]=m.d3; r[3][3]=m.d4;
        return r;
    }

    struct BoneInfo { int id; glm::mat4 offset; };

    struct KeyPos { float time; glm::vec3 value; };
    struct KeyRot { float time; glm::quat value; };
    struct KeyScl { float time; glm::vec3 value; };

    struct BoneAnimChannel {
        std::string nodeName;
        std::vector<KeyPos> positions;
        std::vector<KeyRot> rotations;
        std::vector<KeyScl> scales;
    };

    struct AnimationClip {
        std::string name;
        float duration = 0.f;       // в тиках
        float ticksPerSecond = 25.f;
        std::vector<BoneAnimChannel> channels; // по одному на анимированный узел
    };

    // Узел иерархии скелета — с бинд-позой (transform по умолчанию,
    // если для этого узла нет анимационного канала в текущем клипе)
    struct SkeletonNode {
        std::string name;
        glm::mat4 bindTransform{1.0f};
        std::vector<SkeletonNode> children;
    };

    class Model
    {
    public:
        unsigned int VAO = 0, VBO = 0;
        int vertexCount = 0;
        bool loaded = false;
        glm::vec3 boundsMin{0}, boundsMax{0};

        // ── Скелетная анимация ──
        bool hasSkeleton = false;
        std::unordered_map<std::string, BoneInfo> boneInfoMap;
        int boneCounter = 0;
        SkeletonNode rootNode;
        glm::mat4 globalInverseTransform{1.0f};
        std::vector<AnimationClip> animations;

        bool Load(const std::string& path)
        {
            Assimp::Importer importer;
            const aiScene* scene = importer.ReadFile(
                path,
                aiProcess_Triangulate |
                aiProcess_GenSmoothNormals |
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices |
                aiProcess_ImproveCacheLocality |
                aiProcess_LimitBoneWeights |
                aiProcess_FlipUVs
            );

            if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
                std::cerr << "Assimp failed to load model '" << path << "': "
                          << importer.GetErrorString() << "\n";
                return false;
            }

            globalInverseTransform = glm::inverse(AiToGlm(scene->mRootNode->mTransformation));

            struct VTmp {
                glm::vec3 pos, normal;
                glm::vec2 uv;
                int   boneIDs[MAX_BONE_INFLUENCE]  = {-1,-1,-1,-1};
                float weights[MAX_BONE_INFLUENCE]  = {0,0,0,0};
            };
            std::vector<VTmp> vtmp;
            std::vector<unsigned int> indices;

            bool first = true;

            auto addBoneWeight = [](VTmp& v, int boneID, float weight){
                for (int slot=0; slot<MAX_BONE_INFLUENCE; slot++) {
                    if (v.boneIDs[slot] < 0) { v.boneIDs[slot]=boneID; v.weights[slot]=weight; return; }
                }
            };

            std::function<void(aiNode*)> processNode = [&](aiNode* node){
                for (unsigned int i=0; i<node->mNumMeshes; i++) {
                    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                    unsigned int base = (unsigned int)vtmp.size();

                    for (unsigned int v=0; v<mesh->mNumVertices; v++) {
                        VTmp vt;
                        vt.pos    = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
                        vt.normal = mesh->HasNormals()
                            ? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z)
                            : glm::vec3(0,1,0);
                        vt.uv     = mesh->HasTextureCoords(0)
                            ? glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y)
                            : glm::vec2(0,0);
                        vtmp.push_back(vt);

                        if (first) { boundsMin=boundsMax=vt.pos; first=false; }
                        else { boundsMin=glm::min(boundsMin,vt.pos); boundsMax=glm::max(boundsMax,vt.pos); }
                    }

                    for (unsigned int b=0; b<mesh->mNumBones; b++) {
                        aiBone* bone = mesh->mBones[b];
                        std::string boneName = bone->mName.C_Str();
                        int boneID;
                        auto it = boneInfoMap.find(boneName);
                        if (it == boneInfoMap.end()) {
                            BoneInfo info; info.id=boneCounter; info.offset=AiToGlm(bone->mOffsetMatrix);
                            boneInfoMap[boneName]=info;
                            boneID = boneCounter;
                            boneCounter++;
                        } else boneID = it->second.id;

                        for (unsigned int w=0; w<bone->mNumWeights; w++) {
                            unsigned int vId = base + bone->mWeights[w].mVertexId;
                            if (vId < vtmp.size()) addBoneWeight(vtmp[vId], boneID, bone->mWeights[w].mWeight);
                        }
                    }
                    if (mesh->mNumBones > 0) hasSkeleton = true;

                    for (unsigned int f=0; f<mesh->mNumFaces; f++) {
                        aiFace& face = mesh->mFaces[f];
                        if (face.mNumIndices != 3) continue;
                        for (unsigned int k=0;k<3;k++) indices.push_back(base+face.mIndices[k]);
                    }
                }
                for (unsigned int i=0;i<node->mNumChildren;i++) processNode(node->mChildren[i]);
            };
            processNode(scene->mRootNode);

            if (vtmp.empty() || indices.empty()) {
                std::cerr << "Model has no triangles: " << path << "\n";
                return false;
            }

            // ── Плоский буфер: pos(3) normal(3) uv(2) boneIDs(4) weights(4) = 16 float/vertex ──
            std::vector<float> verts;
            verts.reserve(indices.size()*16);
            for (unsigned int idx : indices) {
                VTmp& v = vtmp[idx];
                verts.insert(verts.end(), { v.pos.x,v.pos.y,v.pos.z, v.normal.x,v.normal.y,v.normal.z, v.uv.x,v.uv.y });
                for (int s=0;s<MAX_BONE_INFLUENCE;s++) verts.push_back((float)v.boneIDs[s]);
                for (int s=0;s<MAX_BONE_INFLUENCE;s++) verts.push_back(v.weights[s]);
            }
            vertexCount = (int)indices.size();

            // ── Иерархия узлов скелета (бинд-поза + имена под анимацию) ──
            std::function<void(aiNode*, SkeletonNode&)> buildSkeleton = [&](aiNode* node, SkeletonNode& out){
                out.name = node->mName.C_Str();
                out.bindTransform = AiToGlm(node->mTransformation);
                out.children.resize(node->mNumChildren);
                for (unsigned int i=0;i<node->mNumChildren;i++) buildSkeleton(node->mChildren[i], out.children[i]);
            };
            buildSkeleton(scene->mRootNode, rootNode);

            // ── Клипы анимации ──
            for (unsigned int a=0; a<scene->mNumAnimations; a++) {
                aiAnimation* anim = scene->mAnimations[a];
                AnimationClip clip;
                clip.name = anim->mName.length>0 ? anim->mName.C_Str() : ("Anim"+std::to_string(a));
                clip.duration = (float)anim->mDuration;
                clip.ticksPerSecond = anim->mTicksPerSecond!=0 ? (float)anim->mTicksPerSecond : 25.f;
                for (unsigned int c=0; c<anim->mNumChannels; c++) {
                    aiNodeAnim* ch = anim->mChannels[c];
                    BoneAnimChannel bc; bc.nodeName = ch->mNodeName.C_Str();
                    for (unsigned int k=0;k<ch->mNumPositionKeys;k++){
                        auto& key=ch->mPositionKeys[k];
                        bc.positions.push_back({(float)key.mTime, glm::vec3(key.mValue.x,key.mValue.y,key.mValue.z)});
                    }
                    for (unsigned int k=0;k<ch->mNumRotationKeys;k++){
                        auto& key=ch->mRotationKeys[k];
                        bc.rotations.push_back({(float)key.mTime, glm::quat(key.mValue.w,key.mValue.x,key.mValue.y,key.mValue.z)});
                    }
                    for (unsigned int k=0;k<ch->mNumScalingKeys;k++){
                        auto& key=ch->mScalingKeys[k];
                        bc.scales.push_back({(float)key.mTime, glm::vec3(key.mValue.x,key.mValue.y,key.mValue.z)});
                    }
                    clip.channels.push_back(bc);
                }
                animations.push_back(clip);
            }

            if (VAO) glDeleteVertexArrays(1,&VAO);
            if (VBO) glDeleteBuffers(1,&VBO);

            glGenVertexArrays(1,&VAO);
            glGenBuffers(1,&VBO);
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER,VBO);
            glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
            int stride = 16*sizeof(float);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0);                    glEnableVertexAttribArray(0);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float)));     glEnableVertexAttribArray(1);
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float)));     glEnableVertexAttribArray(2);
            glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,stride,(void*)(8*sizeof(float)));     glEnableVertexAttribArray(3); // boneIDs (как float, приводим в шейдере)
            glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,stride,(void*)(12*sizeof(float)));    glEnableVertexAttribArray(4); // weights
            glBindVertexArray(0);

            loaded = true;
            std::cout << "Model loaded via Assimp: " << path << " (" << vertexCount << " verts, "
                       << scene->mNumMeshes << " mesh(es), " << boneCounter << " bones, "
                       << animations.size() << " animation(s))\n";
            return true;
        }

        void Draw() {
            if (!loaded) return;
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        }

        // ── Считает финальные матрицы костей для клипа animIndex в момент timeSec ──
        // Результат — массив до MAX_BONES матриц, готовый к заливке в шейдер.
        std::vector<glm::mat4> GetBoneMatrices(int animIndex, float timeSec, bool loop)
        {
            std::vector<glm::mat4> result(std::max(1, boneCounter), glm::mat4(1.0f));
            if (animIndex < 0 || animIndex >= (int)animations.size()) return result;

            AnimationClip& clip = animations[animIndex];
            float tps = clip.ticksPerSecond > 0 ? clip.ticksPerSecond : 25.f;
            float timeInTicks = timeSec * tps;
            float animTime = clip.duration > 0.f
                ? (loop ? fmodf(timeInTicks, clip.duration) : std::min(timeInTicks, clip.duration))
                : 0.f;

            std::function<void(const SkeletonNode&, const glm::mat4&)> walk =
                [&](const SkeletonNode& node, const glm::mat4& parentGlobal){
                glm::mat4 nodeTransform = node.bindTransform;

                // Если для этого узла есть анимационный канал — берём интерполированную позу
                for (auto& ch : clip.channels) {
                    if (ch.nodeName == node.name) {
                        glm::vec3 pos  = SamplePos(ch, animTime);
                        glm::quat rot  = SampleRot(ch, animTime);
                        glm::vec3 scl  = SampleScl(ch, animTime);
                        glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
                        glm::mat4 R = glm::mat4_cast(rot);
                        glm::mat4 S = glm::scale(glm::mat4(1.0f), scl);
                        nodeTransform = T * R * S;
                        break;
                    }
                }

                glm::mat4 globalTransform = parentGlobal * nodeTransform;

                auto it = boneInfoMap.find(node.name);
                if (it != boneInfoMap.end()) {
                    int id = it->second.id;
                    if (id >= 0 && id < (int)result.size())
                        result[id] = globalInverseTransform * globalTransform * it->second.offset;
                }

                for (auto& child : node.children) walk(child, globalTransform);
            };
            walk(rootNode, glm::mat4(1.0f));

            return result;
        }

        ~Model() {
            if (loaded) { glDeleteVertexArrays(1,&VAO); glDeleteBuffers(1,&VBO); }
        }

    private:
        static glm::vec3 SamplePos(BoneAnimChannel& ch, float t) {
            if (ch.positions.empty()) return glm::vec3(0);
            if (ch.positions.size()==1) return ch.positions[0].value;
            for (size_t i=0;i+1<ch.positions.size();i++) {
                if (t < ch.positions[i+1].time) {
                    float span = ch.positions[i+1].time - ch.positions[i].time;
                    float f = span>0.f ? (t-ch.positions[i].time)/span : 0.f;
                    return glm::mix(ch.positions[i].value, ch.positions[i+1].value, glm::clamp(f,0.f,1.f));
                }
            }
            return ch.positions.back().value;
        }
        static glm::quat SampleRot(BoneAnimChannel& ch, float t) {
            if (ch.rotations.empty()) return glm::quat(1,0,0,0);
            if (ch.rotations.size()==1) return ch.rotations[0].value;
            for (size_t i=0;i+1<ch.rotations.size();i++) {
                if (t < ch.rotations[i+1].time) {
                    float span = ch.rotations[i+1].time - ch.rotations[i].time;
                    float f = span>0.f ? (t-ch.rotations[i].time)/span : 0.f;
                    return glm::slerp(ch.rotations[i].value, ch.rotations[i+1].value, glm::clamp(f,0.f,1.f));
                }
            }
            return ch.rotations.back().value;
        }
        static glm::vec3 SampleScl(BoneAnimChannel& ch, float t) {
            if (ch.scales.empty()) return glm::vec3(1);
            if (ch.scales.size()==1) return ch.scales[0].value;
            for (size_t i=0;i+1<ch.scales.size();i++) {
                if (t < ch.scales[i+1].time) {
                    float span = ch.scales[i+1].time - ch.scales[i].time;
                    float f = span>0.f ? (t-ch.scales[i].time)/span : 0.f;
                    return glm::mix(ch.scales[i].value, ch.scales[i+1].value, glm::clamp(f,0.f,1.f));
                }
            }
            return ch.scales.back().value;
        }
    };
}