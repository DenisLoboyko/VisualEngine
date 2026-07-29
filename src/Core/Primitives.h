#pragma once
#include <glad/glad.h>
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =========================================================
//  Primitives.h  —  формат вершин: pos(3) + normal(3) + uv(2)
//  stride = 8 * sizeof(float)
//  location 0 = aPos, location 1 = aNormal, location 2 = aTexCoord
// =========================================================

namespace VE {

    struct Mesh {
        unsigned int VAO, VBO, EBO;
        int indexCount;
        int vertexCount;
        bool useEBO;
    };

    // Вспомогательная — залить VAO по формату pos+normal+uv (stride 8)
    inline void SetupMeshVAO(Mesh& m,
                              const std::vector<float>& verts,
                              const std::vector<unsigned int>& indices)
    {
        m.useEBO     = true;
        m.indexCount  = (int)indices.size();
        m.vertexCount = 0;

        glGenVertexArrays(1, &m.VAO);
        glGenBuffers(1, &m.VBO);
        glGenBuffers(1, &m.EBO);
        glBindVertexArray(m.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        const int stride = 8 * sizeof(float);
        // location 0 — позиция
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        // location 1 — нормаль
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
        glEnableVertexAttribArray(1);
        // location 2 — UV
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    // ─────────────────────────────────────────────────────
    //  Сфера
    // ─────────────────────────────────────────────────────
    inline Mesh CreateSphere(int stacks = 24, int slices = 24)
    {
        std::vector<float> verts;
        std::vector<unsigned int> indices;

        for (int i = 0; i <= stacks; i++) {
            float phi = M_PI * i / stacks;
            for (int j = 0; j <= slices; j++) {
                float theta = 2.0f * M_PI * j / slices;
                float x = sin(phi) * cos(theta);
                float y = cos(phi);
                float z = sin(phi) * sin(theta);
                float u = (float)j / slices;
                float v = (float)i / stacks;
                verts.insert(verts.end(), {x*0.5f, y*0.5f, z*0.5f, x, y, z, u, v});
            }
        }

        for (int i = 0; i < stacks; i++) {
            for (int j = 0; j < slices; j++) {
                int a = i*(slices+1)+j, b = a+slices+1;
                indices.insert(indices.end(), {
                    (unsigned)a,(unsigned)b,(unsigned)(a+1),
                    (unsigned)b,(unsigned)(b+1),(unsigned)(a+1)
                });
            }
        }

        Mesh m;
        SetupMeshVAO(m, verts, indices);
        return m;
    }

    // ─────────────────────────────────────────────────────
    //  Цилиндр
    // ─────────────────────────────────────────────────────
    inline Mesh CreateCylinder(int segments = 32)
    {
        std::vector<float> verts;
        std::vector<unsigned int> indices;

        // Top cap
        verts.insert(verts.end(), {0,0.5f,0, 0,1,0, 0.5f,0.5f});
        for (int i = 0; i <= segments; i++) {
            float a = 2*M_PI*i/segments;
            float x = cos(a)*0.5f, z = sin(a)*0.5f;
            float u = 0.5f + 0.5f*cos(a), v = 0.5f + 0.5f*sin(a);
            verts.insert(verts.end(), {x,0.5f,z, 0,1,0, u,v});
        }
        for (int i = 1; i <= segments; i++)
            indices.insert(indices.end(), {0,(unsigned)(i),(unsigned)(i+1)});

        // Bottom cap
        int base = (int)verts.size()/8;
        verts.insert(verts.end(), {0,-0.5f,0, 0,-1,0, 0.5f,0.5f});
        for (int i = 0; i <= segments; i++) {
            float a = 2*M_PI*i/segments;
            float x = cos(a)*0.5f, z = sin(a)*0.5f;
            float u = 0.5f + 0.5f*cos(a), v = 0.5f + 0.5f*sin(a);
            verts.insert(verts.end(), {x,-0.5f,z, 0,-1,0, u,v});
        }
        for (int i = 1; i <= segments; i++)
            indices.insert(indices.end(), {(unsigned)base,(unsigned)(base+i+1),(unsigned)(base+i)});

        // Side
        int sideBase = (int)verts.size()/8;
        for (int i = 0; i <= segments; i++) {
            float a = 2*M_PI*i/segments;
            float x = cos(a)*0.5f, z = sin(a)*0.5f;
            float nx = cos(a), nz = sin(a);
            float u = (float)i/segments;
            verts.insert(verts.end(), {x, 0.5f, z, nx,0,nz, u,1});
            verts.insert(verts.end(), {x,-0.5f, z, nx,0,nz, u,0});
        }
        for (int i = 0; i < segments; i++) {
            int t = sideBase + i*2;
            indices.insert(indices.end(), {
                (unsigned)t,(unsigned)(t+1),(unsigned)(t+2),
                (unsigned)(t+1),(unsigned)(t+3),(unsigned)(t+2)
            });
        }

        Mesh m;
        SetupMeshVAO(m, verts, indices);
        return m;
    }

    // ─────────────────────────────────────────────────────
    //  Пирамида
    // ─────────────────────────────────────────────────────
    inline Mesh CreatePyramid()
    {
        // pos(3) + normal(3) + uv(2)
        std::vector<float> verts = {
            // Дно
            -0.5f,0,-0.5f, 0,-1,0, 0,0,
             0.5f,0,-0.5f, 0,-1,0, 1,0,
             0.5f,0, 0.5f, 0,-1,0, 1,1,
            -0.5f,0, 0.5f, 0,-1,0, 0,1,
            // Грань +Z
            -0.5f,0, 0.5f, 0,0.7f,0.7f, 0,0,
             0.5f,0, 0.5f, 0,0.7f,0.7f, 1,0,
             0.0f,1, 0.0f, 0,0.7f,0.7f, 0.5f,1,
            // Грань -Z
             0.5f,0,-0.5f, 0,0.7f,-0.7f, 0,0,
            -0.5f,0,-0.5f, 0,0.7f,-0.7f, 1,0,
             0.0f,1, 0.0f, 0,0.7f,-0.7f, 0.5f,1,
            // Грань +X
             0.5f,0, 0.5f, 0.7f,0.7f,0, 0,0,
             0.5f,0,-0.5f, 0.7f,0.7f,0, 1,0,
             0.0f,1, 0.0f, 0.7f,0.7f,0, 0.5f,1,
            // Грань -X
            -0.5f,0,-0.5f, -0.7f,0.7f,0, 0,0,
            -0.5f,0, 0.5f, -0.7f,0.7f,0, 1,0,
             0.0f,1, 0.0f, -0.7f,0.7f,0, 0.5f,1,
        };
        std::vector<unsigned int> indices = {
            0,1,2, 2,3,0,
            4,5,6, 7,8,9,
            10,11,12, 13,14,15,
        };

        Mesh m;
        SetupMeshVAO(m, verts, indices);
        return m;
    }

    // ─────────────────────────────────────────────────────
    //  Капсула
    // ─────────────────────────────────────────────────────
    inline Mesh CreateCapsule(float radius = 0.5f, float height = 1.0f, int segments = 32, int rings = 8)
    {
        std::vector<float> verts;
        std::vector<unsigned int> indices;

        float halfH = height * 0.5f;

        // Верхняя полусфера
        for (int i = 0; i <= rings; i++) {
            float phi = (M_PI * 0.5f) * i / rings;
            float y   = radius * sin(phi) + halfH;
            float r   = radius * cos(phi);
            for (int j = 0; j <= segments; j++) {
                float theta = 2.0f * M_PI * j / segments;
                float x = r * cos(theta), z = r * sin(theta);
                float nx = cos(phi)*cos(theta), ny = sin(phi), nz = cos(phi)*sin(theta);
                float u = (float)j/segments, v = 0.5f + 0.5f*(float)i/rings;
                verts.insert(verts.end(), {x,y,z, nx,ny,nz, u,v});
            }
        }

        // Нижняя полусфера
        for (int i = 0; i <= rings; i++) {
            float phi = (M_PI * 0.5f) * i / rings;
            float y   = -radius * sin(phi) - halfH;
            float r   = radius * cos(phi);
            for (int j = 0; j <= segments; j++) {
                float theta = 2.0f * M_PI * j / segments;
                float x = r * cos(theta), z = r * sin(theta);
                float nx = cos(phi)*cos(theta), ny = -sin(phi), nz = cos(phi)*sin(theta);
                float u = (float)j/segments, v = 0.5f - 0.5f*(float)i/rings;
                verts.insert(verts.end(), {x,y,z, nx,ny,nz, u,v});
            }
        }

        // Цилиндр между полусферами
        int cylBase = (int)verts.size() / 8;
        for (int i = 0; i <= 1; i++) {
            float y = (i == 0) ? halfH : -halfH;
            for (int j = 0; j <= segments; j++) {
                float theta = 2.0f * M_PI * j / segments;
                float x = radius * cos(theta), z = radius * sin(theta);
                float nx2 = cos(theta), nz2 = sin(theta);
                float u = (float)j/segments, v = (float)(1-i);
                verts.insert(verts.end(), {x,y,z, nx2,0,nz2, u,v});
            }
        }

        // Индексы верхней полусферы
        for (int i = 0; i < rings; i++)
            for (int j = 0; j < segments; j++) {
                int a = i*(segments+1)+j, b = a+segments+1;
                indices.insert(indices.end(), {(unsigned)a,(unsigned)b,(unsigned)(a+1),(unsigned)b,(unsigned)(b+1),(unsigned)(a+1)});
            }

        // Нижняя
        int lowerBase = (rings+1)*(segments+1);
        for (int i = 0; i < rings; i++)
            for (int j = 0; j < segments; j++) {
                int a = lowerBase+i*(segments+1)+j, b = a+segments+1;
                indices.insert(indices.end(), {(unsigned)a,(unsigned)(a+1),(unsigned)b,(unsigned)b,(unsigned)(a+1),(unsigned)(b+1)});
            }

        // Цилиндр
        for (int j = 0; j < segments; j++) {
            int t = cylBase+j, t2 = cylBase+segments+1+j;
            indices.insert(indices.end(), {(unsigned)t,(unsigned)(t+1),(unsigned)t2,(unsigned)(t+1),(unsigned)(t2+1),(unsigned)t2});
        }

        Mesh m;
        SetupMeshVAO(m, verts, indices);
        return m;
    }

    // ─────────────────────────────────────────────────────
    //  Плоскость
    // ─────────────────────────────────────────────────────
    inline Mesh CreatePlane(int subdivisions = 1)
    {
        std::vector<float> verts;
        std::vector<unsigned int> indices;
        int n = subdivisions + 1;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                float x = (float)j/subdivisions - 0.5f;
                float z = (float)i/subdivisions - 0.5f;
                float u = (float)j/subdivisions;
                float v = (float)i/subdivisions;
                verts.insert(verts.end(), {x, 0.f, z, 0.f, 1.f, 0.f, u, v});
            }
        }
        for (int i = 0; i < subdivisions; i++) {
            for (int j = 0; j < subdivisions; j++) {
                int a = i*n+j, b = a+1, c = a+n, d = c+1;
                indices.insert(indices.end(), {(unsigned)a,(unsigned)c,(unsigned)b,(unsigned)b,(unsigned)c,(unsigned)d});
            }
        }
        Mesh m;
        SetupMeshVAO(m, verts, indices);
        return m;
    }

} // namespace VE