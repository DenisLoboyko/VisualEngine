#include "CSM.h"
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <algorithm>

void CSM::CreateCascade(int idx) {
    CascadeLevel& c = cascades[idx];
    if (c.depthMap) glDeleteTextures(1, &c.depthMap);
    if (c.FBO) glDeleteFramebuffers(1, &c.FBO);
    glGenTextures(1, &c.depthMap);
    glBindTexture(GL_TEXTURE_2D, c.depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mapSize, mapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &c.FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, c.FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, c.depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CSM::Init(int cascadeCount, int size) {
    mapSize = size;
    cascades.resize(cascadeCount);
    for (int i = 0; i < cascadeCount; i++) CreateCascade(i);
}

void CSM::Resize(int w, int h) {}

void CSM::Update(const glm::vec3& lightDir, const glm::mat4& view, const glm::mat4& proj, float nearPlane, float farPlane) {
    int n = (int)cascades.size();
    std::vector<float> splits(n + 1);
    splits[0] = nearPlane; splits[n] = farPlane;
    for (int i = 1; i < n; i++) {
        float t = (float)i / n;
        float logS = nearPlane * pow(farPlane / nearPlane, t);
        float linS = nearPlane + (farPlane - nearPlane) * t;
        splits[i] = lambda * logS + (1.0f - lambda) * linS;
    }
    glm::mat4 invView = glm::inverse(view);
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec3 dir = glm::normalize(lightDir);
    glm::vec3 up = (fabs(dir.y) > 0.99f) ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
    for (int i = 0; i < n; i++) {
        auto ndcZ = [&](float d) { glm::vec4 p = proj * glm::vec4(0,0,-d,1); return p.z / p.w; };
        float z0 = ndcZ(splits[i]);
        float z1 = ndcZ(splits[i+1]);
        glm::vec4 cs[8]; int k = 0;
        for (int x = -1; x <= 1; x += 2) for (int y = -1; y <= 1; y += 2) {
            glm::vec4 a = invProj * glm::vec4((float)x,(float)y,z0,1.0f); a /= a.w; cs[k++] = a;
            glm::vec4 b = invProj * glm::vec4((float)x,(float)y,z1,1.0f); b /= b.w; cs[k++] = b;
        }
        glm::vec4 wc[8];
        glm::vec3 center(0);
        for (int j = 0; j < 8; j++) { wc[j] = invView * cs[j]; center += glm::vec3(wc[j]); }
        center /= 8.0f;
        glm::mat4 lightView = glm::lookAt(center + dir * 150.0f, center, up);
        glm::vec3 mn(1e9f), mx(-1e9f);
        for (int j = 0; j < 8; j++) {
            glm::vec3 p = glm::vec3(lightView * wc[j]);
            mn = glm::min(mn, p); mx = glm::max(mx, p);
        }
        float pad = 10.0f;
        float zn = std::max(0.1f, -mx.z - pad);
        float zf = -mn.z + pad;
        if (zf <= zn) zf = zn + 1.0f;
        glm::mat4 lightProj = glm::ortho(mn.x - pad, mx.x + pad, mn.y - pad, mx.y + pad, zn, zf);
        cascades[i].lightViewProj = lightProj * lightView;
        cascades[i].splitDepth = splits[i+1];
    }
}

void CSM::BindShadowMaps(GLuint prog, int startUnit) {
    int n = (int)cascades.size();
    float sp[8]; for (int i = 0; i < n; i++) sp[i] = cascades[i].splitDepth;
    glUniform1fv(glGetUniformLocation(prog, "cascadeSplits"), n, sp);
    for (int i = 0; i < n; i++) {
        glActiveTexture(GL_TEXTURE0 + startUnit + i);
        glBindTexture(GL_TEXTURE_2D, cascades[i].depthMap);
        std::string tn = "shadowMaps[" + std::to_string(i) + "]";
        glUniform1i(glGetUniformLocation(prog, tn.c_str()), startUnit + i);
        std::string mn2 = "cascadeVP[" + std::to_string(i) + "]";
        glUniformMatrix4fv(glGetUniformLocation(prog, mn2.c_str()), 1, GL_FALSE, &cascades[i].lightViewProj[0][0]);
    }
}

