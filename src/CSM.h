#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct CascadeLevel {
    GLuint FBO = 0;
    GLuint depthMap = 0;
    float splitDepth = 0.0f;
    glm::mat4 lightViewProj;
    glm::vec2 uvScale;
    glm::vec2 uvOffset;
};

class CSM {
public:
    void Init(int cascadeCount = 4, int mapSize = 2048);
    void Resize(int w, int h);
    void Update(const glm::vec3& lightDir, const glm::mat4& cameraView, const glm::mat4& cameraProj, float nearPlane, float farPlane);
    void BindShadowMaps(GLuint shaderProg, int startUnit = 5);
    const std::vector<CascadeLevel>& GetCascades() const { return cascades; }
    int GetCascadeCount() const { return (int)cascades.size(); }
    void SetDebug(bool d) { debug = d; }
    bool IsDebug() const { return debug; }
    
private:
    std::vector<CascadeLevel> cascades;
    int mapSize = 2048;
    bool debug = false;
    float lambda = 0.5f; // PSSM lambda (0=linear, 1=logarithmic)
    
    void CreateCascade(int idx);
    glm::mat4 ComputeLightViewProj(const glm::vec3& lightDir, const glm::mat4& cameraView, float nearSplit, float farSplit);
};
