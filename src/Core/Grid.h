#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace VE {

    class Grid
    {
    public:
        unsigned int VAO = 0, VBO = 0;
        int vertexCount = 0;
        unsigned int prog = 0;

        Grid(int size = 20)
        {
            (void)size;
            glGenVertexArrays(1, &VAO);

            const char* vsSrc = R"(#version 330 core
                out vec2 vUV;
                void main()
                {
                    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
                    vUV = p;
                    gl_Position = vec4(p * 2.0 - 1.0, 0.999999, 1.0);
                })";

            const char* fsSrc = R"(#version 330 core
                in vec2 vUV;
                uniform mat4 uInvView;
                uniform mat4 uInvProj;
                uniform vec3 uCamPos;
                uniform vec3 uGridColor;
                out vec4 outColor;

                float lineMask(vec2 p, float scale)
                {
                    vec2 q = p / scale;
                    vec2 w = max(fwidth(q), vec2(1e-4));
                    vec2 d = abs(fract(q - 0.5) - 0.5) / w;
                    return 1.0 - min(min(d.x, d.y), 1.0);
                }

                void main()
                {
                    vec4 ndc = vec4(vUV * 2.0 - 1.0, 1.0, 1.0);
                    vec4 vd = uInvProj * ndc;
                    vd.xyz /= vd.w; vd.w = 0.0;
                    vec3 dir = normalize((uInvView * vd).xyz);
                    if (dir.y > -1e-4) discard;

                    float t = -uCamPos.y / dir.y;
                    vec3 wp = uCamPos + dir * t;

                    float dist = length(wp.xz - uCamPos.xz);
                    float fade = 1.0 - smoothstep(80.0, 300.0, dist);
                    fade *= smoothstep(0.0, 0.015, -dir.y);
                    if (fade <= 0.001) discard;

                    float minor = lineMask(wp.xz, 1.0);
                    float major = lineMask(wp.xz, 10.0);

                    vec2 aw = max(fwidth(wp.xz), vec2(1e-4));
                    float axX = 1.0 - min(abs(wp.z) / (aw.y * 2.0), 1.0);
                    float axZ = 1.0 - min(abs(wp.x) / (aw.x * 2.0), 1.0);

                    vec3 col = uGridColor;
                    col = mix(col, vec3(0.85, 0.35, 0.35), clamp(axX, 0.0, 1.0) * 0.8);
                    col = mix(col, vec3(0.35, 0.55, 0.90), clamp(axZ, 0.0, 1.0) * 0.8);

                    float a = clamp(minor * 0.30 + major * 0.45 + max(axX, axZ) * 0.85, 0.0, 1.0) * fade;
                    outColor = vec4(col, a);
                })";

            auto compile = [](GLenum type, const char* src) -> GLuint {
                GLuint s = glCreateShader(type);
                glShaderSource(s, 1, &src, nullptr);
                glCompileShader(s);
                return s;
            };
            GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
            GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
            prog = glCreateProgram();
            glAttachShader(prog, vs);
            glAttachShader(prog, fs);
            glLinkProgram(prog);
            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        void Draw(unsigned int shaderID, const glm::mat4& view, const glm::mat4& proj)
        {
            (void)shaderID;
            if (!prog) return;
            glUseProgram(prog);

            glm::mat4 invV = glm::inverse(view);
            glm::mat4 invP = glm::inverse(proj);
            glUniformMatrix4fv(glGetUniformLocation(prog, "uInvView"), 1, GL_FALSE, glm::value_ptr(invV));
            glUniformMatrix4fv(glGetUniformLocation(prog, "uInvProj"), 1, GL_FALSE, glm::value_ptr(invP));
            glm::vec3 camPos(invV[3]);
            glUniform3f(glGetUniformLocation(prog, "uCamPos"), camPos.x, camPos.y, camPos.z);
            glUniform3f(glGetUniformLocation(prog, "uGridColor"), 0.30f, 0.31f, 0.35f);

            GLboolean blendWas = glIsEnabled(GL_BLEND);
            GLboolean depthMask = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            if (!blendWas) glDisable(GL_BLEND);
            glDepthMask(depthMask);
        }

        ~Grid()
        {
            if (prog) glDeleteProgram(prog);
            if (VAO) glDeleteVertexArrays(1, &VAO);
            if (VBO) glDeleteBuffers(1, &VBO);
        }
    };
}
