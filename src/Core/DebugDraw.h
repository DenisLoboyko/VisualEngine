#pragma once
// =========================================================
//  DebugDraw.h  —  Debug-рисование для VisualEngine (как Debug.DrawLine в Unity)
//
//  Собирает очередь примитивов (линии/сферы/боксы) за кадр,
//  рисует их поверх Game View существующим gizmoShader'ом
//  (тем же, которым рисуются гизмо-стрелки редактора).
//
//  Положи файл: src/Core/DebugDraw.h
//
//  C++ API:
//    VE::DebugDraw::Get().Line(a, b, color, duration);
//    VE::DebugDraw::Get().Render(gizmoShader.ID, vp, (float)glfwGetTime());
//    -- вызывать из renderScene() в SceneRenderer.h (там уже есть glad/glm) --
//
//  Lua API (регистрируется один раз в LuaEngine::registerFunctions() через LuaBindings.h):
//    Debug.DrawLine(x1,y1,z1, x2,y2,z2, r?,g?,b?,a?, duration?)
//    Debug.DrawSphere(x,y,z, radius, r?,g?,b?,a?, duration?)
//    Debug.DrawBox(x,y,z, hx,hy,hz, r?,g?,b?,a?, duration?)
//    Debug.Clear()
// =========================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

namespace VE {

    struct DebugPrimitive
    {
        enum class Type { Line, Sphere, Box } type;
        glm::vec3 a{0,0,0};       // Line: точка A          Sphere/Box: центр
        glm::vec3 b{0,0,0};       // Line: точка B          Box: полуразмер (halfSize)
        float     radius = 0.5f;  // Sphere
        glm::vec4 color{1,1,1,1};
        float     expireTime = 0.f; // glfwGetTime() когда примитив нужно убрать (0 = один кадр)
    };

    // =========================================================
    //  DebugDraw — синглтон
    // =========================================================
    class DebugDraw
    {
    public:
        static DebugDraw& Get()
        {
            static DebugDraw instance;
            return instance;
        }
        DebugDraw(const DebugDraw&)            = delete;
        DebugDraw& operator=(const DebugDraw&) = delete;

        // ── C++ API ───────────────────────────────────────────
        void Line(const glm::vec3& p1, const glm::vec3& p2,
                  const glm::vec4& color = {0,1,0,1}, float duration = 0.f, float now = 0.f)
        {
            DebugPrimitive p; p.type = DebugPrimitive::Type::Line;
            p.a = p1; p.b = p2; p.color = color; p.expireTime = now + duration;
            Push(p);
        }

        void Sphere(const glm::vec3& center, float radius,
                    const glm::vec4& color = {0,1,0,1}, float duration = 0.f, float now = 0.f)
        {
            DebugPrimitive p; p.type = DebugPrimitive::Type::Sphere;
            p.a = center; p.radius = radius; p.color = color; p.expireTime = now + duration;
            Push(p);
        }

        void Box(const glm::vec3& center, const glm::vec3& halfSize,
                 const glm::vec4& color = {0,1,0,1}, float duration = 0.f, float now = 0.f)
        {
            DebugPrimitive p; p.type = DebugPrimitive::Type::Box;
            p.a = center; p.b = halfSize; p.color = color; p.expireTime = now + duration;
            Push(p);
        }

        void Clear() { m_Queue.clear(); }

        // ── Рендер — вызывать из renderScene() (SceneRenderer.h), только в Game View ──
        // gizmoShaderID — тот же шейдер что рисует стрелки гизмо (mvp + color uniform)
        // now           — текущее время (glfwGetTime()), нужно для очистки истёкших примитивов
        void Render(unsigned int gizmoShaderID, const glm::mat4& vp, float now)
        {
            if (m_Queue.empty()) return;

            glUseProgram(gizmoShaderID);
            GLint mvpLoc   = glGetUniformLocation(gizmoShaderID, "mvp");
            GLint colorLoc = glGetUniformLocation(gizmoShaderID, "color");
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(vp));

            glLineWidth(1.5f);

            for (auto& prim : m_Queue)
            {
                std::vector<float> verts;
                switch (prim.type)
                {
                    case DebugPrimitive::Type::Line:
                        AppendLine(verts, prim.a, prim.b);
                        break;
                    case DebugPrimitive::Type::Sphere:
                        AppendSphere(verts, prim.a, prim.radius);
                        break;
                    case DebugPrimitive::Type::Box:
                        AppendBox(verts, prim.a, prim.b);
                        break;
                }
                if (verts.empty()) continue;

                unsigned int vao, vbo;
                glGenVertexArrays(1, &vao);
                glGenBuffers(1, &vbo);
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);

                glUniform4f(colorLoc, prim.color.r, prim.color.g, prim.color.b, prim.color.a);
                glDrawArrays(GL_LINES, 0, (GLsizei)(verts.size() / 3));

                glDeleteVertexArrays(1, &vao);
                glDeleteBuffers(1, &vbo);
            }

            // Убираем примитивы этого кадра (duration=0) и истёкшие по времени
            m_Queue.erase(
                std::remove_if(m_Queue.begin(), m_Queue.end(),
                    [now](const DebugPrimitive& p){ return p.expireTime <= now; }),
                m_Queue.end());
        }

        // ── Lua биндинги ──────────────────────────────────────
        // getNow — функция, возвращающая текущее время (обычно glfwGetTime через лямбду)
        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // Debug.DrawLine(x1,y1,z1, x2,y2,z2, r?,g?,b?,a?, duration?)
            lua_pushstring(L, "DrawLine");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                glm::vec3 p1((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
                glm::vec3 p2((float)luaL_optnumber(LS,4,0), (float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,0));
                glm::vec4 col((float)luaL_optnumber(LS,7,0), (float)luaL_optnumber(LS,8,1), (float)luaL_optnumber(LS,9,0), (float)luaL_optnumber(LS,10,1));
                float duration = (float)luaL_optnumber(LS,11,0.0);
                DebugDraw::Get().Line(p1, p2, col, duration, (float)glfwGetTime());
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Debug.DrawSphere(x,y,z, radius, r?,g?,b?,a?, duration?)
            lua_pushstring(L, "DrawSphere");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                glm::vec3 c((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
                float radius = (float)luaL_optnumber(LS,4,0.5);
                glm::vec4 col((float)luaL_optnumber(LS,5,0), (float)luaL_optnumber(LS,6,1), (float)luaL_optnumber(LS,7,0), (float)luaL_optnumber(LS,8,1));
                float duration = (float)luaL_optnumber(LS,9,0.0);
                DebugDraw::Get().Sphere(c, radius, col, duration, (float)glfwGetTime());
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Debug.DrawBox(x,y,z, hx,hy,hz, r?,g?,b?,a?, duration?)
            lua_pushstring(L, "DrawBox");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                glm::vec3 c((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
                glm::vec3 h((float)luaL_optnumber(LS,4,0.5), (float)luaL_optnumber(LS,5,0.5), (float)luaL_optnumber(LS,6,0.5));
                glm::vec4 col((float)luaL_optnumber(LS,7,0), (float)luaL_optnumber(LS,8,1), (float)luaL_optnumber(LS,9,0), (float)luaL_optnumber(LS,10,1));
                float duration = (float)luaL_optnumber(LS,11,0.0);
                DebugDraw::Get().Box(c, h, col, duration, (float)glfwGetTime());
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Debug.Clear()
            lua_pushstring(L, "Clear");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                DebugDraw::Get().Clear(); return 0;
            }, 0);
            lua_settable(L, -3);

            lua_setglobal(L, "Debug");
        }

    private:
        DebugDraw() = default;

        void Push(const DebugPrimitive& p)
        {
            // Защита от бесконтрольного роста очереди, если кто-то спамит Debug.* без duration
            if (m_Queue.size() > 20000) return;
            m_Queue.push_back(p);
        }

        static void AppendLine(std::vector<float>& v, const glm::vec3& a, const glm::vec3& b)
        {
            v.insert(v.end(), { a.x,a.y,a.z, b.x,b.y,b.z });
        }

        static void AppendBox(std::vector<float>& v, const glm::vec3& c, const glm::vec3& h)
        {
            glm::vec3 p[8] = {
                c + glm::vec3(-h.x,-h.y,-h.z), c + glm::vec3( h.x,-h.y,-h.z),
                c + glm::vec3( h.x, h.y,-h.z), c + glm::vec3(-h.x, h.y,-h.z),
                c + glm::vec3(-h.x,-h.y, h.z), c + glm::vec3( h.x,-h.y, h.z),
                c + glm::vec3( h.x, h.y, h.z), c + glm::vec3(-h.x, h.y, h.z),
            };
            int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0}, // задняя грань
                {4,5},{5,6},{6,7},{7,4}, // передняя грань
                {0,4},{1,5},{2,6},{3,7}  // рёбра между гранями
            };
            for (auto& e : edges) AppendLine(v, p[e[0]], p[e[1]]);
        }

        static void AppendSphere(std::vector<float>& v, const glm::vec3& c, float r)
        {
            const int SEG = 24;
            // Три окружности по осям X/Y/Z (как wire-sphere в Unity)
            for (int axis = 0; axis < 3; axis++)
            {
                for (int i = 0; i < SEG; i++)
                {
                    float a0 = 2.f * 3.14159265f * i     / SEG;
                    float a1 = 2.f * 3.14159265f * (i+1) / SEG;
                    glm::vec3 p0, p1;
                    if (axis == 0)      { p0 = {0, cosf(a0)*r, sinf(a0)*r}; p1 = {0, cosf(a1)*r, sinf(a1)*r}; }
                    else if (axis == 1) { p0 = {cosf(a0)*r, 0, sinf(a0)*r}; p1 = {cosf(a1)*r, 0, sinf(a1)*r}; }
                    else                { p0 = {cosf(a0)*r, sinf(a0)*r, 0}; p1 = {cosf(a1)*r, sinf(a1)*r, 0}; }
                    AppendLine(v, c + p0, c + p1);
                }
            }
        }

        std::vector<DebugPrimitive> m_Queue;
    };

} // namespace VE
