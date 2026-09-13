#pragma once
#include <vector>
#include <map>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>

namespace VE {

struct InstCube { glm::vec3 pos; glm::vec3 scale; glm::vec2 uv; GLuint tex; bool active; };

class InstanceRenderer {
public:
    std::vector<InstCube> items;
    std::vector<int> freeList;
    bool dirty = true;
    GLuint prog = 0, vao = 0, vbo = 0, ebo = 0;
    std::map<GLuint, std::vector<float>> cpuBuf;
    std::map<GLuint, GLuint> ivbo;
    std::map<GLuint, int> counts;

    static InstanceRenderer& Get(){ static InstanceRenderer r; return r; }

    int Add(GLuint tex, const glm::vec3& p, const glm::vec3& s, const glm::vec2& uv) {
        int id;
        if (!freeList.empty()) { id = freeList.back(); freeList.pop_back(); items[id] = { p, s, uv, tex, true }; }
        else { id = (int)items.size(); items.push_back({ p, s, uv, tex, true }); }
        dirty = true; return id;
    }
    void Remove(int id) { if (id >= 0 && id < (int)items.size() && items[id].active) { items[id].active = false; freeList.push_back(id); dirty = true; } }
    void SetActive(int id, bool a) { if (id >= 0 && id < (int)items.size()) { items[id].active = a; dirty = true; } }
    void Clear() { items.clear(); freeList.clear(); dirty = true; }
    int Count() const { return (int)items.size() - (int)freeList.size(); }

    void Init() {
        if (prog) return;
        const char* vs = "#version 330 core\nlayout(location=0) in vec3 aPos;\nlayout(location=1) in vec3 aN;\nlayout(location=2) in vec2 aUV;\nlayout(location=3) in vec3 iPos;\nlayout(location=4) in vec3 iScl;\nlayout(location=5) in vec2 iUV;\nuniform mat4 view,proj;\nout vec2 uv;\nout vec3 fn;\nvoid main(){\n  vec3 p=aPos*iScl+iPos;\n  uv=aUV*mix(vec2(1.0,iScl.y),vec2(1.0),step(0.5,abs(aN.y)))*iUV;\n  fn=aN;\n  gl_Position=proj*view*vec4(p,1.0);\n}\n";
        const char* fs = "#version 330 core\nin vec2 uv;\nin vec3 fn;\nuniform sampler2D tex;\nuniform vec3 sunDir;\nuniform vec3 sunCol;\nuniform float sunInt;\nuniform vec3 amb;\nout vec4 FC;\nvoid main(){\n  vec4 c=texture(tex,uv);\n  float d=max(dot(normalize(fn),normalize(sunDir)),0.0);\n  vec3 lit=amb+sunCol*sunInt*d;\n  FC=vec4(c.rgb*lit,c.a);\n}\n";
        GLuint v=glCreateShader(GL_VERTEX_SHADER), f2=glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(v,1,&vs,0); glCompileShader(v);
        glShaderSource(f2,1,&fs,0); glCompileShader(f2);
        prog=glCreateProgram(); glAttachShader(prog,v); glAttachShader(prog,f2); glLinkProgram(prog);
        glDeleteShader(v); glDeleteShader(f2);
        float cv[]={
            -0.5f,-0.5f,-0.5f,0,0,-1,0,0, 0.5f,-0.5f,-0.5f,0,0,-1,1,0, 0.5f,0.5f,-0.5f,0,0,-1,1,1, -0.5f,0.5f,-0.5f,0,0,-1,0,1,
            -0.5f,-0.5f,0.5f,0,0,1,0,0, 0.5f,-0.5f,0.5f,0,0,1,1,0, 0.5f,0.5f,0.5f,0,0,1,1,1, -0.5f,0.5f,0.5f,0,0,1,0,1,
            -0.5f,-0.5f,-0.5f,-1,0,0,0,0, -0.5f,0.5f,-0.5f,-1,0,0,1,0, -0.5f,0.5f,0.5f,-1,0,0,1,1, -0.5f,-0.5f,0.5f,-1,0,0,0,1,
            0.5f,-0.5f,-0.5f,1,0,0,0,0, 0.5f,0.5f,-0.5f,1,0,0,1,0, 0.5f,0.5f,0.5f,1,0,0,1,1, 0.5f,-0.5f,0.5f,1,0,0,0,1,
            -0.5f,-0.5f,-0.5f,0,-1,0,0,0, 0.5f,-0.5f,-0.5f,0,-1,0,1,0, 0.5f,-0.5f,0.5f,0,-1,0,1,1, -0.5f,-0.5f,0.5f,0,-1,0,0,1,
            -0.5f,0.5f,-0.5f,0,1,0,0,0, 0.5f,0.5f,-0.5f,0,1,0,1,0, 0.5f,0.5f,0.5f,0,1,0,1,1, -0.5f,0.5f,0.5f,0,1,0,0,1
        };
        unsigned int ci[]={0,1,2,2,3,0,4,5,6,6,7,4,8,9,10,10,11,8,12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20};
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER,vbo); glBufferData(GL_ARRAY_BUFFER,sizeof(cv),cv,GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(ci),ci,GL_STATIC_DRAW);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,32,(void*)0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,32,(void*)12); glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,32,(void*)24); glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

    void Rebuild() {
        if (!dirty) return; dirty = false;
        cpuBuf.clear(); counts.clear();
        for (auto& it : items) {
            if (!it.active) continue;
            auto& b = cpuBuf[it.tex];
            b.push_back(it.pos.x); b.push_back(it.pos.y); b.push_back(it.pos.z);
            b.push_back(it.scale.x); b.push_back(it.scale.y); b.push_back(it.scale.z);
            b.push_back(it.uv.x); b.push_back(it.uv.y);
            counts[it.tex]++;
        }
        for (auto& kv : cpuBuf) {
            if (ivbo.find(kv.first) == ivbo.end()) { GLuint b; glGenBuffers(1,&b); ivbo[kv.first]=b; }
            glBindBuffer(GL_ARRAY_BUFFER, ivbo[kv.first]);
            glBufferData(GL_ARRAY_BUFFER, kv.second.size()*4, kv.second.data(), GL_DYNAMIC_DRAW);
        }
    }

    void Render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& sunDir, const glm::vec3& sunCol, float sunInt, const glm::vec3& amb) {
        if (items.empty()) return;
        Init(); Rebuild();
        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog,"view"),1,GL_FALSE,glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog,"proj"),1,GL_FALSE,glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(prog,"sunDir"),1,glm::value_ptr(sunDir));
        glUniform3fv(glGetUniformLocation(prog,"sunCol"),1,glm::value_ptr(sunCol));
        glUniform1f(glGetUniformLocation(prog,"sunInt"),sunInt);
        glUniform3fv(glGetUniformLocation(prog,"amb"),1,glm::value_ptr(amb));
        glUniform1i(glGetUniformLocation(prog,"tex"),0);
        glBindVertexArray(vao);
        for (auto& kv : counts) {
            if (kv.second <= 0) continue;
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, kv.first);
            glBindBuffer(GL_ARRAY_BUFFER, ivbo[kv.first]);
            glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,32,(void*)0); glEnableVertexAttribArray(3); glVertexAttribDivisor(3,1);
            glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,32,(void*)12); glEnableVertexAttribArray(4); glVertexAttribDivisor(4,1);
            glVertexAttribPointer(5,2,GL_FLOAT,GL_FALSE,32,(void*)24); glEnableVertexAttribArray(5); glVertexAttribDivisor(5,1);
            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0, kv.second);
        }
        glVertexAttribDivisor(3,0); glVertexAttribDivisor(4,0); glVertexAttribDivisor(5,0);
        glBindVertexArray(0);
    }
};
}
