#pragma once
// =========================================================
//  ParticleSystem.h  —  простые CPU-частицы для VisualEngine
//
//  Рисуется существующим cubeVAO (масштабированный маленький кубик
//  на каждую частицу) через gizmoShader с включённым альфа-блендингом —
//  без новых шейдеров/текстур/ассетов.
//
//  Положи файл: src/Core/ParticleSystem.h
//
//  C++ API:
//    VE::ParticleSystem::Get().Spawn(pos, vel, colorStart, colorEnd, life, size, gravity);
//    VE::ParticleSystem::Get().Update(deltaTime);      -- каждый кадр, пока isPlaying
//    VE::ParticleSystem::Get().Render(gizmoShader.ID, vp, cubeVAO); -- из renderScene()
//
//  Lua API (регистрируется один раз в LuaEngine::registerFunctions() через LuaBindings.h):
//    Particles.Spawn(x, y, z, count?, speed?, life?, size?,
//                     r?, g?, b?, a?, r2?, g2?, b2?, a2?, gravity?)
//    Particles.Clear()
// =========================================================

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <cmath>

extern "C" {
    #include "../../external/Lua/include/lua.h"
    #include "../../external/Lua/include/lualib.h"
    #include "../../external/Lua/include/lauxlib.h"
}

namespace VE {

    struct Particle
    {
        glm::vec3 Position{0,0,0};
        glm::vec3 Velocity{0,0,0};
        glm::vec4 ColorStart{1,1,1,1};
        glm::vec4 ColorEnd{1,1,1,0};
        float     Life    = 1.f;   // оставшееся время жизни, сек
        float     MaxLife = 1.f;   // изначальное время жизни, сек (для интерполяции цвета/альфы)
        float     Size    = 0.2f;  // сторона кубика
        float     Gravity = 0.f;   // м/с^2 вниз (0 = не падает)
    };

    // =========================================================
    //  ParticleSystem — синглтон
    // =========================================================
    class ParticleSystem
    {
    public:
        static ParticleSystem& Get()
        {
            static ParticleSystem instance;
            return instance;
        }
        ParticleSystem(const ParticleSystem&)            = delete;
        ParticleSystem& operator=(const ParticleSystem&) = delete;

        // ── C++ API ───────────────────────────────────────────
        void Spawn(const glm::vec3& pos, const glm::vec3& vel,
                   const glm::vec4& colorStart, const glm::vec4& colorEnd,
                   float life = 1.f, float size = 0.2f, float gravity = 0.f)
        {
            if (m_Particles.size() >= MAX_PARTICLES) return; // переполнен — пропускаем
            Particle p;
            p.Position = pos; p.Velocity = vel;
            p.ColorStart = colorStart; p.ColorEnd = colorEnd;
            p.Life = life; p.MaxLife = (life > 0.001f) ? life : 0.001f;
            p.Size = size; p.Gravity = gravity;
            m_Particles.push_back(p);
        }

        // Выпустить "взрыв" частиц в случайных направлениях — под Particles.Spawn(...) из Lua
        void SpawnBurst(const glm::vec3& pos, int count, float speed,
                         float life, float size,
                         const glm::vec4& colorStart, const glm::vec4& colorEnd, float gravity)
        {
            for (int i = 0; i < count; i++)
            {
                // Случайное направление на сфере
                float theta = ((float)std::rand() / RAND_MAX) * 6.2831853f;
                float phi   = ((float)std::rand() / RAND_MAX) * 3.14159265f;
                glm::vec3 dir(sinf(phi)*cosf(theta), cosf(phi), sinf(phi)*sinf(theta));
                float s = speed * (0.5f + 0.5f * ((float)std::rand() / RAND_MAX));
                Spawn(pos, dir * s, colorStart, colorEnd, life, size, gravity);
            }
        }

        void Update(float dt)
        {
            for (auto& p : m_Particles)
            {
                p.Velocity.y -= p.Gravity * dt;
                p.Position   += p.Velocity * dt;
                p.Life       -= dt;
            }
            // Удаляем умершие (swap-and-pop, порядок частиц не важен)
            for (size_t i = 0; i < m_Particles.size(); )
            {
                if (m_Particles[i].Life <= 0.f) {
                    m_Particles[i] = m_Particles.back();
                    m_Particles.pop_back();
                } else {
                    i++;
                }
            }
        }

        void Clear() { m_Particles.clear(); }
        size_t Count() const { return m_Particles.size(); }

        // ── Рендер — вызывать из renderScene() (SceneRenderer.h), только в Game View ──
        // cubeVAO — уже существующий VAO кубика движка (36 индексов, EBO)
        void Render(unsigned int gizmoShaderID, const glm::mat4& vp, unsigned int cubeVAO)
        {
            if (m_Particles.empty()) return;

            glUseProgram(gizmoShaderID);
            GLint mvpLoc   = glGetUniformLocation(gizmoShaderID, "mvp");
            GLint colorLoc = glGetUniformLocation(gizmoShaderID, "color");

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); // частицы не пишут в depth buffer — не перекрывают друг друга жёстко

            glBindVertexArray(cubeVAO);
            for (auto& p : m_Particles)
            {
                float t = 1.f - (p.Life / p.MaxLife); // 0 в начале жизни -> 1 в конце
                glm::vec4 col = glm::mix(p.ColorStart, p.ColorEnd, t);

                glm::mat4 model = glm::translate(glm::mat4(1.f), p.Position);
                model = glm::scale(model, glm::vec3(p.Size));
                glm::mat4 mvp = vp * model;

                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniform4f(colorLoc, col.r, col.g, col.b, col.a);
                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ── Lua биндинги ──────────────────────────────────────
        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            // Particles.Spawn(x, y, z, count?, speed?, life?, size?,
            //                  r?, g?, b?, a?,  r2?, g2?, b2?, a2?, gravity?)
            lua_pushstring(L, "Spawn");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                glm::vec3 pos((float)luaL_optnumber(LS,1,0), (float)luaL_optnumber(LS,2,0), (float)luaL_optnumber(LS,3,0));
                int   count  = (int)luaL_optinteger(LS,4,16);
                float speed  = (float)luaL_optnumber(LS,5,2.0);
                float life   = (float)luaL_optnumber(LS,6,1.0);
                float size   = (float)luaL_optnumber(LS,7,0.15);
                glm::vec4 colStart(
                    (float)luaL_optnumber(LS,8, 1.0), (float)luaL_optnumber(LS,9, 0.7),
                    (float)luaL_optnumber(LS,10,0.2), (float)luaL_optnumber(LS,11,1.0));
                glm::vec4 colEnd(
                    (float)luaL_optnumber(LS,12,colStart.r), (float)luaL_optnumber(LS,13,colStart.g),
                    (float)luaL_optnumber(LS,14,colStart.b), (float)luaL_optnumber(LS,15,0.0));
                float gravity = (float)luaL_optnumber(LS,16,0.0);

                count = std::max(1, std::min(count, 500)); // защита от Particles.Spawn(x,y,z,999999)
                ParticleSystem::Get().SpawnBurst(pos, count, speed, life, size, colStart, colEnd, gravity);
                return 0;
            }, 0);
            lua_settable(L, -3);

            // Particles.Clear()
            lua_pushstring(L, "Clear");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                ParticleSystem::Get().Clear(); return 0;
            }, 0);
            lua_settable(L, -3);

            // Particles.GetCount() -> number
            lua_pushstring(L, "GetCount");
            lua_pushcclosure(L, [](lua_State* LS) -> int {
                lua_pushinteger(LS, (lua_Integer)ParticleSystem::Get().Count());
                return 1;
            }, 0);
            lua_settable(L, -3);

            lua_setglobal(L, "Particles");
        }

    private:
        ParticleSystem() = default;

        static constexpr size_t MAX_PARTICLES = 4000;
        std::vector<Particle> m_Particles;
    };

} // namespace VE
