#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include "imgui.h"

struct Sprite2D {
    std::string name = "Sprite2D";
    glm::vec2 pos = glm::vec2(0.f);
    float z = 0.f;
    glm::vec2 scale = glm::vec2(1.f);
    glm::vec4 color = glm::vec4(1.f);
    GLuint tex = 0;
    std::string texPath;
    bool flipX = false, flipY = false;
    bool visible = true;
};

namespace VE2D {
    inline GLuint sh2D=0, q2DVAO=0, q2DVBO=0, whiteTex=0;
    inline GLuint fbFBO=0, fbTex=0; inline int fbW=0, fbH=0;
    inline bool mode2D = false;

    inline void Init() {
        const char* vs = "#version 330 core\n"
            "layout(location=0) in vec2 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "uniform mat4 uProj; uniform vec2 uPos; uniform vec2 uScale; uniform vec2 uFlip;\n"
            "out vec2 vUV;\n"
            "void main(){ vUV = vec2(uFlip.x>0.5?1.0-aUV.x:aUV.x, uFlip.y>0.5?1.0-aUV.y:aUV.y);\n"
            " vec2 p = aPos*uScale + uPos; gl_Position = uProj*vec4(p,0.0,1.0); }";
        const char* fs = "#version 330 core\n"
            "in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor; out vec4 oC;\n"
            "void main(){ oC = texture(uTex,vUV)*uColor; }";
        auto compile=[](GLenum t,const char* src)->GLuint{
            GLuint s=glCreateShader(t); glShaderSource(s,1,&src,nullptr); glCompileShader(s); return s; };
        GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs);
        sh2D=glCreateProgram(); glAttachShader(sh2D,v); glAttachShader(sh2D,f); glLinkProgram(sh2D);
        glDeleteShader(v); glDeleteShader(f);
        float quad[] = { -0.5f,-0.5f, 0,0,  0.5f,-0.5f, 1,0,  0.5f,0.5f, 1,1,  -0.5f,-0.5f, 0,0,  0.5f,0.5f, 1,1,  -0.5f,0.5f, 0,1 };
        glGenVertexArrays(1,&q2DVAO); glGenBuffers(1,&q2DVBO);
        glBindVertexArray(q2DVAO); glBindBuffer(GL_ARRAY_BUFFER,q2DVBO);
        glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
        unsigned char px[64]; for(int i=0;i<64;i++) px[i]=255;
        glGenTextures(1,&whiteTex); glBindTexture(GL_TEXTURE_2D,whiteTex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }

    inline void EnsureFBO(int w,int h) {
        if (w<8||h<8) return;
        if (fbW==w && fbH==h && fbFBO) return;
        fbW=w; fbH=h;
        if (!fbFBO) glGenFramebuffers(1,&fbFBO);
        if (!fbTex) glGenTextures(1,&fbTex);
        glBindTexture(GL_TEXTURE_2D,fbTex);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER,fbFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fbTex,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }

    inline void Draw(std::vector<Sprite2D>& sprites, float aspect) {
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(sh2D);
        glm::mat4 proj = glm::ortho(-aspect, aspect, -1.f, 1.f, -100.f, 100.f);
        glUniformMatrix4fv(glGetUniformLocation(sh2D,"uProj"),1,GL_FALSE,&proj[0][0]);
        std::vector<int> order(sprites.size());
        for (size_t i=0;i<sprites.size();i++) order[i]=(int)i;
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){ return sprites[a].z < sprites[b].z; });
        glBindVertexArray(q2DVAO);
        for (int idx : order) {
            Sprite2D& s = sprites[idx];
            if (!s.visible) continue;
            glUniform2f(glGetUniformLocation(sh2D,"uPos"), s.pos.x, s.pos.y);
            glUniform2f(glGetUniformLocation(sh2D,"uScale"), s.scale.x, s.scale.y);
            glUniform2f(glGetUniformLocation(sh2D,"uFlip"), s.flipX?1.f:0.f, s.flipY?1.f:0.f);
            glUniform4f(glGetUniformLocation(sh2D,"uColor"), s.color.r, s.color.g, s.color.b, s.color.a);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s.tex ? s.tex : whiteTex);
            glUniform1i(glGetUniformLocation(sh2D,"uTex"),0);
            glDrawArrays(GL_TRIANGLES,0,6);
        }
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
    }

    inline void RenderFrame(std::vector<Sprite2D>& sprites, int w, int h) {
        EnsureFBO(w,h);
        if (!fbFBO) return;
        glBindFramebuffer(GL_FRAMEBUFFER,fbFBO);
        glViewport(0,0,w,h);
        glClearColor(0.09f,0.09f,0.11f,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        Draw(sprites, (float)w/(float)h);
        glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
}
