#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace VE {
    class ShadowMap {
    public:
        unsigned int FBO = 0;
        unsigned int depthMap = 0;
        int width = 2048, height = 2048;

        ShadowMap(int w = 4096, int h = 4096) : width(w), height(h) {
            glGenFramebuffers(1, &FBO);
            glGenTextures(1, &depthMap);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
            
            glBindFramebuffer(GL_FRAMEBUFFER, FBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        glm::mat4 GetLightSpaceMatrix(const glm::vec3& lightDir, const glm::vec3& target = glm::vec3(0.0f)) {
            glm::vec3 lightPos = target + lightDir * 50.0f;
            glm::vec3 up = glm::abs(lightDir.y) > 0.95f ? glm::vec3(0.0f,0.0f,1.0f) : glm::vec3(0.0f,1.0f,0.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, target, up);
            glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 150.0f);
            return lightProjection * lightView;
        }

        ~ShadowMap() {
            if (FBO) glDeleteFramebuffers(1, &FBO);
            if (depthMap) glDeleteTextures(1, &depthMap);
        }
    };
}



